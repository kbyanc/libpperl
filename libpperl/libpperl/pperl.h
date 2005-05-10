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
 *
 * $NTTMCL$
 */

#ifndef _INCLUDE_LIBPPERL_
#define _INCLUDE_LIBPPERL_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <stdarg.h>

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

	UNICODE_STDIN		= 0x00000100,	/*!< -CI perl command-line. */
	UNICODE_STDOUT		= 0x00000200,	/*!< -CO perl command-line. */
	UNICODE_STDERR		= 0x00000400,	/*!< -CE perl command-line. */
	UNICODE_STDALL		= 0x00000700,	/*!< -CS perl command-line. */

	UNICODE_INPUT_DEFAULT	= 0x00001000,	/*!< -Ci perl command-line. */
	UNICODE_OUTPUT_DEFAULT	= 0x00002000,	/*!< -Co perl command-line. */
	UNICODE_IO_DEFAULT	= 0x00003000,	/*!< -CD perl command-line. */

	UNICODE_ARGV		= 0x00004000,	/*!< -CA perl command-line. */
	_UNICODE_MASK		= 0x00007700
};


/*!
 * @struct perlresult
 *
 * Data structure for representing exit status of executed perl code.
 *
 *	@param	pperl_status	Equivalent to perl's \$! variable.
 *				Set to parameter perl code called exit() with;
 *				otherwise value is zero.
 *
 *	@param	pperl_result	Equivalent to perl's \$@ variable.
 *				Stringified version of parameter perl code
 *				called die() with; otherwise value is NULL.
 */
struct perlresult {
	int		 pperl_status;
	const char	*pperl_errmsg;
};


__BEGIN_DECLS


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
static __inline
void
pperl_result_clear(struct perlresult *result)
{
	if (result == NULL)
		return;
	result->pperl_status = 0;
	result->pperl_errmsg = NULL;
}


extern perlinterp_t	 pperl_new(const char *procname,
				   enum pperl_newflags flags);
extern void		 pperl_destroy(perlinterp_t *interpp);


extern perlenv_t	 pperl_env_new(perlinterp_t interp, bool tainted,
				       int envc, const char **envp);
extern void		 pperl_env_set(perlenv_t penv, const char *name,
				       const char *value);
extern const char	*pperl_env_get(const perlenv_t penv,
				       const char *name);
extern void		 pperl_env_unset(perlenv_t penv, const char *name);
extern void		 pperl_env_destroy(perlenv_t *penvp);


extern perlargs_t	 pperl_args_new(perlinterp_t interp, bool tainted,
					int argc, const char **argv);
extern void		 pperl_args_append(perlargs_t pargs, const char *arg);
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


/*
 * Internal logging routines for libpperl.
 * These all are defined as weak linker symbols in the library, allowing
 * applications to replace them with their own implementations.  Hence, we
 * export the declarations.
 */
extern void		 pperl_log(int priority, const char *fmt, ...)
			     __attribute__ ((format (printf, 2, 3)));
extern void		 pperl_logv(int priority, const char *fmt, va_list ap);
extern void		 pperl_fatal(int eval, const char *fmt, ...)
			     __attribute__ ((noreturn, format (printf, 2, 3)));


__END_DECLS

#endif
