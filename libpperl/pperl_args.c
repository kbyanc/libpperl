/*
 * Copyright (c) 2004,2005 NTT Multimedia Communications Laboratories, Inc.
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
 */

#include "pperl_platform.h"
#include <sys/types.h>

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#define	HAS_BOOL	/* We use stdbool's bool type rather than perl's. */
#include <EXTERN.h>
#include <perl.h>

#include "queue.h"

#include "pperl.h"
#include "pperl_private.h"


#define	ROUNDUP(x, n)	(((x) + (n) - 1) & ~((n) - 1))


/*
 * pperl_args_new() - Initialize an argument list.
 *
 *	Creates a new argument list, initializing it with the contents of the
 *	given array.
 *
 *	@param	interp		Interpreter to create \@ARGV in.  Currently,
 *				this is not necessary but may be in the future
 *				if the implementation changes.
 *
 *	@param	tainted		Whether or not perl code should consider the
 *				arguments in this list "tainted" (possibly
 *				untrustworthy).  This is global as it applies
 *				to all elements in the argument list.
 *
 *	@param	argc		Number of strings in \a argv array to
 *				initialize argument list with.  If -1, then
 *				all elements in \a argv will be added up to
 *				the first NULL entry.  If 0, no arguments will
 *				be added to argument list and \a argv may be
 *				NULL.
 *
 *	@param	argv		Array of strings to initialize the argument
 *				list from.  If \a argc is -1, the array must
 *				be terminated with a NULL entry.
 *
 *	@return	New argument list.
 */
perlargs_t
pperl_args_new(perlinterp_t interp, bool tainted, int argc, const char **argv)
{
	perlargs_t pargs;
	const char **pos;

	assert(argc == 0 || argv != NULL);

	if (argc == -1 /* XXXCONST */) {
		argc = 0;
		for (pos = argv; *pos != NULL; pos++)
			argc++;
	}

	assert(argc >= 0);

	pargs = pperl_malloc(sizeof(*pargs));
	pargs->pa_interp = interp;
	pargs->pa_tainted = tainted;

	pargs->pa_argc = 0;
	pargs->pa_arglenv_size = ROUNDUP(argc, 4);
	if (pargs->pa_arglenv_size == 0)
		pargs->pa_arglenv_size = 4;

	pargs->pa_arglenv = pperl_malloc(pargs->pa_arglenv_size *
					 sizeof(size_t));

	pargs->pa_strbuf_len = 0;
	pargs->pa_strbuf_size = ROUNDUP(argc * 20, 32);
	if (pargs->pa_strbuf_size == 0)
		pargs->pa_strbuf_size = 32;

	pargs->pa_strbuf = pperl_malloc(pargs->pa_strbuf_size);

	for (; argc > 0; argc--, argv++)
		pperl_args_append(pargs, *argv);

	LIST_INSERT_HEAD(&interp->pi_args_head, pargs, pa_link);

	return (pargs);
}


/*
 * pperl_args_destroy() - Free all memory allocated to an argument list.
 *
 *	@param	pargsp		Pointer to argument list to free.
 *
 *	@post	*pargsp is set to NULL.
 *
 */
void
pperl_args_destroy(perlargs_t *pargsp)
{
	perlargs_t pargs = *pargsp;

	*pargsp = NULL;
	LIST_REMOVE(pargs, pa_link);
	free(pargs->pa_strbuf);
	free(pargs->pa_arglenv);
	free(pargs);
}


/*
 * pperl_args_append() - Append a string to the given argument list.
 *
 *	@param	pargs		Argument list to extend.
 *
 *	@param	arg		String to append to list.
 */
void
pperl_args_append(perlargs_t pargs, const char *arg)
{
	char *pos;
	size_t len;

	assert(arg != NULL);

	/*
	 * If the argument vector is full, enlarge it to make room for the
	 * entry.  The vector is enlarged in multiples of 4 entries to reduce
	 * the number of calls to realloc(3) necessary.  However, the usual
	 * doubling algorithm is not used as it is expected that most argument
	 * lists will be short.
	 */
	if (pargs->pa_argc == pargs->pa_arglenv_size) {
		pargs->pa_arglenv_size += 4;
		pargs->pa_arglenv = pperl_realloc(pargs->pa_arglenv,
				pargs->pa_arglenv_size * sizeof(size_t));
	}

	len = strlen(arg);	/* Note we don't include trailing nul. */

	/*
	 * If there isn't room in the string buffer for the new string, enlarge
	 * it.  Note that in this case, the size doubling algorithm *is* used
	 * to limit calls to realloc(3) to O(lg n) because we have no idea how
	 * long the argument strings may be.
	 */
	if (pargs->pa_strbuf_len + len > pargs->pa_strbuf_size) {
		do {
			pargs->pa_strbuf_size *= 2;
		} while (pargs->pa_strbuf_len + len > pargs->pa_strbuf_size);
		pargs->pa_strbuf = pperl_realloc(pargs->pa_strbuf,
						 pargs->pa_strbuf_size);
	}

	/*
	 * Append new argument string to string buffer.
	 */
	pos = pargs->pa_strbuf + pargs->pa_strbuf_len;
	memcpy(pos, arg, len);
	pargs->pa_strbuf_len += len;

	pargs->pa_arglenv[pargs->pa_argc] = len;
	pargs->pa_argc++;
}


/*!
 * pperl_args_append_printf() - Append a printf-formated string to the given
 *				argument list.
 *
 *	Wrapper around pperl_args_append() which constructs the string to
 *	append to the argument list from a printf-like format and argument
 *	list.
 */
void
pperl_args_append_printf(perlargs_t pargs, const char *fmt, ...)
{
	char *str;
	va_list ap;

	va_start(ap, fmt);
	if (vasprintf(&str, fmt, ap) < 0)
		pperl_fatal(EX_OSERR, "vasprintf: %m");
	va_end(ap);

	pperl_args_append(pargs, str);
	free(str);
}


/*!
 * pperl_args_populate() - Populate \@ARGV array from argument list.
 *
 *	Replaces the contents of \@ARGV array in the current interpreter
 *	with the strings in the specified argument list.
 *
 *	@param	pargs		Argument list to populate \@ARGV from.
 *				If NULL, \@ARGV is set to an empty array.
 */
void
pperl_args_populate(perlargs_t pargs)
{
	AV *perlargv;
	SV *arg_sv;
	const char *pos;
	size_t *lenp;
	size_t len;
	int i;
	int orig_tainting;

	/*
	 * Clear any existing elements of the @ARGV array and ensure that the
	 * array itself has not accumulated any magic.  If the caller supplied
	 * no arguments, then we'll leave the array empty.
	 */
	perlargv = get_av("ARGV", TRUE);
	av_clear(perlargv);
	mg_free((SV *)perlargv);

	if (pargs == NULL)
		return;

	assert(pargs->pa_interp->pi_perl == PERL_GET_CONTEXT);

	/*
	 * Propogate argument's tainted flag to perl.  This lets the caller
	 * tell perl whether it can trust the contents or not.  Technically,
	 * I don't think PL_tainting is a public API, but toggling it directly
	 * appears the only way to implement this functionality.
	 */
	orig_tainting = PL_tainting;
	PL_tainting = pargs->pa_tainted;

	/*
	 * Pre-size the @ARGV array to the appropriate size.  Note that the
	 * second argument to av_extend() is the last index and indices start
	 * counting from zero, hence we have to subtract one from pa_argc.
	 */
	av_extend(perlargv, pargs->pa_argc - 1);

	/*
	 * Iterate through the argument list, adding them to perl's @ARGV
	 * array.  The 'pos' variable iterates through the pa_strbuf, pointing
	 * to each argument string while the 'lenp' variable iterates through
	 * the pa_arglenv array pointing to the length of each argument.  The
	 * length is used to determine how many bytes to increment 'pos' to
	 * get to the beginning of the next argument string.
	 */
	pos = pargs->pa_strbuf;
	lenp = pargs->pa_arglenv;
	for (i = 0; i < pargs->pa_argc; i++) {

		len = *lenp;

		arg_sv = newSVpvn(pos, len);
		av_store(perlargv, i, arg_sv);

		pos += len;
		lenp++;
	}

	/* Restore original tainting state. */
	PL_tainting = orig_tainting;
}
