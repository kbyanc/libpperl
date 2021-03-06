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

#ifndef _INCLUDE_LIBPPERL_
#define _INCLUDE_LIBPPERL_

#include <sys/types.h>
#include <inttypes.h>		/* For intptr_t */
#include <stdarg.h>

#if !(__GNUC__ == 2 && __GNUC_MINOR__ >= 7 || __GNUC__ >= 3 || defined(__INTEL_COMPILER))
#  ifndef __attribute__
#    define	__attribute__(args)
#  endif
#endif


/*!
 * @file
 *
 * Routines for maintaining one (or more) persistent perl interpreters.
 * See embperl.h for an API to standard non-persistent perl interpreters.
 *
 * A persistent perl interpreter is orders of magnitude faster than a standard
 * perl interpreter; the improvement is obtained by pre-loading perl code,
 * allowing perl to parse and compile the code only once, and then running the
 * compiled form multiple times.
 *
 * This is the same technique as is used by the popular mod_perl package to
 * improve performance of perl CGIs (specifically, the Apache::Registry
 * implementation in mod_perl).  As such, many of the same caveats apply:
 * http://perl.apache.org/docs/general/perl_reference/perl_reference.html
 *
 * As with many (but not all) persistent perl interpreters, perl END blocks are
 * only executed when the code is unloaded rather than once per time the code
 * is run.  Global variables retain their contents across invocations.  Any
 * calls to 'exit' are trapped so that they do not cause the process to
 * actually terminate.  The exit value is instead returned via the pperl_status
 * member of the perlresult structure.
 *
 * This implementation differs from that of most persistent perl interpreters
 * (including mod_perl) in that it *does* invoked perl CHECK and INIT blocks
 * at the appropriate time (when the code is initially loaded).
 */


typedef struct perlinterp *perlinterp_t;
typedef struct perlenv *perlenv_t;
typedef struct perlargs *perlargs_t;
typedef struct perlio *perlio_t;
typedef struct perlcode *perlcode_t;


/*!
 * pperl_log_callback() - Log a message using stdarg(3) argument list.
 *      
 *      Pointer to function called by libpperl to log messages.
 *      The \a priority parameter specifies the severity of the message, the
 *      acceptable values are identical to those defined by syslog(3). 
 *      The \a fmt string is identical to a printf()-style format string
 *      except that '%m' is expected to be replaced by an error message
 *      correponding to the current value of the errno global variable.
 */
extern void (*pperl_log_callback)(int, const char *, va_list);

/*!
 * pperl_fatal_callback() - Recording a critical condition and exit.
 *
 *	Pointer to function called by libpperl whenever a critical condition
 *	occurs that precludes the program from continuing (usually
 *	an out-of-memory condition).  It is always called with the global
 *	errno variable set to the cause of the critical condition.
 *	The \a eval argument is the recommended exit code, as defined in
 *	<sysexits.h>; the remaining arguments are a printf()-style format
 *	string and argument list.  The implemention *must* not return as
 *	the caller has encountered at fatal error.
 */
extern void (*pperl_fatal_callback)(int, const char *, va_list);


/*!
 * Flags used to specify the behavior of an interpreter created by
 * pperl_new().  WARNINGS_* options are mutually-exclusive (that is, only
 * zero or one option from that group of flags should be specified).
 * Similarly, the TAINT_* options are mutually exclusive.  However, the
 * UNICODE_* options may be combined.  Flags are bitwise-OR'ed together.
 * For example, WARNINGS_ENABLE|TAINT_WARN|UNICODE_STDALL|UNICODE_IO_DEFAULT
 * is equivalent to the perl command-line "-wt -CSD".
 */
enum pperl_newflags {
	DEFAULT			= 0x00000000,

	WARNINGS_ENABLE		= 0x00000001,	/*!< -w perl command-line. */
	WARNINGS_FORCE_ALL	= 0x00000002,	/*!< -W perl command-line. */
	WARNINGS_FORCE_NONE	= 0x00000003,	/*!< -X perl command-line. */
	_WARNINGS_MASK		= 0x00000003,

	TAINT_WARN		= 0x00000010,	/*!< -t perl command-line. */
	TAINT_FATAL		= 0x00000020,	/*!< -T perl command-line. */
	_TAINT_MASK		= 0x00000030,

	UNSAFE_ENABLE		= 0x00000040,	/*!< -U perl command-line. */

	ARGLOOP_NOPRINT		= 0x00000100,	/*!< -n perl command-line. */
	ARGLOOP_PRINT		= 0x00000200,	/*!< -p perl command-line. */
	_ARGLOOP_MASK		= 0x00000300,

	UNICODE_STDIN		= 0x00010000,	/*!< -CI perl command-line. */
	UNICODE_STDOUT		= 0x00020000,	/*!< -CO perl command-line. */
	UNICODE_STDERR		= 0x00040000,	/*!< -CE perl command-line. */
	UNICODE_STDALL		= 0x00070000,	/*!< -CS perl command-line. */

	UNICODE_INPUT_DEFAULT	= 0x00100000,	/*!< -Ci perl command-line. */
	UNICODE_OUTPUT_DEFAULT	= 0x00200000,	/*!< -Co perl command-line. */
	UNICODE_IO_DEFAULT	= 0x00300000,	/*!< -CD perl command-line. */

	UNICODE_ARGV		= 0x00400000,	/*!< -CA perl command-line. */
	_UNICODE_MASK		= 0x00770000,
};


/*!
 * @struct perlresult
 *
 * Data structure for representing exit status of executed perl code.
 *
 *	@param	pperl_status	Equivalent to perl's \$? variable.
 *				Set to parameter perl code called exit() with;
 *				otherwise value is zero.
 *
 *	@param	pperl_errno	Equivalent to perl's \$! variable as a numeric
 *				value (which is the same as the C errno value
 *				of the library call that failed).  Zero if
 *				no error occurred.
 *
 *	@param	pperl_result	Equivalent to perl's \$@ variable.
 *				Stringified version of parameter perl code
 *				called die() with; otherwise value is NULL.
 */
struct perlresult {
	int		 pperl_status;
	int		 pperl_errno;
	const char	*pperl_errmsg;
	intptr_t	 pperl_reserved;
};


#ifdef __cplusplus
extern "C" {
#endif

/*!
 * pperl_result_clear() - Clear contents of perlresult structure.
 *
 *	In general, applications never need to call this routine as all APIs
 *	which take a perlresult pointer clear the contents of the structure
 *	before performing any operations which may set it (by calling this
 *	routine in fact).
 *
 *	@param	result		Structure to clear.
 */
static inline
void
pperl_result_clear(struct perlresult *result)
{
	if (result == NULL)
		return;

	result->pperl_status = 0;
	result->pperl_errno = 0;
	result->pperl_errmsg = NULL;
	result->pperl_reserved = 0;
}


extern perlinterp_t	 pperl_new(const char *procname,
				   enum pperl_newflags flags);
extern void		 pperl_destroy(perlinterp_t *interpp);


extern perlenv_t	 pperl_env_new(perlinterp_t interp, bool tainted,
				       int envc, const char **envp);
extern void		 pperl_env_set(perlenv_t penv, const char *name,
				       const char *value);
extern void		 pperl_env_setf(perlenv_t penv, const char *name,
					const char *fmt, ...);
extern void		 pperl_env_setv(perlenv_t penv, const char *name,
					const char *fmt, va_list ap);
extern const char	*pperl_env_get(const perlenv_t penv,
				       const char *name);
extern void		 pperl_env_unset(perlenv_t penv, const char *name);
extern void		 pperl_env_destroy(perlenv_t *penvp);


extern perlargs_t	 pperl_args_new(perlinterp_t interp, bool tainted,
					int argc, const char **argv);
extern void		 pperl_args_append(perlargs_t pargs, const char *arg);
extern void		 pperl_args_append_printf(perlargs_t pargs,
						  const char *fmt, ...)
			     __attribute__ ((format (printf, 2, 3)));
extern void		 pperl_args_destroy(perlargs_t *pargsp);


typedef size_t (pperl_io_read_t)(char *buf, size_t buflen, intptr_t data);
typedef size_t (pperl_io_write_t)(const char *buf, size_t buflen,
				  intptr_t data);
typedef void (pperl_io_close_t)(intptr_t);

extern void		 pperl_io_override(perlinterp_t interp,
					   const char *name,
					   pperl_io_read_t *onRead,
					   pperl_io_write_t *onWrite,
					   pperl_io_close_t *onClose,
					   intptr_t data);


extern void		 pperl_incpath_add(perlinterp_t interp,
					   const char *path);

extern void		 pperl_load_module(perlinterp_t interp,
					   const char *modulename,
					   perlenv_t penv,
					   struct perlresult *result);


extern perlcode_t	 pperl_load(perlinterp_t interp,
				    const char *name, perlenv_t penv,
				    const char *code, size_t codelen,
				    struct perlresult *result);
extern void		 pperl_run(perlcode_t pc,
				   perlargs_t pargs, perlenv_t penv,
				   struct perlresult *result);
extern void		 pperl_unload(perlcode_t *pcp);


extern perlcode_t	 pperl_load_file(perlinterp_t interp, const char *path,
					 perlenv_t penv,
					 struct perlresult *result);
extern perlcode_t	 pperl_load_fd(perlinterp_t interp, const char *name,
				       perlenv_t penv, int fd,
				       struct perlresult *result);

#ifdef __cplusplus
}
#endif

#endif
