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

#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <syslog.h>

#define	HAS_BOOL	/* We use stdbool's bool type rather than perl's. */
#include <EXTERN.h>
#include <XSUB.h>
#include <perl.h>

#include "pperl.h"
#include "pperl_private.h"


extern void __pperl_log(int priority, const char *fmt, ...)
		__attribute__ ((format (printf, 2, 3)));
extern void __pperl_logv(int priority, const char *fmt, va_list ap);
extern void __pperl_fatal(int eval, const char *fmt, ...)
		__attribute__ ((format (printf, 2, 3)));


/*!
 * @fn pperl_log(int priority, const char *fmt, ...)
 *
 * pperl_log() - Log a message.
 *
 *	This is an interval interface called by libpperl to log messages.
 *	The \a priority parameter specifies the severity of the message, the
 *	acceptable values are identical to those defined by syslog(3).
 *	The \a fmt string is identical to a printf()-style format string
 *	except that '%m' is expected to be replaced by an error message
 *	correponding to the current value of the errno global variable.
 *
 *	@note	pperl_log() is actually a weak linker symbol which references
 *		__pperl_log() by default.  If program linked against
 *		libpperl defines its own pperl_log() routine, the linker
 *		will cause the symbol to be remapped to that implementation
 *		instead.
 */
__weak_reference(__pperl_log, pperl_log);


/*!
 * @fn pperl_logv(int priority, const char *fmt, va_args ap)
 *
 * pperl_logv() - Log a message using stdarg(3) argument list.
 *
 *	This routine is identical to pperl_log() except that the arguments
 *	are taken from the given stdarg(3) argument list.  This routine is
 *	never called directly in libpperl, but rather is called by the
 *	default implementations of pperl_log() and pperl_fatal().
 *
 *	@note	pperl_logv() is actually a weak linker symbol which references
 *		__pperl_logv() by default.  If program linked against
 *		libpperl defines its own pperl_logv() routine, the linker
 *		will cause the symbol to be remapped to that implementation
 *		instead.  Since the default pperl_log() and pperl_fatal()
 *		implementations call pperl_logv() to do the actual logging,
 *		overriding pperl_logv() allows the application to define the
 *		logging behavior of libpperl.
 */
__weak_reference(__pperl_logv, pperl_logv);


/*!
 * @fn pperl_fatal(int eval, const char *fmt, ...)
 *
 * pperl_fatal() - Log a message recording a critical condition and exit.
 *
 *	This is an internal interface called by libpperl whenever a critical
 *	condition occurs that precludes the program from continuing (usually
 *	an out-of-memory condition).  It is always called with the global
 *	errno variable set to the cause of the critical condition.
 *	The \a eval argument is the recommended exit code, as defined in
 *	<sysexits.h>; the remaining arguments are a printf()-style format
 *	string and argument list.
 *
 *	@note	pperl_fatal() is actually a weak linker symbol which references
 *		__pperl_fatal() by default.  If program linked against
 *		libpperl defines its own pperl_fatal() routine, the linker
 *		will cause the symbol to be remapped to that implementation
 *		instead.
 */
__weak_reference(__pperl_fatal, pperl_fatal);


/*!
 * __pperl_log() - Default wrapper for pperl_logv().
 *
 *	@see pperl_log(), pperl_logv()
 */
void
__pperl_log(int priority, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	pperl_logv(priority, fmt, ap);
	va_end(ap);
}


/*!
 * __pperl_logv() - Default logging implementation using vsyslog(3).
 *
 *	@see pperl_logv()
 */
void
__pperl_logv(int priority, const char *fmt, va_list ap)
{
	vsyslog(priority, fmt, ap);
}


/*!
 * __pperl_fatal() - Default critical condition logging implementation.
 *
 *	Logs the specified error message by simply passing it to pperl_logv()
 *	before calling exit(3) to terminate the program with the specified
 *	exit code.
 *
 *	@see pperl_fatal(), pperl_logv()
 */
void
__pperl_fatal(int eval, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	pperl_logv(LOG_CRIT, fmt, ap);
	va_end(ap);

	exit(eval);
}


