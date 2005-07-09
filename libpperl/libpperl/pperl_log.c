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

#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <syslog.h>

#define	HAS_BOOL	/* We use stdbool's bool type rather than perl's. */
#include <EXTERN.h>
#include <XSUB.h>
#include <perl.h>

#include "queue.h"

#include "pperl.h"
#include "pperl_private.h"


static void __pperl_fatal(int eval, const char *fmt, va_list ap);

void (*pperl_log_callback)(int, const char *, va_list) = vsyslog;
void (*pperl_fatal_callback)(int, const char *, va_list) = __pperl_fatal;


/*!
 * pperl_log() - Log a message.
 *
 *	This is an internal interface called by libpperl to log messages.
 *	The \a priority parameter specifies the severity of the message, the
 *	acceptable values are identical to those defined by syslog(3).
 *	The \a fmt string is identical to a printf()-style format string
 *	except that '%m' is expected to be replaced by an error message
 *	correponding to the current value of the errno global variable.
 */
void
pperl_log(int priority, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	pperl_logv(priority, fmt, ap);
	va_end(ap);
}


/*!
 * pperl_logv() - Log a message using stdarg(3) argument list.
 *
 *	This routine is identical to pperl_log() except that the arguments
 *	are taken from the given stdarg(3) argument list.  This routine is
 *	never called directly in libpperl, but rather is called by the
 *	default implementations of pperl_log() and pperl_fatal().
 */
void
pperl_logv(int priority, const char *fmt, va_list ap)
{
	pperl_log_callback(priority, fmt, ap);
}


/*!
 * pperl_fatal() - Log a message recording a critical condition and exit.
 *
 *	This is an internal interface called by libpperl whenever a critical
 *	condition occurs that precludes the program from continuing (usually
 *	an out-of-memory condition).  It is always called with the global
 *	errno variable set to the cause of the critical condition.
 *	The \a eval argument is the recommended exit code, as defined in
 *	<sysexits.h>; the remaining arguments are a printf()-style format
 *	string and argument list.
 */
void
pperl_fatal(int eval, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	pperl_fatal_callback(eval, fmt, ap);
	va_end(ap);

	/* Just in case the callback didn't do it itself. */
	exit(eval);
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
__pperl_fatal(int eval, const char *fmt, va_list ap)
{
	pperl_logv(LOG_CRIT, fmt, ap);
	exit(eval);
}
