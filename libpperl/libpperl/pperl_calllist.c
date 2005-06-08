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

#include <sys/types.h>
#include <sys/queue.h>

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
 *				remove.
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
 *	This routine is similar to pperl_calllist_clear() except that the
 *	entries in the call list are run before being removed.
 *
 *	By default, perl executes all BEGIN code blocks in its compilation
 *	step (in our case, in pperl_load()).  And perl executes all END
 *	code blocks when the interpreter is destroyed by perl_destroy().
 *	However, typically, persistent perl environments never execute CHECK
 *	or INIT blocks by virtue of the fact that the code blocks have not yet
 *	been declared when perl wants to run them.
 *
 *	This allows us to properly call CHECK and INIT blocks (a first for
 *	persistent perl interpreters, I believe), explictely call END blocks
 *	before code is unloaded from the interpreter (not just when the
 *	interpreter is destroyed), and allows code to be unloaded from an
 *	interpreter without leaking memory (see notes in pperl_unload()).
 *
 *	@param	calllist	Perl call list to iterate over.
 *
 *	@param	pkgstash	The perl package containing the code blocks to
 *				run/remove.
 *
 *	@post	ERRSV is true if any code block in the call list raised an
 *		exception.  The caller should check for this condition.
 *
 *	@note	Must be called within an ENTER/LEAVE block. 
 *		pperl_setvars() should have already been called to setup
 *		the perl environment.
 */
void
pperl_calllist_run(AV *calllist, const HV *pkgstash)
{
	SV *sv;
	int max;
	int i;
	dSP;

	/* Nothing to do if the call list is empty. */
	if (calllist == NULL || (max = av_len(calllist)) == -1)
		return;

	for (i = 0; i <= max; i++) {

		/* Retrieve the next element in the call list array. */
		sv = av_shift(calllist);
		if (sv == NULL)
			continue;

		/* Check that it is a code reference. */
		assert (SvTYPE(sv) == SVt_PVCV);

		/*
		 * If the code belongs to a different package, put it back.
		 */
		if (CvSTASH((CV *)sv) != pkgstash) {
			av_push(calllist, sv);
			continue;
		}

		/*
		 * We always run all END code blocks, but for all other call
		 * lists we stop calling blocks once one dies.
		 */
		if (calllist == PL_endav || !SvTRUE(ERRSV)) {
			I32 oldscope = PL_scopestack_ix;

			PUSHMARK(SP);
			call_sv(sv, G_EVAL|G_VOID|G_DISCARD);

			/* Ensure we return the same scope we started in. */
			while (PL_scopestack_ix > oldscope) {
				LEAVE;
			}
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
 * pperl_calllist_run_all() - Run all call list entries.
 *
 *	This routine is similar to pperl_calllist_run() except that all
 *	entries in the call list are run, no matter what package they are
 *	defined in.
 *
 *	@param	calllist	Perl call list to iterate over.
 *
 *	@post	ERRSV is true if any code block in the call list raised an
 *		exception.  The caller should check for this condition.
 *
 *	@post	The given call list will be empty on return.
 *
 *	@note	Must be called within an ENTER/LEAVE block. 
 *		pperl_setvars() should have already been called to setup
 *		the perl environment.
 */
void
pperl_calllist_run_all(AV *calllist)
{
	SV *sv;
	int max;
	dSP;

	/* Nothing to do if the call list is empty. */
	if (calllist == NULL || (max = av_len(calllist)) == -1)
		return;

	while (max >= 0) {

		/* Retrieve the next element in the call list array. */
		sv = av_shift(calllist);
		if (sv == NULL)
			continue;

		/* Check that it is a code reference. */
		assert (SvTYPE(sv) == SVt_PVCV);

		/*
		 * We always run all END code blocks, but for all other call
		 * lists we stop calling blocks once one dies.
		 */
		if (calllist == PL_endav || !SvTRUE(ERRSV)) {
			I32 oldscope = PL_scopestack_ix;

			PUSHMARK(SP);
			call_sv(sv, G_EVAL|G_VOID|G_DISCARD);

			/* Ensure we return the same scope we started in. */
			while (PL_scopestack_ix > oldscope) {
				LEAVE;
			}
		}

		/*
		 * Free the call list entry and adjust our record of the
		 * maximum array index to reflect reality.
		 */
		SvREFCNT_dec(sv);
		max--;
	}

	assert(av_len(calllist) == -1);
}
