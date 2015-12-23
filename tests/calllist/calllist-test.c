
#include <sys/types.h>
#include <stdbool.h>
#include <stdlib.h>

#include <pperl.h>

int
main(void)
{
	struct perlresult result;
	perlinterp_t interp;
	perlenv_t penv;
	perlargs_t pargs;
	perlcode_t pc[5];
	int i;

	interp = pperl_new("calllist-test", DEFAULT);
	penv = pperl_env_new(interp, false, 0, NULL);

	/*
	 * Load five separate instances of the test script.  Each references
	 * the CallListTest.pm module; we want to make sure that each of
	 * module's BEGIN and END blocks are only run once and that they are
	 * run in the order described in perlmod(1).
	 *
	 * XXX: Currently, a module's CHECK and INIT routines are run
	 *	*after* any code in the module's body.  This is wrong, but
	 *	I don't think it should cause a problem with real modules
	 *	since most only have "1;" in their body or don't use CHECK/
	 *	INIT blocks.
	 */
	for (i = 0; i < 5; i++) {
		pperl_result_clear(&result);
		pc[i] = pperl_load_file(interp, "calllist-test.pl", penv,
					&result);
	}

	for (i = 0; i < 5; i++) {
		pargs = pperl_args_new(interp, false, 0, NULL);
		pperl_args_append_printf(pargs, "%d", i);
		pperl_run(pc[i], pargs, penv, &result);
		pperl_args_destroy(&pargs);

		/* Unload the code; it's END block should run now. */
		pperl_unload(&pc[i]);
	}

	/*
	 * Destroy the interpreter.
	 * The END blocks of the implicitely loaded modules will be run now.
	 */
	pperl_destroy(&interp);

	exit(0);
}
