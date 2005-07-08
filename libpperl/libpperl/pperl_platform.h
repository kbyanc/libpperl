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

#ifndef _INCLUDE_LIBPPERL_PLATFORM_
#define _INCLUDE_LIBPPERL_PLATFORM_

/*!
 * @file
 *
 * This header is the first thing included in any of the libpperl source
 * files.  Platform-specific issues should be dealt with here and not within
 * individual source files.
 */

#if HAVE_CONFIG_H
#  include <config.h>	/* autoconf */
#endif

#if HAVE_SYS_CDEFS_H
#  include <sys/cdefs.h>
#endif


#undef __BEGIN_DECLS
#undef __END_DECLS

#if defined(__cplusplus)
#  define	__BEGIN_DECLS	extern "C" {
#  define	__END_DECLS	}
#else
#  define	__BEGIN_DECLS
#  define	__END_DECLS
#endif


/*
 * C99 requires va_copy().  Older versions of GCC provide __va_copy.
 * Per the autoconf manual, test for the availability of these using
 * #ifdef, falling back to memcpy() if neither is defined.
 */
#ifndef va_copy
#  ifdef __va_copy
#    define	va_copy(d, s)	__va_copy(d, s)
#  else
#    define	va_copy(d, s)	memcpy(&(d), &(s), sizeof(va_list))
#  endif
#endif

#if !(__GNUC__ == 2 && __GNUC_MINOR__ >= 7 || __GNUC__ >= 3 || defined(__INTEL_COMPILER))
#  define	__attribute__(args)
#endif

#ifndef __unused
#  define	__unused	__attribute__((__unused__))
#endif

#ifndef __printflike
#  define	__printflike(fmtarg, firstvararg) \
		__attribute__((__format__ (__printf__, fmtarg, firstvararg)))
#endif


#endif /* !_INCLUDE_LIBPPERL_PLATFORM_ */
