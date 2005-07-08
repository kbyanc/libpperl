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

#include "queue.h"

#include "pperl.h"
#include "pperl_private.h"


/*!
 * pperl_calllist_clear() - Remove all references to the given package from a
 *			    perl call list.
 *
 *	Perl maintains a number of a special arrays called call lists to
 *	represent pseudo-subroutine code blocks.  This routine iterates over
 *	a call list, removing any entries which exist in the given package.
 *
 *	@param	calllist	Perl call list to iterate over.
 *
 *	@param	pkgstash	The perl package containing the code blocks to
 *				remove.  If NULL, all code blocks in the
 *				call list will be removed.
 */
void
pperl_calllist_clear(AV *calllist, const HV *pkgstash)
{
	SV *sv;
	int max;
	int i;

	/* Nothing to do if the call list is empty. */
	if (calllist == NULL || (max = av_len(calllist)) == -1)
		return;

	/*
	 * If no package was specified, clear all entries from the call list.
	 */
	if (pkgstash == NULL) {
		av_clear(calllist);
		return;
	}

	/*
	 * Otherwise, only remove entries which live in the specified package.
	 */ 
	for (i = 0; i <= max; i++) {

		/* Retrieve the next element in the call list array. */
		sv = av_shift(calllist);
		if (sv == NULL)
			continue;

		/* Check that it is a code reference. */
		assert (SvTYPE(sv) == SVt_PVCV);

		/* If the code belongs to a different package, put it back. */
		if (CvSTASH((CV *)sv) != pkgstash) {
			av_push(calllist, sv);
			continue;
		}

		/*
		 * Free the call list entry and adjust our record of the
		 * maximum array index to reflect reality.
		 */
		SvREFCNT_dec(sv);
		max--;	
	}
}


/*!
 * pperl_calllist_run() - Run all call list entries which are in the given
 *			  perl package.
 *
 *	By default, perl executes all BEGIN code blocks in its compilation
 *	step (in our case, in pperl_load()).  And perl executes all END
 *	code blocks when the interpreter is destroyed by perl_destroy().
 *	However, typically, persistent perl environments never execute CHECK
 *	or INIT blocks by virtue of the fact that the code blocks have not yet
 *	been declared when perl wants to run them.
 *
 *	We call this routine whenever new code is loaded into the perl
 *	interpreter in order to execute CHECK and INIT blocks (a first for
 *	persistent perl interpreters, I believe).  We also call this routine
 *	whenever code is unloaded in order to run any END blocks defined in
 *	that code (most interpreters only run END blocks when the interpreter
 *	is destroyed).
 *
 *	@param	calllist	Perl call list to iterate over.
 *
 *	@param	pkgstash	The perl package containing the code blocks to
 *				run.  If NULL, code blocks in all packages
 *				will be run.
 *
 *	@param	flags		
 *
 *	@post	ERRSV is true if any code block in the call list raised an
 *		exception.  The caller should check for this condition.
 *
 *	@note	Must be called within an ENTER/LEAVE block. 
 *		pperl_setvars() should have already been called to setup
 *		the perl environment.
 */
void
pperl_calllist_run(AV *calllist, const HV *pkgstash,
		   enum pperl_calllist_flags flags)
{
	HV *cstash;
	SV **svp;
	SV *sv;
	int i;
	dSP;

	if (calllist == NULL)
		return;

	if (calllist == PL_endav) {
		/* We are supposed to always run all END blocks. */
		assert((flags & CONTINUE_ON_ERROR) != 0);
		flags |= CONTINUE_ON_ERROR;
	}

	if (pkgstash == NULL) {
		/*
		 * Not specifying a package stash is equivalent to running
		 * code blocks in all packages.
		 */
		assert((flags & RUN_ALL) != 0);
		flags |= RUN_ALL;
	}

	for (i = 0; i <= av_len(calllist); i++) {
		I32 oldscope;

		/* Retrieve the next element in the call list array. */
		svp = av_fetch(calllist, i, FALSE);
		if (svp == NULL || *svp == &PL_sv_undef)
			continue;
		sv = *svp;

		/* Check that it is a code reference. */
		assert (SvTYPE(sv) == SVt_PVCV);

		/* Lookup the package "stash" the code resides in. */
		cstash = CvSTASH((CV *)sv);

		if ((flags & RUN_ALL) ||
		    cstash == pkgstash) {
			/*
			 * If the caller specified a package and the current
			 * code resides in that package, then fall through
			 * and run the code.
			 */
		}
		else if ((flags & RUN_PACKAGE_AND_MODULES) &&
		    strncmp(HvNAME(cstash), PPERL_NAMESPACE_PRIVATE "::_p",
			    strlen(PPERL_NAMESPACE_PRIVATE "::_p")) != 0) {
			/*
			 * If the caller specified the RUN_PACKAGE_AND_MODULES
			 * flag, then fall through and run code blocks that
			 * reside in packages not in the pperl private
			 * namespace too.  Only code loaded from perl modules
			 * can live outside the pperl private namespace.
			 */
		}
		else {
			/* Skip all other code blocks. */
			continue;
		}

		oldscope = PL_scopestack_ix;

		PUSHMARK(SP);
		call_sv(sv, G_EVAL|G_VOID|G_DISCARD|
			    (flags & CONTINUE_ON_ERROR) ? G_KEEPERR : 0);

		/* Ensure we return the same scope we started in. */
		while (PL_scopestack_ix > oldscope) {
			LEAVE;
		}

		/*
		 * Unless told to continue on error, stop calling blocks in
		 * the call list once one dies.
		 */
		if ((flags & CONTINUE_ON_ERROR) != 0)
			continue;
		if (SvTRUE(ERRSV))
			break;
	}
}
