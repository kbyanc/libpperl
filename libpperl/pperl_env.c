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


/*!
 * pperl_env_new() - Initialize an environment list.
 *
 *	Creates a new environment list, initializing it with the contents of
 *	the given environ(7)-style array of strings.
 *
 *	@param	interp		Interpreter to create \%ENV in.
 *
 *	@param	tainted		Whether or not perl could should consider the
 *				environment variables "tainted" (possibly
 *				untrustworthy).  This is global as it applies
 *				to all elements in the environment list.
 *
 *	@param	envc		Number of strings in the \a envp array to
 *				initialize environment with.  If -1, then all
 *				elements in the \a envp array will be added up
 *				to the first NULL entry.  If 0, no variables
 *				will be added to the environment and \a envp
 *				may be NULL.
 *
 *	@param	envp		Array of strings to initialize the environment
 *				from.  If \a envc is -1, the array must be
 *				terminated with a NULL entry.  Each string
 *				is expected to contain a variable name and
 *				value separated by an equals ('=') sign.
 *
 *	@return	New environment list.
 *
 */
perlenv_t
pperl_env_new(perlinterp_t interp, bool tainted, int envc, const char **envp)
{
	PerlInterpreter *orig_perl;
	perlenv_t penv;

	orig_perl = PERL_GET_CONTEXT;
	PERL_SET_CONTEXT(interp->pi_perl);

	penv = pperl_malloc(sizeof(*penv));
	penv->pe_interp = interp;
	penv->pe_envhash = newHV();
	penv->pe_tainted = tainted;

	if (envp == NULL)
		envc = 0;

	for (; envc != 0 && *envp != NULL; envc--, envp++) {
		SV *val_sv;
		const char *key, *value;
		size_t keylen;
 
		key = *envp;
		value = strchr(key, '=');
		if (value == NULL)
			continue;		/* Skip strings lacking '='. */
		keylen = value - key;
		value++;

		/* Add element to hash. */
		val_sv = newSVpv(value, 0);
		hv_store(penv->pe_envhash, key, keylen, val_sv, 0);
	}

	LIST_INSERT_HEAD(&interp->pi_env_head, penv, pe_link);

	PERL_SET_CONTEXT(orig_perl);

	return (penv);
}


/*!
 * pperl_env_destroy() - Free all memory allocated to an environment list.
 *
 *	@param	penvp		Pointer to environment list to free.
 *
 *	@post	*penvp is set to NULL.
 */
void
pperl_env_destroy(perlenv_t *penvp)
{
	PerlInterpreter *orig_perl;
	perlenv_t penv = *penvp;

	orig_perl = PERL_GET_CONTEXT;
	PERL_SET_CONTEXT(penv->pe_interp->pi_perl);

	*penvp = NULL;
	LIST_REMOVE(penv, pe_link);
	SvREFCNT_dec(penv->pe_envhash);
	free(penv);

	PERL_SET_CONTEXT(orig_perl);
}


/*!
 * pperl_env_set() - Add or update a perl environment variable.
 *
 *	@param	penv		Perl environment variable list to update.
 *
 *	@param	name		Name of environment variable.
 *
 *	@param	value		Value to assign to the environment variable.
 */
void
pperl_env_set(perlenv_t penv, const char *name, const char *value)
{
	PerlInterpreter *orig_perl;
	SV *val_sv;
	size_t namelen;

	orig_perl = PERL_GET_CONTEXT;
	PERL_SET_CONTEXT(penv->pe_interp->pi_perl);

	namelen = strlen(name);
	val_sv = newSVpv(value, 0);
	hv_store(penv->pe_envhash, name, namelen, val_sv, 0);

	PERL_SET_CONTEXT(orig_perl);
}


/*!
 * pperl_env_setf() - Add or update a perl environment variable with a value
 *		      constructed from a printf(3)-style format specifier.
 *
 *	@param	penv		Perl environment variable list to update.
 *
 *	@param	name		Name of environment variable.
 *
 *	@param	fmt		printf(3)-style format string which specifies
 *				how to construct the environment variable's
 *				value from the remaining arguments.
 */
void
pperl_env_setf(perlenv_t penv, const char *name, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	pperl_env_setv(penv, name, fmt, ap);
	va_end(ap);
}


/*!
 * pperl_env_setv() - Add or update a perl environment variable with a value
 *		      constructed from a printf(3)-style format specifier and
 *		      an stdarg argument list.
 *
 *	@param	penv		Perl environment variable list to update.
 *
 *	@param	name		Name of environment variable.
 *
 *	@param	fmt		printf(3)-style format string which specifies
 *				how to construct the environment variable's
 *				value.
 *
 *	@param	ap		stdarg(3) variable-length argument list to
 *				be formatted.
 */
void
pperl_env_setv(perlenv_t penv, const char *name, const char *fmt, va_list ap)
{
	char *value;

	vasprintf(&value, fmt, ap);
	if (value == NULL)
		pperl_fatal(EX_OSERR, "malloc: %m");

	pperl_env_set(penv, name, value);
	free(value);
}


/*!
 * pperl_env_get() - Lookup value of perl environment variable.
 *
 *	@param	penv		Perl environment variable list to query.
 *
 *	@param	name		Name of environment variable to retrieve value
 *				for.
 *
 *	@return	Pointer to environment variable's value.  This is a pointer
 *		to internal storage that is freed when the variable (or the
 *		environment it is in) is deleted.
 */
const char *
pperl_env_get(const perlenv_t penv, const char *name)
{
	PerlInterpreter *orig_perl;
	SV **val_svp;
	const char *result;
	size_t namelen;

	result = NULL;

	orig_perl = PERL_GET_CONTEXT;
	PERL_SET_CONTEXT(penv->pe_interp->pi_perl);

	namelen = strlen(name);
	val_svp = hv_fetch(penv->pe_envhash, name, namelen, 0);
	if (val_svp != NULL)
		result = SvPV_nolen(*val_svp);

	PERL_SET_CONTEXT(orig_perl);

	return (result);
}


/*!
 * pperl_env_unset() - Delete a perl environment variable.
 *
 *	Removes any environment variable with the specified name from the
 *	environment variable list.  If no variable exists with the given name,
 *	does nothing.
 *
 *	@param	penv		Perl environment variable list to update.
 *
 *	@param	name		Name of the environment variable to delete.
 */
void
pperl_env_unset(perlenv_t penv, const char *name)
{
	PerlInterpreter *orig_perl;
	size_t namelen;

	orig_perl = PERL_GET_CONTEXT;
	PERL_SET_CONTEXT(penv->pe_interp->pi_perl);

	namelen = strlen(name);
	hv_delete(penv->pe_envhash, name, namelen, G_DISCARD);

	PERL_SET_CONTEXT(orig_perl);
}


/*!
 * pperl_env_populate() - Populate \%ENV hash from environment list.
 *
 *	Replaces the contents of the \%ENV hash in the current interpreter
 *	with name/value pairs in the specified environment variable list.
 *	Saves the original environment to be restored when LEAVE statement
 *	encountered.
 *
 *	@param	penv		Environment variable list to populate \%ENV
 *				from.  If NULL,  \%ENV is set to an empty hash.
 *
 *	@note	Must be called inside an ENTER/LEAVE block.
 *
 *	@warning
 *		Perl's \%ENV hash manipulates the global process environ
 *		variable directly.  While the original contents of environ
 *		are restored after the perl code is executed, this behavior
 *		precludes using an embedded perl interpreter in an threaded
 *		program if more than one thread may manipulate the global
 *		environ variable.  Blame perl.
 */
void
pperl_env_populate(perlenv_t penv)
{
	HV *envhash_hv;
	HE *entry;
	SV *val_sv;
	char **newenviron;
	int count;
	int i;

	/*
	 * Ensure that perl's global PL_envgv pointer refers to the symbol
	 * table entry for ENV.
	 */
	PL_envgv = gv_fetchpv("ENV", TRUE, SVt_PVHV);
	GvMULTI_on(PL_envgv);		/* XXX May not be necessary. */

	/*
	 * If there is no environment to install, simply saving the original
	 * %ENV hash will leave us with a localized empty %ENV hash.
	 */
	if (penv == NULL) {
		save_hash(PL_envgv);
		return;
	}

	/*
	 * Make a copy of the original environment.  Perl will clear out the
	 * global environ variable when it creates the empty local copy in
	 * save_hash().  Technically, it would then refill it as we put
	 * keys into the %ENV hash, but it does so fairly inefficiently.
	 * Instead, we disable perl's %ENV magic so we can copy the environment
	 * ourself and then renable the magic later.  Yes, perl really calls
	 * it "magic".
	 *
	 * Note: If we ever implement the copy-on-dirty optimization described
	 *	 in pperl_private.h, we can cache this copy of the environment
	 *	 too.
	 */
	count = HvUSEDKEYS(GvHVn(PL_envgv)) + 1;
	newenviron = pperl_malloc(count * sizeof(char *));
	for (i = 0; i < count && environ[i] != NULL; i++) {
		if (strchr(environ[i], '=') != NULL)
			newenviron[i] = strdup(environ[i]);
		else
			asprintf(&newenviron[i], "%s=", environ[i]);

		if (newenviron[i] == NULL)
			pperl_fatal(EX_OSERR, "malloc: %m");
	}
	newenviron[i] = NULL;

	/*
	 * Localize %ENV.  This clears the global environ variable as a
	 * side-effect.
	 */
	envhash_hv = save_hash(PL_envgv);

	assert(penv->pe_interp->pi_perl == PERL_GET_CONTEXT);

	/*
	 * Clear perl's %ENV magic so it doesn't touch the global environ
	 * array.  Install our newly-copied environment in place of the empty
	 * one perl left us with.
	 */
	sv_unmagic((SV*)envhash_hv, PERL_MAGIC_env);

	free(environ);
	environ = newenviron;

	/*
	 * XXX It would be nice if we could do the equivilent of
	 *     keys(%ENV)= count here.
	 */

	/*
	 * Iterate through our environment variable hash, adding each element
	 * to the %ENV hash.
	 */
	hv_iterinit(penv->pe_envhash);
	while ((entry = hv_iternext_flags(penv->pe_envhash, 0)) != NULL) {

		val_sv = newSVsv(HeVAL(entry));
		if (penv->pe_tainted)
			SvTAINT(val_sv);

		hv_store_flags(envhash_hv, HeKEY(entry), HeKLEN(entry),
			       val_sv, HeHASH(entry), HeKFLAGS(entry));
	}

	/*
	 * Reinstate perl's %ENV magic so it can tinker with environ again.
	 */
	hv_magic(envhash_hv, Nullgv, PERL_MAGIC_env);
}

