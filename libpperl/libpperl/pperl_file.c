/*
 * Copyright (c) 2005 NTT Multimedia Communications Laboratories, Inc.
 * All rights reserved
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $NTTMCL$
 */

#include "pperl_platform.h"
#include <sys/types.h>
#include <sys/mman.h>

#include <assert.h>
#include <fcntl.h>
#include <poll.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <syslog.h>

#define	HAS_BOOL	/* We use stdbool's bool type rather than perl's. */
#include <EXTERN.h>
#include <XSUB.h>
#include <perl.h>

#include "queue.h"

#include "pperl.h"
#include "pperl_private.h"


/* Round up to the next multiple of 'n' where 'n' is a power of two. */
#define	ROUNDUP(x, n)	(((x) + (n) - 1) & ~((n) - 1))


static perlcode_t	 pperl_load_fd_mmap(perlinterp_t interp,
					    const char *name, perlenv_t penv,
					    int fd, size_t size,
					    struct perlresult *result);
static perlcode_t	 pperl_load_fd_read(perlinterp_t interp,
					    const char *name, perlenv_t penv,
					    int fd, size_t size,
					    struct perlresult *result);


/*!
 * pperl_load_file() - Helper routine to load perl code from a file into
 *		       an interpreter for later execution.
 *
 *	This routine is a wrapper for pperl_load() which handles the details
 *	of reading the perl code from a file on disk.  No attempt is made to
 *	automatically re-load the file should its on-disk contents change.
 *
 *	@param	interp		Perl interpreter to load the code into;
 *				the code will always be executed in this
 *				interpreter.
 *
 *	@param	path		Path to file to load the code from.
 *
 *	@param	penv		Environment variable list to populate \%ENV
 *				with while loading code.  This is primarilly
 *				for the benefit of any BEGIN, CHECK, or INIT
 *				code blocks that may run during load.
 *
 *	@param	result		If non-NULL, populated with the result returned
 *				by any perl BEGIN, CHECK, or INIT code blocks
 *				executed during load.  If an error occurs
 *				reading the code from the specified file, the
 *				pperl_errno member is set to indicate the
 *				cause of the error.
 *
 *	@return	Handle for refering to the loaded code if successful.  If an
 *		error occurred during load, returns NULL; if \a result was
 *		non-NULL, it is populated with the cause of the failure.
 *
 *	@see pperl_load(), pperl_load_fd()
 */
perlcode_t
pperl_load_file(perlinterp_t interp, const char *path, perlenv_t penv,
		struct perlresult *result)
{
	const char *scriptname;
	perlcode_t pc;
	int fd;

	/*
	 * Extract the last component of the path as the script name.
	 */
	scriptname = strrchr(path, '/');
	if (scriptname == NULL)
		scriptname = path;
	else
		scriptname++;

	/*
	 * Open the given filepath read-only.  Use a shared lock to discourage
	 * well-behaved programs from modifying the file contents while we
	 * read them (e.g. vi(1) defaults to exclusively locking files being
	 * edited).
	 */
	fd = open(path, O_RDONLY|O_SHLOCK);
	if (fd < 0) {
		pperl_seterr(errno, result);
		return NULL;
	}

	/*
	 * Now that we have a file descriptor, call pperl_load_fd() to do
	 * the rest.
	 */
	pc = pperl_load_fd(interp, scriptname, penv, fd, result);

	close(fd);

	return pc;
}


/*!
 * pperl_load_fd() - Helper routine to load perl code from a file descriptor
 *		     into an interpreter for later execution.
 *
 *	This routine is a wrapper for pperl_load() which handles the details
 *	of reading the perl code from an open file descriptor (file, socket,
 *	pipe, etc).  The file descriptor must be open for reading.  Returns
 *	only once the given descriptor returns end-of-file for a read or an
 *	error occurs.
 *
 *	@param	interp		Perl interpreter to load the code into;
 *				the code will always be executed in this
 *				interpreter.
 *
 *	@param	name		Text descripting the code being loaded.  See
 *				explanation under pperl_eval().
 *
 *	@param	fd		File descriptor to read code from.
 *
 *	@param	penv		Environment variable list to populate \%ENV
 *				with while loading code.  This is primarilly
 *				for the benefit of any BEGIN, CHECK, or INIT
 *				code blocks that may run during load.
 *
 *	@param	result		If non-NULL, populated with the result returned
 *				by any perl BEGIN, CHECK, or INIT code blocks
 *				executed during load.  If an error occurs
 *				reading the code from the specified descriptor,
 *				the pperl_errno member is set to indicate the
 *				cause of the error.
 *
 *	@return	Handle for refering to the loaded code if successful.  If an
 *		error occurred during load, returns NULL; if \a result was
 *		non-NULL, it is populated with the cause of the failure.
 */
perlcode_t
pperl_load_fd(perlinterp_t interp, const char *name, perlenv_t penv, int fd,
	      struct perlresult *result)
{
	perlcode_t pc;
	struct stat sb;

	/*
	 * First, read the length of the file represented by the descriptor.
	 * For sockets, pipes, etc. this will be zero.  Note that even if the
	 * length is non-zero, mmap(2) may fail to read the file (e.g. some
	 * network filesystems do not support mmap(2)).
	 */
	if (fstat(fd, &sb) < 0) {
		pperl_seterr(errno, result);
		return NULL;
	}

	/*
	 * First, attempt to read the contents via mmap(2).  Not all
	 * descriptors support mmap(2) so if this method fails, fall back to
	 * using a read(2) loop to load the file contents.  Of course, if
	 * read(2) fails also, then something is really wrong and we report
	 * that error.  In the common case, loading code from a file on
	 * disk, mmap(2) will usually work and is faster, hence we try that
	 * first.
	 */
	pc = pperl_load_fd_mmap(interp, name, penv, fd, sb.st_size, result);
	if (pc != NULL)
		return pc;

	pc = pperl_load_fd_read(interp, name, penv, fd, sb.st_size, result);
	return pc;
}


/*!
 * pperl_load_fd_mmap() - Internal implementation of pperl_load_fd() using
 *			  mmap(2) to read file contents.
 *
 *	The parameters and return values are exactly the same as
 *	pperl_load_fd() except for the additional \a size argument which
 *	specifies the number of bytes to read from the file descriptor.
 */
perlcode_t
pperl_load_fd_mmap(perlinterp_t interp, const char *name, perlenv_t penv,
		   int fd, size_t size, struct perlresult *result)
{
	perlcode_t pc;
	char *code;

	/*
	 * Map the text of the script file into memory.
	 * Failing here is not fatal because we'll fall back to
	 */
	code = mmap(NULL, size, PROT_READ, 0, fd, 0);
	if (code == MAP_FAILED) {
		pperl_seterr(errno, result);
		return NULL;
	}

	/*
	 * Load the script into the interpreter.
	 */
	pperl_result_clear(result);
	pc = pperl_load(interp, name, penv, code, size, result);

	/*
	 * We don't need the script contents anymore.
	 */
	munmap(code, size);

	return pc;
}


/*!
 * pperl_load_fd_read() - Internal implementation of pperl_load_fd() using
 *			  read(2) to read file contents.
 *
 *	The parameters are return values are exactly the same as
 *	pperl_load_fd() except for the additional \a size argument which
 *	is only used as an initial estimate of the amount of data available
 *	to read from the descriptor.  This value is used to size the initial
 *	memory allocation for reading the file contents, but the allocation
 *	will be extended as necessary.  Less data may also be read.
 */
perlcode_t
pperl_load_fd_read(perlinterp_t interp, const char *name, perlenv_t penv,
		   int fd, size_t size, struct perlresult *result)
{
	perlcode_t pc;
	char *code;
	size_t codelen;
	ssize_t len;

	/*
	 * Do reads in multiples of the VM's page size since that is most
	 * likely to be optimal.
	 */
	size = ROUNDUP(size, PAGE_SIZE);
	if (size == 0)
		size = PAGE_SIZE;

	code = pperl_malloc(size);
	codelen = 0;

	/*
	 * Loop for populating the code buffer with data read from the given
	 * file descriptor.
	 */
	for (;;) {
		assert(codelen < size);

		len = read(fd, code + codelen, size - codelen);

		if (len == 0)		/* End-of-file. */
			break;

		if (len < 0) {		/* Read error. */
			if (errno == EINTR)
				continue;
			if (errno == EAGAIN) {
				/*
				 * Caller passed us a non-blocking socket (e.g.
				 * one with the O_NONBLOCK flag set); since
				 * read(2) won't block waiting for data, call
				 * poll(2) to wait for the rest of the data.
				 * Note: we don't care about poll(2)'s return
				 *	 value because we'll immediately retry
				 *	 if there is no data ready to read.
				 */
				struct pollfd pfd;
				pfd.fd = fd;
				pfd.events = POLLIN;
				poll(&pfd, 1, INFTIM);

				continue;
			}

			/* All other errors are fatal. */
			pperl_seterr(errno, result);
			free(code);
			return NULL;
		}

		/*
		 * Successfully read some data.
		 * If it filled our allocated buffer, double the size of the
		 * buffer and try to read some more.
		 */
		codelen += len;

		if (codelen == size) {
			size *= 2;
			code = pperl_realloc(code, size);
		}
	}

	/*
	 * Load the script into the interpreter.
	 */
	pperl_result_clear(result);
	pc = pperl_load(interp, name, penv, code, size, result);

	/*
	 * We don't need the script contents anymore.
	 */
	free(code);

	return pc;
}
