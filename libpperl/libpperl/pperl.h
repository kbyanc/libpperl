/*
 * Copyright (c) 2004 NTT Multimedia Communications Laboratories, Inc.
 * All rights reserved 
 *
 * Redistribution and use in source and/or binary forms of 
 * this software, with or without modification, are prohibited. 
 * Detailed license terms appear in the file named "COPYRIGHT".
 *
 * $NTTMCL$
 */

#ifndef _INCLUDE_NTTMCL_PPERL_
#define _INCLUDE_NTTMCL_PPERL_

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
 * actually terminate. XXXEXCEPTION
 *
 * This implementation differs from that of most persistent perl interpreters
 * (including mod_perl) in that it *does* invoked perl CHECK and INIT blocks
 * at the appropriate time (when the code is initially loaded).
 */


typedef struct perlinterp *perlinterp_t;
typedef struct perlenv *perlenv_t;
typedef struct perlargs *perlargs_t;
typedef struct perlcode *perlcode_t;


/*!
 * Flags used to specify the behavior of an interpreter created by
 * ntt_pperl_new().  WARNINGS_* options are mutually-exclusive (that is,
 * only zero or one option from that group of flags should be specified).
 * Simiarly, the TAINT_* options are mutually exclusive.  However, the
 * UNICODE_* options may be combined.  Flags are bitwise-OR'ed together.
 * For example, WARNINGS_ENABLE|TAINT_WARN|UNICODE_STDALL|UNICODE_IO_DEFAULT
 * is equivalent to the perl command-line "-wt -CSD".
 */
enum ntt_pperl_newflags {
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


__BEGIN_DECLS

extern perlinterp_t	 ntt_pperl_new(enum ntt_pperl_newflags flags);
extern void		 ntt_pperl_destroy(perlinterp_t *interpp);


extern perlenv_t	 ntt_pperl_env_new(perlinterp_t interp, bool tainted,
					   int envc, const char **envp);
extern void		 ntt_pperl_env_set(perlenv_t penv, const char *name,
					   const char *value);
extern const char	*ntt_pperl_env_get(const perlenv_t penv,
					   const char *name);
extern void		 ntt_pperl_env_unset(perlenv_t penv, const char *name);
extern void		 ntt_pperl_env_destroy(perlenv_t *penvp);


extern perlargs_t	 ntt_pperl_args_new(perlinterp_t interp, bool tainted,
					    int argc, const char **argv);
extern void		 ntt_pperl_args_append(perlargs_t pargs,
					       const char *arg);
extern void		 ntt_pperl_args_destroy(perlargs_t *pargsp);


extern void		 ntt_pperl_incpath_add(perlinterp_t interp,
					       const char *path);

extern void		 ntt_pperl_load_module(perlinterp_t interp,
					       const char *modulename,
					       perlenv_t penv);


extern perlcode_t	 ntt_pperl_load(perlinterp_t interp,
					const char *name, perlenv_t penv,
					const char *code, size_t codelen);
extern void		 ntt_pperl_run(perlcode_t pc, perlargs_t pargs,
				       perlenv_t penv);
extern void		 ntt_pperl_unload(perlcode_t *pcp);

__END_DECLS

#endif
