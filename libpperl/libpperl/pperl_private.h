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

#ifndef _INCLUDE_LIBPPERL_PRIVATE_
#define _INCLUDE_LIBPPERL_PRIVATE_

/*
 * Requires <sys/types.h>, <sys/queue.h>, <perl.h>, and "pperl.h" to be
 * included before this file.
 */


/*!
 * @file
 *
 * Internal data structures and private functions for persistent perl
 * implementation.  This header MUST not be installed nor included by
 * applications.
 */


#define	PPERL_NAMESPACE "libpperl::_private"
#define	PPERL_IOLAYER	"pperl"


/* Macro for removing const poisoning.  Use with extreme caution. */
#define	ignoreconst(exp)	((void *)(intptr_t)(exp))



/*!
 * @struct perlinterp
 * @internal
 *
 * Data structure representing a persistent perl interpreter.
 *
 * Intended to abstract details of maintaining a persistent perl interpreter
 * so the caller does not need any of the detailed knowledge of perl that
 * would otherwise be required to perform even simple tasks with a persistent
 * per interpreter.
 *
 *	@param	pi_perl		The perl interpreter itself.
 *
 *	@param	pi_alloc_argv	Memory allocated to hold fake argv passed to
 *				perl_parse(); we have to allocate the fake argv
 *				array on the heap to avoid attempts to modify
 *				$0 from crashing the program.
 *
 *	@param	pi_args_head	Linked-list of perlargs structures so we can
 *				free them when pperl_destroy() is called.
 *
 *	@param	pi_code_head	Linked-list of perlcode structures so we can
 *				free them when pperl_destroy() is called.
 *
 *	@param	pi_env_head	Linked-list of perlenv structures so we can
 *				free them when pperl_destroy() is called.
 *
 *	@param	pi_io_head	Linked-list of perlio structures so we can
 *				free them when pperl_destroy() is called.
 */
struct perlinterp {
	PerlInterpreter		 *pi_perl;
	char			**pi_alloc_argv;
	LIST_HEAD(, perlargs)	  pi_args_head;
	LIST_HEAD(, perlcode)	  pi_code_head;
	LIST_HEAD(, perlenv)	  pi_env_head;
	LIST_HEAD(, perlio)	  pi_io_head;
};


/*!
 * @struct perlcode
 * @internal
 *
 * Data structure representing compiled perl code.
 *
 *	@param	pc_interp	Back-pointer to perl interpreter used to
 *				compile the code in.  We use this to ensure
 *				that we always execute the code in the same
 *				interpreter it was compiled in.  This allows
 *				the calling program to maintain multiple
 *				interpreter instances without having to jump
 *				through hoops to use them.
 *
 *	@param	pc_sv		Perl reference to the anonymous subroutine
 *				representing the compiled code.  See comments
 *				in pperl_compile() for details.
 *
 *	@param	pc_name		Name associated with the code.  This is used
 *				for reporting error messages and is the
 *				initial value of $0 when the code is executed.
 *
 *	@param	pc_pkgid	Unique number for identifying compiled code.
 *				This is used internally for creating a unique
 *				namespace for each piece of code compiled
 *				within a single interpreter.  See
 *				pperl_compile() for details.
 *
 *	@param	pc_pkgstash	Perl package code was compiled and executes in.
 *
 *	@param	pc_link		Link in linked list of perlcode structures
 *				associated with the compiling interpreter.
 */
struct perlcode {
	perlinterp_t		  pc_interp;

	SV			 *pc_sv;
	char			 *pc_name;
	u_int			  pc_pkgid; 
	HV			 *pc_pkgstash;

	LIST_ENTRY(perlcode)	  pc_link;
};


/*!
 * @struct perlargs
 * @internal
 *
 * Abstract data type for representing argument list passed to perl code
 * as \@ARGV array.
 *
 * This could be implemented more simply as a perl array except that there is
 * currently a speed advantage for not doing so.  If perl ever provides
 * copy-on-write semantics for its arrays, then it would probably be worth   
 * reimplementing as a perl array.  As it is, by using an abstract interface,
 * we are free to change the implementation in the future without changing the
 * API.
 *
 *	@param	pa_interp	Perl interpreter \@ARGV array is created in.
 *
 *	@param	pa_tainted	Whether or not to set the TAINTED flag on
 *				the elements of perl's \@ARGV array.
 *
 *	@param	pa_argc		The number of arguments in the list.
 *
 *	@param	pa_arglenv	Array of argument lengths.
 *
 *	@param	pa_strbuf	Buffer for holding argument strings.  The
 *				strings are concatenated in this storage buffer
 *				with the \a pa_arglenv used to determine where
 *				each argument ends.
 *
 *	@param	pa_arglenv_size	The number of elements the \a pa_arglenv array
 *				can currently hold.
 *
 *	@param	pa_strbuf_size	The number of bytes the \a pa_strbuf buffer can
 *				currently hold.
 *
 *	@param	pa_strbuf_len	The number of bytes used in the \a pa_strbuf
 *				buffer.
 *
 *	@param	pa_link		Link in linked list of perlargs structures
 *				for the parent interpreter.
 */
struct perlargs {
	perlinterp_t	  pa_interp;

	bool		  pa_tainted;
	int		  pa_argc;   
	size_t		 *pa_arglenv;
	char		 *pa_strbuf; 

	int		  pa_arglenv_size;	/* Size of arglenv array. */
	size_t		  pa_strbuf_size;	/* Size of strbuf allocation. */
	size_t		  pa_strbuf_len;	/* Used portion of strbuf. */   

	LIST_ENTRY(perlargs) pa_link;
};


/*!
 * @struct perlenv
 * @internal
 *
 *	Abstract data type for representing environment variable list passed
 *	to perl code as the \%ENV hash.
 *
 *	This is implemented using perl's own hash data structure.  This hash
 *	is duplicated each time perl code is run so that the original remains
 *	unmodified.
 *
 *	This could be further improved by duplicating the hash into a named
 *	variable inside the libpperl::_private namespace, reassigning the  
 *	\%ENV global to that hash (like we do for "exit"), and adding magic to
 *	mark the named hash as dirty if it gets modified.  If such logic were
 *	implemented, we would only have to rebuild the named hash from the   
 *	original if a script modified \%ENV, which should be fairly rare.     
 *	However, this has not been implemented yet.
 *
 *	@param	pe_interp	The interpreter this environment list is
 *				associated with.
 *
 *	@param	pe_envhash	Perl hash holding the environment variables.
 *
 *	@param	pe_tainted	Whether or not to set the TAINTED flag on the
 *				elements of perl's \%ENV hash.
 *
 *	@param	pe_link		Link in linked list of perlenv structures
 *				for the parent interpreter.
 */
struct perlenv {
	perlinterp_t	  pe_interp;
	HV		 *pe_envhash;
	bool		  pe_tainted;

	LIST_ENTRY(perlenv) pe_link;
};


extern void	 pperl_args_populate(perlargs_t pargs);
extern void	 pperl_env_populate(perlenv_t penv);


/*!
 * @struct perlio
 * @internal
 *
 *	Abstract data type for representing a perl I/O handle.
 *
 *	@param	pio_onRead	Callback, if any, to be invoked whenever a
 *				perl script reads from the I/O handle.
 *
 *	@param	pio_onWrite	Callback, if any, to be invoked whenever a
 *				perl script writes to the I/O handle.
 *
 *	@param	pio_onClose	Callback, if any, to be invoked when the I/O
 *				handle is closed.
 *
 *	@param	pio_data	Opaque data passed to callbacks when they are
 *				invoked.
 *
 *	@param	pio_f		The PerlIO structure representing the perl I/O
 *				handle.
 *
 *	@param	pio_interp	The persistent perl interpreter the handle
 *				exists in.
 *
 *	@param	pio_link	Link in linked list of perlio structures for
 *				the parent interpreter.
 */
struct perlio {
	pperl_io_read_t		*pio_onRead; 
	pperl_io_write_t	*pio_onWrite;
	pperl_io_close_t	*pio_onClose;

	intptr_t		 pio_data;

	PerlIO			*pio_f;
	perlinterp_t		 pio_interp;
	LIST_ENTRY(perlio)	 pio_link;
};

extern void	 pperl_io_init(void);
extern void	 pperl_io_destroy(perlio_t *piop);

extern void	 pperl_calllist_clear(AV *calllist, const HV *pkgstash);
extern void	 pperl_calllist_run(AV *calllist, const HV *pkgstash);

extern void	 pperl_seterr(int errnum, struct perlresult *result);

extern void	*pperl_malloc(size_t size);
extern void	*pperl_realloc(void *ptr, size_t size);
extern char	*pperl_strdup(const char *str);

#endif
