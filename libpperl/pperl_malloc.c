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
 */

#include "pperl_platform.h"
#include <sys/types.h>

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <syslog.h>

#define HAS_BOOL	/* We use stdbool's bool type rather than perl's. */
#include <EXTERN.h>
#include <XSUB.h>
#include <perl.h>

#include "queue.h"

#include "pperl.h"
#include "pperl_private.h"


/*!
 * pperl_malloc() - Allocate memory.
 *
 *	This is an internal interface used by libpperl itself to allocate
 *	memory.  It is simply a wrapper around the system's malloc(3) routine
 *	that is guarantees to never return NULL.  If the system's malloc(3)
 *	returns NULL, pperl_malloc() calls pperl_fatal() to log the error
 *	and abort the program.
 *
 *	@param	size		The number of bytes of memory to allocate.
 */
void *
pperl_malloc(size_t size)
{
	void *alloc;

	assert(size > 0);	/* Should never try to allocate 0 bytes. */

	alloc = malloc(size);
	if (alloc != NULL)
		return alloc;

	pperl_fatal(EX_OSERR, "malloc: %m");
}


/*!
 * pperl_realloc() - Change size of memory allocation.
 *
 *	This is an internal interface used by libpperl itself to change the
 *	size of an existing memory allocation.  It is simply a wrapper around
 *	the system's realloc(3) routine that is guaranteed to never return
 *	NULL.  If the memory allocation cannot be resized as requested,
 *	calls pperl_fatal() to log the error and abort the program.
 *
 *	@param	ptr		Pointer to existing memory allocation.
 *
 *	@param	size		The new size in byte for the memory allocation.
 *
 *	@returns pointer to resized allocation; note that the returned pointer
 *		 may be different than the \a ptr of the existing allocation.
 */
void *
pperl_realloc(void *ptr, size_t size)
{
	void *newptr;

	assert(size > 0);	/* Should never try to allocate 0 bytes. */

	newptr = realloc(ptr, size);
	if (newptr != NULL)
		return newptr;

	pperl_fatal(EX_OSERR, "malloc: %m");
}


/*!
 * pperl_strdup() - Copy a string.
 *
 *	This is an internal interface used by libpperl itself to duplicate the
 *	contents of a string.  It is simply a wrapper around the system's
 *	strdup(3) routine that is guaranteed to never return NULL.  Instead,
 *	if pperl_strdup() fails to allocate memory for the new string it
 *	calls pperl_fatal() to log the error and abort the program.
 *
 *	@param	str		Pointer to nul-terminated string to copy.
 *
 *	@returns newly-allocated nul-terminated string consististing of
 *		 the same sequence of characters as the original.
 */
char *
pperl_strdup(const char *str)
{
	char *newstr;
	size_t len;

	len = strlen(str) + 1;
	newstr = pperl_malloc(len);
	memcpy(newstr, str, len);

	return newstr;
}
