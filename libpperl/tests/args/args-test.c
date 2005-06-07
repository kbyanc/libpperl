/*
 * $NTTMCL$
 */

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
	perlcode_t pc;
	int i, j;

	interp = pperl_new("args-test", DEFAULT);
	penv = pperl_env_new(interp, false, 0, NULL);

	pperl_result_clear(&result);
	pc = pperl_load_file(interp, "args-test.pl", penv, &result);

	for (i = 0; i < 5; i++) {
		pargs = pperl_args_new(interp, false, 0, NULL);

		/* Run the script with no arguments. */
		pperl_run(pc, pargs, penv, &result);

		/* Run the script with 1 argument ... 100 arguments. */
		for (j = 1; j <= 100; j++) {
			pperl_args_append_printf(pargs, "%d", j);
			pperl_run(pc, pargs, penv, &result);
		}

		/* Rinse and repeat... */
		pperl_args_destroy(&pargs);
	}

	pperl_destroy(&interp);

	exit(0);
}
