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

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/sbuf.h>

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <syslog.h>
#include <unistd.h>

#define	HAS_BOOL	/* We use stdbool's bool type rather than perl's. */
#include <EXTERN.h>
#include <XSUB.h>
#include <perl.h>

#include "pperl.h"
#include "pperl_private.h"


EXTERN_C void	 xs_init _((void));			    /* perlxsi.c */

static void	 pperl_setvars(const char *procname);
static SV	*pperl_eval(SV *code_sv, const char *name,
			    perlenv_t penv, struct perlresult *result);
static XS(XS_pperl_exit);


/*
 * Dummy result structure used when caller doesn't provide one of their own.
 * This is junk storage and only used to simplify result reporting logic.
 */
static struct perlresult dummy_result;

static inline
void
pperl_result_init(struct perlresult **resultp)
{
	struct perlresult *result = *resultp;

	if (result == NULL)
		*resultp = &dummy_result;
	else
		pperl_result_clear(result);
}


/*!
 * pperl_seterr() - Populate result structure from given errno(2) value.
 */
void
pperl_seterr(int errnum, struct perlresult *result)
{
	if (result == NULL)
		return;
	result->pperl_status = 0;
	result->pperl_errno = errnum;
	result->pperl_errmsg = strerror(errnum); /* XXX Not re-entrant. */
}


/*!
 * pperl_new() - Create a new persistent perl interpreter.
 *
 *	Initializes a new perl interpreter for executing perl code in a
 *	persistent environment.
 *
 *	@param	procname	Process name used for current process when no
 *				perl code is executing.
 *
 *	@param	flags		Bitwise-OR of flags indicating behaviour of
 *				the new interpreter.  Corelate directly to
 *				various perl command-line options.
 *				\see pperl_newflags in pperl.h.
 *
 *	@return	Handle for referring to the new persistent perl interpreter.
 */
perlinterp_t
pperl_new(const char *procname, enum pperl_newflags flags)
{
	struct sbuf opt_sb;
	perlinterp_t interp;
	char **argv;
	PerlInterpreter *perl;

	/*
	 * Require perl 5.8.4 or later.  Not done as a compile-time check to
	 * allow libpperl to build even if installed perl is older.
	 */
	assert(PERL_REVISION == 5 && PERL_VERSION >= 8 && PERL_SUBVERSION >= 4);

	/*
	 * Convert flags into command-line options for perl_parse() as this
	 * is the only public API perl provides for toggling these features.
	 */
	sbuf_new(&opt_sb, NULL, 32, SBUF_AUTOEXTEND);

	switch (flags & _WARNINGS_MASK) {
	case WARNINGS_ENABLE:		sbuf_cat(&opt_sb, "-w "); break;
	case WARNINGS_FORCE_ALL:	sbuf_cat(&opt_sb, "-W "); break;
	case WARNINGS_FORCE_NONE:	sbuf_cat(&opt_sb, "-X "); break;
	default:			;
	}

	switch (flags & _TAINT_MASK) {
	case TAINT_WARN:		sbuf_cat(&opt_sb, "-t "); break;
	case TAINT_FATAL:		sbuf_cat(&opt_sb, "-T "); break;
	default:			;
	}

	switch (flags & _ARGLOOP_MASK) {
	case ARGLOOP_NOPRINT:		sbuf_cat(&opt_sb, "-n "); break;
	case ARGLOOP_PRINT:		sbuf_cat(&opt_sb, "-p "); break;
	default:			;
	}

	/*
	 * Have perl run a no-op script for now.
	 */
	sbuf_cat(&opt_sb, "-e;0 ");

	/* Macro for handling boolean options. */
#define OPTIONFLAG(mask, str)	if ((flags & mask) != 0) sbuf_cat(&opt_sb, str)

	OPTIONFLAG(UNSAFE_ENABLE,	  "-U ");

	/*
	 * Parse Unicode-related options.  perl_parse() requires -C... to be
	 * the final command-line argument so this is done last.
	 */
	OPTIONFLAG(_UNICODE_MASK,	  "-C");
	OPTIONFLAG(UNICODE_STDIN,	   "I");
	OPTIONFLAG(UNICODE_STDOUT,	   "O");
	OPTIONFLAG(UNICODE_INPUT_DEFAULT,  "i");
	OPTIONFLAG(UNICODE_OUTPUT_DEFAULT, "o");
	OPTIONFLAG(UNICODE_ARGV,	   "A");

#undef OPTIONFLAG

	sbuf_finish(&opt_sb);

	/*
	 * Contrary to what examples there are of using an embedded perl
	 * interpreter, we have to allocate the synthesized argv array we
	 * pass to perl_parse() on the heap (rather than on the stack).
	 * Otherwise, if the $0 variable is modified from inside the perl
	 * interpreter, the stack gets corrupted.
	 */
	argv = malloc(2 * sizeof(char *));
	argv[1] = sbuf_data(&opt_sb);		/* command-line options. */
	argv[0] = argv[1] + sbuf_len(&opt_sb);	/* "" */

	PL_perl_destruct_level = 2;

	/*
	 * Build a new perl interpreter.
	 */
	perl = perl_alloc();
	perl_construct(perl);

	/*
	 * Initialize the interpreter.  Perl intertwines the parsing and
	 * initialization steps, so we have to provide something to parse in
	 * order to initialize the interpreter to a useable state.  As such,
	 * we provide a null script using the command-line -e argument.
	 */
	if (perl_parse(perl, xs_init, 2, argv, environ) != 0) {
		pperl_fatal(EX_UNAVAILABLE,
			    "failed to initialize perl interpreter");
	}

	/*
	 * Run the parsed script, defering END blocks until we call
	 * perl_destruct().  Technically, this step isn't necessary as we
	 * had perl parse a no-op script.  However, it doesn't hurt to run
	 * it (earlier versions of perl5 required it even), so do so just to
	 * be on the safe side.
	 */
	PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
	perl_run(perl);

	/*
	 * Define our own exit function in the PPERL_NAMESPACE and remap the
	 * global "exit" function to call it instead.  This allows us to
	 * catch script exits and return them to the calling code rather than
	 * terminating the calling program.
	 * Note: Perl's prototype for newXS is missing const specifiers for
	 *	 the string arguments even though they are really constant.
	 */
	newXS(ignoreconst(PPERL_NAMESPACE "::exit"), XS_pperl_exit,
	      ignoreconst(__FILE__));
	{
		/* *CORE::GLOBAL::exit = \&libpperl::_private::exit; */
		GV *gv = gv_fetchpv("CORE::GLOBAL::exit", TRUE, SVt_PVCV);
		GvCV(gv) = get_cv(PPERL_NAMESPACE "::exit", TRUE);
		GvIMPORTED_CV_on(gv);
	}

	/*
	 * Now that the perl interpreter is initialized, construct our local
	 * data structure to contain the interpreter state information.
	 */
	interp = pperl_malloc(sizeof(*interp));
	interp->pi_perl = perl;
	interp->pi_alloc_argv = argv;
	LIST_INIT(&interp->pi_args_head);
	LIST_INIT(&interp->pi_code_head);
	LIST_INIT(&interp->pi_env_head);
	LIST_INIT(&interp->pi_io_head);

	pperl_io_init();

	/*
	 * Set the default process name displayed in 'ps' when no perl code
	 * is being executed.  If we do not set this explicitely, perl will
	 * display '-e' which is a pretty obscure default.
	 */
	{
		GV *zero = gv_fetchpv("0", TRUE, SVt_PV); 
		sv_setpv_mg(GvSV(zero), procname);
	}

	pperl_log(LOG_DEBUG, "perl interpreter initialized (%p)", interp);

	return (interp);
}


/*!
 * pperl_destroy() - Destroy a persistent perl interpreter.
 *
 *	Frees all memory associated with the given perl interpreter as well
 *	as all code loaded into the interpreter.
 *
 *	@param	interpp		Pointer to perlinterp_t to destroy.
 *
 *	@post	*interpp is set to NULL.
 */
void
pperl_destroy(perlinterp_t *interpp)
{
	perlinterp_t interp = *interpp;
	perlcode_t code;
	perlargs_t pargs;
	perlenv_t penv;
	perlio_t pio;
	PerlInterpreter *orig_perl;
	PerlInterpreter *perl;

	*interpp = NULL;

	assert(interp != NULL);

	perl = interp->pi_perl;
	orig_perl = PERL_GET_CONTEXT;
	PERL_SET_CONTEXT(perl);

	while (!LIST_EMPTY(&interp->pi_code_head)) {
		code = LIST_FIRST(&interp->pi_code_head);
		LIST_REMOVE(code, pc_link);

		/*
		 * Note: we do not need to clean up the perl data structures
		 *	 because they will be freed automatically when the
		 *	 perl interpreter is destroyed below.
		 */

		free(code->pc_name);
		free(code);
	}

	while (!LIST_EMPTY(&interp->pi_args_head)) {
		pargs = LIST_FIRST(&interp->pi_args_head);
		pperl_args_destroy(&pargs);
	}

	while (!LIST_EMPTY(&interp->pi_env_head)) {
		penv = LIST_FIRST(&interp->pi_env_head);
		pperl_env_destroy(&penv);
	}

	while (!LIST_EMPTY(&interp->pi_io_head)) {
		pio = LIST_FIRST(&interp->pi_io_head);
		pperl_io_destroy(&pio);
	}

	PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
	PL_perl_destruct_level = 2;

	perl_destruct(perl);
	perl_free(perl);

	/*
	 * Free memory allocated to our interpreter bookkeeping data structure.
	 */
	free(interp->pi_alloc_argv[1]);		/* "-e;0" argument string. */
	free(interp->pi_alloc_argv);		/* argument vector itself. */
	free(interp);

	PERL_SET_CONTEXT(orig_perl);
}


/*!
 * pperl_incpath_add() - Add directories to perl's \@INC search path.
 *
 *	Adds additional directories to the head of perl's \@INC search path
 *	similar to perl's -I command-line option.  Only a single path may be
 *	added per call.
 *
 *	@param	interp		Interpreter to modify \@INC path in.
 *
 *	@param	path		The directory to add to the path.
 *
 *	@note	All code loaded into a single interpreter shares the same
 *		global \@INC array.  That is, changes made by one piece of
 *		code effect all other code loaded into the interpreter.  I
 *		did not see any point in virtualizing \@INC (as we do for
 *		\%ENV and \@ARGV) because any modules located via the \@INC
 *		array are also shared by all code loaded into the interpreter.
 */
void
pperl_incpath_add(perlinterp_t interp, const char *path)
{
	PerlInterpreter *orig_perl;
	SV *path_sv;

	orig_perl = PERL_GET_CONTEXT;
	PERL_SET_CONTEXT(interp->pi_perl);

	/*
	 * Push the new path on the end of the @INC array.  The array is
	 * scanned in reverse by perl, so this effectively puts the new
	 * include at the head of the list (same as -I command-line option).
	 */
	path_sv = newSVpv(path, 0);
	av_push(GvAVn(PL_incgv), path_sv);

	PERL_SET_CONTEXT(orig_perl);
}


/*!
 * pperl_load_module() - Load a perl module into the interpreter.
 *
 *	Loads the given perl module into the interpreter.  Equivilent to
 *	the "require" perl command, complete with perl's module naming
 *	semantics.
 *
 *	In general, loading code which requires a module will automatically
 *	load that module as part of pperl_load(). As such, this routine
 *	is only useful if arbitrary code is going to be loaded during the
 *	program's lifetime and you want to speed pperl_load() by ensuring
 *	any required modules are preloaded.  If you are going to load all of
 *	your code when the program is started, then this routine gains you
 *	nothing.
 *
 *	@param	interp		Perl interpreter to load module into.
 *
 *	@param	modulename	Name of perl module to load (e.g. "File::Sync").
 *
 *	@param	penv		Environment variable list to populate \%ENV
 *				with while loading code.  This is primarilly
 *				for the benefit of any BEGIN, CHECK, or INIT
 *				code blocks that may run during load.
 *
 *	@param	result		If non-NULL, populated with the result returned
 *				by any perl BEGIN, CHECK, or INIT blocks
 *				executed as part of the module load.
 */
void
pperl_load_module(perlinterp_t interp, const char *modulename,
		  perlenv_t penv, struct perlresult *result)
{
	PerlInterpreter *orig_perl;

	pperl_result_init(&result);

	orig_perl = PERL_GET_CONTEXT;
	PERL_SET_CONTEXT(interp->pi_perl);

	ENTER;
	SAVETMPS;

	pperl_setvars(modulename);
	pperl_env_populate(penv);

	/*
	 * What follows is almost identical to the implementation of perl's
	 * require_pv() function except that it doesn't wrap the argument in
	 * single quotes, thus allowing modules to be specified by name
	 * (e.g. File::Spec).  This is identical to mod_perl's
	 * modperl_require_module() function which should kill any doubt that
	 * perl's embedded API sucks.
	 *
	 * We can't follow perlapi(3)'s recommendation to use load_module()
	 * either as that API croaks if a non-existent module is requested.
	 * In practice, the only safe thing to do is to evaluate the perl code
	 * "require Module" which is exactly what we do...
	 */
	{
		SV* sv;
		dSP;

		PUSHSTACKi(PERLSI_REQUIRE);
		PUTBACK;
		sv = sv_newmortal();
		sv_setpv(sv, "require ");
		sv_catpv(sv, modulename);
		eval_sv(sv, G_DISCARD|G_KEEPERR);
		SPAGAIN;
		POPSTACK;
	}

	FREETMPS;
	LEAVE;

	result->pperl_status = STATUS_CURRENT;
	if (SvTRUE(ERRSV)) {
		/*
		 * XXX Return error to caller.  It would be nice if we could
		 *     postprocess the error messages to make them more useful.
		 */
		result->pperl_errmsg = SvPVX(ERRSV);
		pperl_log(LOG_DEBUG, "%s(%s): %s",
			  __func__, modulename, result->pperl_errmsg);
	}

	PERL_SET_CONTEXT(orig_perl);
}


/*!
 * pperl_setvars() - Populate global perl variables.
 *
 *	Properly sets up several of perl's global variables with appropriate
 *	values in preparation to run perl code.
 *
 *	@param	procname	Process name to use while executing perl
 *				code; this appears in ps output as well as
 *				\$0 to the running perl code.
 *
 *	@note	Must be called within a perl ENTER/LEAVE block.
 */
void
pperl_setvars(const char *procname)
{

	/*
	 * Reset one-time ?pattern? searches.
	 * Note: ?pattern? searches are deprecated, so this is probably
	 *	 unnecessary.
	 * Note: perl's prototype for sv_reset is missing a const qualifier
	 *	 for the first parameter even though it is constant.
	 */
	sv_reset(ignoreconst(""), PL_defstash);

	/*
	 * Reset the $@ variable to indicate no error.
	 */
	sv_setpv(ERRSV, "");

	/*
	 * Set $0 (and hence the process's name as it appears in ps output)
	 * to the name associated with the perl code being run.  Localize
	 * $0 so that the process name will be restored automatically when
	 * the LEAVE statement below is executed.
	 */
	{
		GV *zero = gv_fetchpv("0", TRUE, SVt_PV);
		save_scalar(zero);	/* local $0 */
		sv_setpv_mg(GvSV(zero), procname);
	}

	/*
	 * Virtualize the %SIG hash for the running code.
	 */
	{
		GV *sig = gv_fetchpv("SIG", TRUE, SVt_PVHV);
		save_hptr(&GvHV(sig));	/* local %SIG */
	}

	/*
	 * Ensure $$ contains the correct process ID.  This covers the
	 * possibility that the calling process may fork after calling
	 * pperl_new().
	 */
	{
		GV *pid = gv_fetchpv("$", TRUE, SVt_PV);
		sv_setiv(GvSV(pid), (I32)getpid());
	}
}


/*!
 * pperl_eval() - Evaluate perl code in current interpreter.
 *
 *	Executes the given code inside a perl eval statement.  The code is
 *	always evaluated in scalar context and the result returned.
 *	See 'perldoc -f eval' for details; the function implements the
 *	behavior of the "eval EXPR" syntax.
 *
 *	@note	Perl code in BEGIN, CHECK, and INIT blocks is always executed
 *		during evaluation.  Depending on the expressing being evaluated,
 *		addition code may also be executed.  If an exception is thrown
 *		by any executed code, evaluation fails.  The error message
 *		is propogated into the pperl_errmsg member of the perlresult
 *		structure pointed to by \a result.
 *
 *	@param	code_sv		Perl scalar containing the code to execute
 *				within the eval.
 *
 *	@param	name		Text describing the code being evaluated; this
 *				is used as the \$0 variable visible from the
 *				perl code, appears in ps(1) output, and is
 *				used in error messages pertaining to the
 *				code.  If the code being evaluated was read
 *				from a file, it is recommended that the file
 *				name be passed as the \a name argument.
 *
 *	@param	penv		Environment variable list to populate \%ENV
 *				with while evaluating the string.
 *
 *	@param	result		If non-NULL, populated with the result returned
 *				by any perl code executed during the evaluation.
 *
 *	@return	The result of the eval statement or NULL if the statement
 *		failed to be evaluated.
 */
SV *
pperl_eval(SV *code_sv, const char *name, perlenv_t penv,
	   struct perlresult *result)
{
	SV *anonsub;
	HV *pkgstash;
	void *origstart;
	dSP;			/* Declare local perl stack pointer. */

	pperl_result_init(&result);

#if 0
	fprintf(stderr, "eval> %s", SvPV(code_sv, PL_na));
#endif

	ENTER;
	SAVETMPS;

	pperl_setvars(name);
	pperl_env_populate(penv);

	PUSHMARK(SP);

	/*
	 * Replace the main program start opcode pointer so that any CHECK or
	 * INIT subroutine declarations don't generate warnings.  Cache the
	 * original value so we can restore it later.
	 */
	origstart = PL_main_start;
	PL_main_start = NULL;

	eval_sv(code_sv, G_SCALAR|G_NOARGS|G_EVAL|G_KEEPERR);
	PL_main_start = origstart;

	SPAGAIN;

	/*
	 * Pop the reference to the anonymous subroutine off the top of the
	 * perl stack.  Increment the reference count since we'll be holding
	 * onto it from a while.
	 */
	anonsub = SvREFCNT_inc(POPs);

	/* Don't need perl scalar containing code text anymore. */
	SvREFCNT_dec(code_sv);

	result->pperl_status = STATUS_CURRENT;
	if (SvTRUE(ERRSV)) {
		/*
		 * XXX Return error to caller.  It would be nice if we could
		 *     postprocess the error messages to make them more useful.
		 */
		SvREFCNT_dec(anonsub);
		PUTBACK;
		FREETMPS;
		LEAVE;

		result->pperl_errmsg = SvPVX(ERRSV);
		pperl_log(LOG_DEBUG, "%s(%s): %s",
			  __func__, name, result->pperl_errmsg);

		return (NULL);
	}

	/*
	 * No error; we should have the only reference to the anonymous sub.
	 * Lookup the symbol table for the package we wrapped the sub in.
	 */
	assert(SvROK(anonsub));
	{
		SV *sv = SvRV(anonsub);
		assert(SvTYPE(sv) == SVt_PVCV);

		pkgstash = CvSTASH((CV *)sv);
	}

	/*
	 * Run any CHECK or INIT blocks defined in the given code.  These are
	 * run with the same environment already setup for the compilation
	 * step.
	 *
	 * Note that we run all defined CHECK or INIT blocks to ensure that
	 * we also run blocks defined by any modules loaded by the given code.
	 * The only blocks that can possibly by in the call lists are those
	 * defined by the newly-loaded code and newly-loaded modules as all
	 * other blocks would have already been run, and hence removed from
	 * the call lists.
	 */
	pperl_calllist_run_all(PL_checkav);
	pperl_calllist_run_all(PL_initav);

	PUTBACK;
	FREETMPS;
	LEAVE;

	/*
	 * Handle any exceptions raised by CHECK or INIT blocks.
	 */
	result->pperl_status = STATUS_CURRENT;
	if (SvTRUE(ERRSV)) {
		SvREFCNT_dec(anonsub);
		result->pperl_errmsg = SvPVX(ERRSV);
		pperl_log(LOG_DEBUG, "%s(%s): %s",
			  __func__, name, result->pperl_errmsg);

		return (NULL);		
	}

	assert(SvREFCNT(anonsub) == 1);

	return (anonsub);
}


/*!
 * pperl_load() - Load perl code into interpreter for later execution.
 *
 *	@param	interp		Perl interpreter to load the code into;
 *				the code will always be executed in this
 *				interpreter.
 *
 *	@param	name		Text describing the code being loaded.  See
 *				explanation under pperl_eval().
 *
 *	@param	penv		Environment variable list to populate \%ENV
 *				with while loading code.  This is primarilly
 *				for the benefit of any BEGIN, CHECK, or INIT
 *				code blocks that may run during load.
 *
 *	@param	code		The perl code to load.  Does not require a
 *				nul-terminator as the length is explicitely
 *				provided via the \a codelen argument.
 *
 *	@param	codelen		The length (in bytes) of the perl code to load.
 *
 *	@param	result		If non-NULL, populated with the result returned
 *				by any perl BEGIN, CHECK, or INIT code blocks
 *				executed during load.
 *
 *	@return	Handle for refering to the loaded code if successful.  If
 *		an error occurred during load, returns NULL; if \a result
 *		was non-NULL, it is populated with the cause of the failure.
 */
perlcode_t
pperl_load(perlinterp_t interp, const char *name, perlenv_t penv,
	   const char *code, size_t codelen, struct perlresult *result)
{
	static u_int pkgid = 0;
	PerlInterpreter *orig_perl;
	perlcode_t pc;
	SV *code_sv;
	SV *anonsub;
	HV *pkgstash;

	/*
	 * Compile the code in the given interpreter context.
	 */
	orig_perl = PERL_GET_CONTEXT;
	PERL_SET_CONTEXT(interp->pi_perl);

	/*
	 * Increment counter by some prime number so we can build unique
	 * package name below.  "1" would work, but I picked a more esoteric
	 * prime to discourage people from trying to guess package names.
	 */
	pkgid += 17261921;

	/*
	 * The only way to compile code in perl is to create an anonymous
	 * subroutine which can then be called later.  To do that, we wrap
	 * the code to compile in a sub { ... } block and have perl evaluate
	 * that, which returns a reference to the anonymous subroutine which
	 * we can call later.  Since the subroutine is anonymous, no symbols
	 * are added to the perl namespace.
	 *
	 * However, when the compiled code is executed (by calling the
	 * anonymous subroutine), it may create global variables which would
	 * populate the default perl namespace, potentially conflicting with
	 * variables created by other code.  As such, we further isolate the
	 * anonymous subroutine in its own, uniquely named, perl package;
	 * hence, any "global" variables created by the code will actually be
	 * in the package's namespace away from other code's variables.
	 *
	 * Note: wrapping is performed via multiple statements rather than a
	 *	 call to sv_catpvf() in order to optimize the case that caller
	 *	 may not have a nul-terminated string (e.g. perl code was
	 *	 mmap(2)'ed from a file).
	 */
	code_sv = newSV(codelen + 100);
	sv_catpvf(code_sv, "package %s::_p%08X; sub {\n", PPERL_NAMESPACE, pkgid);
	sv_catpvn(code_sv, code, codelen);
	sv_catpv(code_sv, "\n}\n");

	anonsub = pperl_eval(code_sv, name, penv, result);

	/*
	 * If we failed to evaluate the code, propogate the error back to our
	 * caller.  Details will be in the 'result' structure.
	 */
	if (anonsub == NULL) {
		PERL_SET_CONTEXT(orig_perl);
		return (NULL);
	}

	/*
	 * Lookup perl "stash" representing the encapsulating package.
	 */
	{
		SV *sv = SvRV(anonsub);
		assert(SvTYPE(sv) == SVt_PVCV);

		pkgstash = CvSTASH((CV *)sv);
	}

	/*
	 * Construct perlcode_t data structure to refer to the compiled perl
	 * code.
	 */
	pc = pperl_malloc(sizeof(struct perlcode));
	pc->pc_name = pperl_strdup(name);

	LIST_INSERT_HEAD(&interp->pi_code_head, pc, pc_link);
	pc->pc_interp = interp;
	pc->pc_sv = anonsub;
	pc->pc_pkgid = pkgid;
	pc->pc_pkgstash = pkgstash;

	/* Restore perl context. */
	PERL_SET_CONTEXT(orig_perl);

	return (pc);
}


/*!
 * pperl_run() - Execute loaded perl code.
 *
 *	Runs code loaded via pperl_load() with \@ARGV and \%ENV populated
 *	from the values passed via \a pargs and \a penv.
 *
 *	@param	pc		The perl code to run.
 *
 *	@param	pargs		Argument list to pass as the perl \@ARGV array
 *				of command-line parameters when running the
 *				code.  If NULL, perl's \@ARGV array will be
 *				empty.
 *
 *	@param	penv		Environment variable list to populate \%ENV
 *				with while running code.
 *
 *	@param	result		If non-NULL, populated with the exit status
 *				and/or error message returned by the executed
 *				perl code.
 */
void
pperl_run(const perlcode_t pc, perlargs_t pargs, perlenv_t penv,
	      struct perlresult *result)
{
	const perlinterp_t interp = pc->pc_interp;
	PerlInterpreter *orig_perl;
	dSP;

	pperl_result_init(&result);

	/*
	 * Save perl's notion of the "current" interpreter and switch to
	 * the interpreter that code was compiled in.  Note that we have to
	 * refresh our local copy of perl's state pointer afterwards as the
	 * copy initialized by dSP refers to the original interpreter's stack.
	 */
	orig_perl = PERL_GET_CONTEXT;
	PERL_SET_CONTEXT(interp->pi_perl);
	SPAGAIN;

	ENTER;
	SAVETMPS;

	pperl_setvars(pc->pc_name);
	pperl_env_populate(penv);
	pperl_args_populate(pargs);

	/*
	 * Run the code.
	 */
	PUSHMARK(SP);
	call_sv(pc->pc_sv, G_EVAL|G_VOID|G_DISCARD);

	FREETMPS;
	LEAVE;

	result->pperl_status = STATUS_CURRENT;
	if (SvTRUE(ERRSV)) {
		/*
		 * XXX Return error to caller.  It would be nice if we could
		 *     postprocess the error messages to make them more useful.
		 */
		result->pperl_errmsg = SvPVX(ERRSV);
		pperl_log(LOG_DEBUG, "%s(%s): %s",
			  __func__, pc->pc_name, result->pperl_errmsg);
	}

	/* Restore perl's notion of the "current" interpreter. */
	PERL_SET_CONTEXT(orig_perl);
}


/*!
 * pperl_unload() - Unload code from a perl interpreter.
 *
 *	@warning
 *		If any symbols were imported from other packages, the memory
 *		for those symbols is effectively lost when the code is
 *		unloaded.  There is no documented perl API for finding and/or
 *		removing such symbols so there is nothing we can do about it.
 *		(I've spent days trying to reverse engineer perl's internals
 *		 looking for a solution, but to no avail).  So, as things are
 *		now, this routine can currently only reclaim *most* of the
 *		memory allocated to the unloaded code.
 *
 *	@param	pcp		Pointer to perlcode_t handle of code to unload.
 *
 *	@post	*pcp is set to NULL.
 */
void
pperl_unload(perlcode_t *pcp)
{
	perlcode_t pc = *pcp;
	PerlInterpreter *orig_perl;
	char *name;
	HV *parentstash;
	HV *pkgstash;
	SV *sv;

	*pcp = NULL;

	orig_perl = PERL_GET_CONTEXT;
	PERL_SET_CONTEXT(pc->pc_interp->pi_perl);

	/*
	 * Run END blocks now.  It doesn't really matter if they raise an
	 * exception, because we are going to unload the code anyway.
	 */
	ENTER;
	pperl_setvars(pc->pc_name);
	pperl_calllist_run(PL_endav, pc->pc_pkgstash);
	LEAVE;

	/*
	 * Ensure there are no references to BEGIN, CHECK, or INIT blocks in
	 * the code's package.
	 */
	pperl_calllist_clear(PL_beginav, pc->pc_pkgstash);
	pperl_calllist_clear(PL_checkav, pc->pc_pkgstash);
	pperl_calllist_clear(PL_initav, pc->pc_pkgstash);

	/*
	 * Perl squirrels away extra references to BEGIN and CHECK blocks.
	 * Since want to remove all traces of the code being unloaded, we have
	 * to remove references from perl's secret hiding places too.
	 */
	pperl_calllist_clear(PL_beginav_save, pc->pc_pkgstash);
	pperl_calllist_clear(PL_checkav_save, pc->pc_pkgstash);

	/*
	 * Perform sanity checking to ensure we have a reference to a
	 * subroutine.
	 */
	sv = pc->pc_sv;
	assert(SvROK(sv));

	sv = SvRV(sv);
	assert(SvTYPE(sv) == SVt_PVCV);

	/*
	 * Drop our reference to the subroutine and clear all symbols from the
	 * package created as a unique namespace for the code to execute in.
	 */
	pkgstash = pc->pc_pkgstash;
	assert (pkgstash == CvSTASH((CV *)sv));

	SvREFCNT_dec(pc->pc_sv);
	assert(SvREFCNT(pc->pc_sv) == 0);

	hv_undef(pkgstash);

	/*
	 * Remove unique package name from parent package's namespace.
	 */
	parentstash = gv_stashpv(PPERL_NAMESPACE, FALSE);
	asprintf(&name, "_p%08X::", pc->pc_pkgid);
	hv_delete(parentstash, name, strlen(name), G_DISCARD);
	free(name);

	/*
	 * Free the perlcode_t data structure itself.
	 */
	LIST_REMOVE(pc, pc_link);
	free(pc->pc_name);
	free(pc);

	PERL_SET_CONTEXT(orig_perl);
}


/*!
 * XS_pperl_exit() - Non-fatal replacement for perl's exit function.
 *
 *	Custom exit routine which is installed as the global "exit" symbol by
 *	pperl_new().  Converts a call to exit() from perl code into an
 *	exception which can be caught by the calling C code.  This prevents
 *	perl code from accidentally terminating the calling program.
 *
 *	Has to be written as a perl XS extension.
 */
XS(XS_pperl_exit)
{
	dXSARGS;

	(void)cv;		/* Silence warning about unused parameter. */

	/*
	 * Temporarily clear $SIG{__DIE__} while we throw our exception to
	 * prevent it from interfering.
	 */
	ENTER;
	SAVESPTR(PL_diehook);
	PL_diehook = Nullsv;

#if 0
	errsv = get_sv("@", TRUE);
	sv_setsv(errsv, XXX exception_object);
#endif

	if (items > 0) {
		STATUS_CURRENT = POPi;
		PUTBACK;
	}

	sv_setpv(ERRSV, "");
	croak(Nullch);
	LEAVE;

	XSRETURN_EMPTY;
}
