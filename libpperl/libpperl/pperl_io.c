/*
 * Copyright (c) 2004 NTT Multimedia Communications Laboratories, Inc.
 * All rights reserved 
 *
 * Redistribution and use in source and/or binary forms of 
 * this software, with or without modification, are prohibited. 
 * Detailed license terms appear in the file named "COPYRIGHT".
 *
 * $NTTMCL$
 */

#include <sys/types.h>
#include <sys/queue.h>

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#define	HAS_BOOL	/* We use stdbool's bool type rather than perl's. */
#include <EXTERN.h>
#include <perl.h>
#include <perliol.h>

#include "log.h"
#include "pperl.h"
#include "pperl_private.h"


static IV	 ntt_PerlIO_pushed(pTHX_ PerlIO *f, const char *mode, SV *arg,
				   PerlIO_funcs *tab);
static PerlIO	*ntt_PerlIO_open(pTHX_ PerlIO_funcs *self,
				 PerlIO_list_t *layers, IV n, const char *mode,
				 int fd, int imode, int perm, PerlIO *f,
				 int narg, SV **args);
static IV	 ntt_PerlIO_close(pTHX_ PerlIO *f);
static SSize_t	 ntt_PerlIO_read(pTHX_ PerlIO *f, void *vbuf, Size_t count);
static SSize_t	 ntt_PerlIO_write(pTHX_ PerlIO *f, const void *vbuf,
				  Size_t count);


/*
 * Layer per-instance data.
 */
struct pperl_io_layer {
	struct _PerlIO		 pil_base;
	struct perlio		*pil_pio;
};


/*
 * Table of PerlIO methods for our layer.
 */
static PerlIO_funcs ntt_pperl_io_funcs = {
	.fsize		= sizeof(PerlIO_funcs),
	.name		= ignoreconst(PPERL_IOLAYER),
	.size		= sizeof(struct pperl_io_layer),
	.kind		= PERLIO_K_RAW,
	.Pushed		= ntt_PerlIO_pushed,
	.Popped		= PerlIOBase_popped,
	.Open		= ntt_PerlIO_open,
	.Binmode	= PerlIOBase_binmode,
	.Getarg		= NULL,
	.Fileno		= PerlIOBase_noop_fail,
	.Dup		= NULL,
	.Read		= ntt_PerlIO_read,
	.Unread		= PerlIOBase_unread,
	.Write		= ntt_PerlIO_write,
	.Seek		= NULL,
	.Tell		= NULL,
	.Close		= ntt_PerlIO_close,
	.Flush		= NULL,
	.Fill		= PerlIOBase_noop_fail,
	.Eof		= PerlIOBase_eof,
	.Error		= PerlIOBase_error,
	.Clearerr	= PerlIOBase_clearerr,
	.Setlinebuf	= PerlIOBase_setlinebuf,
	.Get_base	= NULL,
	.Get_bufsiz	= NULL,
	.Get_ptr	= NULL,
	.Get_cnt	= NULL,
	.Set_ptrcnt	= NULL
};


/*!
 * ntt_pperl_io_init() - Initialize support for intercepting I/O requests.
 *
 *	Defines a new PerlIO layer for providing callbacks for intercepting
 *	reads and writes to I/O handles.
 */
void
ntt_pperl_io_init(void)
{
	dTHX;

	PerlIO_define_layer(aTHX_ &ntt_pperl_io_funcs);
}


/*!
 * ntt_PerlIO_pushed() - PerlIO callback called whenever our I/O layer is
 *			 applied to a I/O handle.
 *
 *	See perliol(1) for a description of the callback interface and
 *	arguments.  We pass the address of the perlio structure allocated
 *	in ntt_pperl_io_override() via the \a arg argument.  Once we extract
 *	this address and store it in the layer's per-instance data, we call
 *	the PerlIO base class's pushed() method to perform the remainder of
 *	the expected functionality.
 *
 *	That is to say, this routine is simply a wrapper around the base
 *	class's pushed() method which records the pointer to the perlio
 *	structure for future reference.
 */
IV
ntt_PerlIO_pushed(pTHX_ PerlIO *f, const char *mode, SV *arg,
		  PerlIO_funcs *tab)
{
	IV code;
	struct pperl_io_layer *layer = PerlIOSelf(f, struct pperl_io_layer);
	struct perlio *pio;

	/*
	 * Ensure that no arguments were passed to our layer.
	 */
	if (arg == NULL) {
		Perl_croak(aTHX_ "argument required for ntt_perl layer");
		errno = EINVAL;
		return (-1);
	}

	/* Link the layer to the perlio structure. */
	pio = (void *)(intptr_t)SvIV(arg);
	pio->pio_f = f;
	layer->pil_pio = pio;

	/*
	 * The base class's pushed method can do the rest.
	 */
	code = PerlIOBase_pushed(aTHX_ f, mode, Nullsv, tab);

	return (code);
}


/*!
 * ntt_PerlIO_open() - PerlIO callback for opening an I/O handle with our layer.
 *
 *	See perliol(1) for a description of the callback interface and
 *	arguments.  We close the handle if it was already open (reopen()
 *	semantics) and then call PerlIO_push (which then calls our
 *	ntt_PerlIO_pushed() callback) to complete the open.
 */
PerlIO *
ntt_PerlIO_open(pTHX_
		PerlIO_funcs *self,
		PerlIO_list_t *layers __unused,
		IV n __unused,
		const char *mode,
		int fd __unused,
		int imode __unused,
		int perm __unused,
		PerlIO *old,
		int narg __unused,
		SV **args)
{
	PerlIO *f;

	/*
	 * If there was an old I/O handle, then re-use it (re-open semantics).
	 */
	if (old != NULL)
		f = old;
	else
		f = PerlIO_allocate(aTHX);

	f = PerlIO_push(aTHX_ f, self, mode, args[0]);
	if (f != NULL)
		PerlIOBase(f)->flags |= PERLIO_F_OPEN;

	return (f);
}


/*!
 * ntt_PerlIO_close() - PerlIO callback for closing an I/O handle.
 *
 *	See perliol(1) for a description of the callback interface and
 *	arguments.  Invokes any caller-specified on-close callback for the
 *	I/O handle before closing the handle and freeing our perlio structure.
 */
IV
ntt_PerlIO_close(pTHX_ PerlIO *f)
{
	struct pperl_io_layer *layer = PerlIOSelf(f, struct pperl_io_layer);
	struct perlio *pio = layer->pil_pio;
	IV code;

	if (pio->pio_onClose != NULL)
		pio->pio_onClose(pio->pio_data);

	code = PerlIOBase_close(f);

	ntt_pperl_io_destroy(&pio);

	return (code);
}


/*!
 * ntt_PerlIO_read() - PerlIO callback for reading from an I/O handle.
 *
 *	See perliol(1) for a description of the callback interface and
 *	arguments.  Passes the request to the caller-specified on-read
 *	callback to be serviced.  The callback should fill \a vbuf with
 *	up to \a count bytes of data and return the number of bytes
 *	written into the buffer.
 */
SSize_t
ntt_PerlIO_read(pTHX_ PerlIO *f, void *vbuf, Size_t count)
{
	struct pperl_io_layer *layer = PerlIOSelf(f, struct pperl_io_layer);
	struct perlio *pio = layer->pil_pio;

	assert(pio->pio_onRead != NULL);
	return pio->pio_onRead(vbuf, count, pio->pio_data);
}


/*!
 * ntt_PerlIO_write() - PerlIO callback for writing to an I/O handle.
 *
 *	See perliol(1) for a description of the callback interface and
 *	arguments.  Passes the request to the caller-specified on-write
 *	callback to be serviced.  The callback can consume up to \a count
 *	bytes of data from \a vbuf; the return value of the callback should
 *	be the actual number of bytes consumed.
 */
SSize_t
ntt_PerlIO_write(pTHX_ PerlIO *f, const void *vbuf, Size_t count)
{
	struct pperl_io_layer *layer = PerlIOSelf(f, struct pperl_io_layer);
	struct perlio *pio = layer->pil_pio;

	assert(pio->pio_onWrite != NULL);
	return pio->pio_onWrite(vbuf, count, pio->pio_data);
}


/*!
 * ntt_pperl_io_override() - Intercept I/O for a perl I/O handle.
 *
 *	Allows a persistent perl interpreter to override the read and write
 *	functions of a perl I/O handle such that it can provide its own
 *	implementation of those functions.  For example, writes to the STDERR
 *	handle may be redirected to syslog(3) or other logging library by
 *	providing the onWrite callback.
 *
 *	@param	interp		The persistent perl interpreter to create or
 *				override the I/O handle in.
 *
 *	@param	name		The name of the I/O handle to create/override
 *				as seen from perl scripts.  If a handle with
 *				the given name already exists, it will first
 *				be closed and then re-opened.
 *
 *	@param	onRead		Function to call whenever a perl script
 *				attempts to read from the I/O handle.  If
 *				NULL, the I/O handle will be write-only (both
 *				\a onRead and \a onWrite cannot be NULL).
 *				The \a onRead callback should fill the passed
 *				\a buf buffer with up to \a buflen bytes of
 *				data to be read by the perl script; the exact
 *				number of bytes written to the buffer should
 *				be returned by the callback.
 *
 *	@param	onWrite		Function to call whenever a perl script
 *				attempts to write to the I/O handle.  If NULL,
 *				the I/O handle will be read-only (both
 *				\a onRead and \a onWrite cannot be NULL).
 *				The \a onWrite callback can consume up to
 *				\a buflen bytes from the \a buf buffer.  The
 *				exact number of bytes consumed should be
 *				returned by the callback.
 *
 *	@param	onClose		Function to call when the I/O handle is closed.
 *				May be NULL.
 *
 *	@param	data		Opaque data passed to the function callbacks
 *				specified by \a onRead, \a onWrite, and
 *				\a onClose when they are invoked.
 */
void
ntt_pperl_io_override(perlinterp_t interp, const char *name,
		      ntt_pperl_io_read_t *onRead,
		      ntt_pperl_io_write_t *onWrite,
		      ntt_pperl_io_close_t *onClose,
		      intptr_t data)
{
	struct perlio *pio;
	const char *openstr;
	GV *handle;
	SV *sv;
	dTHX;

	assert(onRead != NULL || onWrite != NULL);

	if (onRead != NULL && onWrite != NULL)
		openstr = "+<:" PPERL_IOLAYER;
	else if (onRead != NULL)
		openstr = "<:" PPERL_IOLAYER;
	else
		openstr = ">:" PPERL_IOLAYER;

	/*
	 * Allocate and populate perlio structure.
	 */
	pio = malloc(sizeof(*pio));
	if (pio == NULL)
		fatal(EX_OSERR, "malloc: %m");
	pio->pio_onRead = onRead;
	pio->pio_onWrite = onWrite;
	pio->pio_onClose = onClose;
	pio->pio_data = data;
	pio->pio_f = NULL;
	pio->pio_interp = interp;
	LIST_INSERT_HEAD(&interp->pi_io_head, pio, pio_link);

	handle = gv_fetchpv(name, TRUE, SVt_PVIO);

	/*
	 * Create a perl scalar to pass the address of the perlio structure
	 * through Perl_do_open9() abstraction to ntt_PerlIO_open().
	 */
	sv = sv_newmortal();
	sv_setiv(sv, (IV)(intptr_t)pio);

	/*
	 * Close the handle if it already exists and is open.
	 */
	if (handle != NULL && SvTYPE(handle) == SVt_PVGV &&
	    IoTYPE(GvIO(handle)) != IoTYPE_CLOSED)
		Perl_do_close(aTHX_ handle, TRUE);

	if (!Perl_do_open9(aTHX_ handle, ignoreconst(openstr), strlen(openstr),
			   FALSE, O_WRONLY, 0, Nullfp, sv, 1)) {
		ntt_log(NTT_LOG_ERROR, "failed to open I/O handle %s: %s",
			name, SvPV(get_sv("!", TRUE), PL_na));
		return;
	}

	IoFLAGS(GvIOp(handle)) &= ~IOf_FLUSH;
}


/*
 * ntt_pperl_io_destroy() - Free a perlio structure.
 *
 *	Internal routine to free a perlio structure.  Called by
 *	ntt_PerlIO_close() when a handle with our layer is closed by a perl
 *	script.  Also called by ntt_pperl_destroy() when destroying a perl
 *	interpreter to free memory allocated to I/O handles in the interpreter
 *	being destroyed.
 *
 *	@param	piop		Pointer to perlio structure to free.  If the
 *				perlio structure refers to an open I/O handle,
 *				the I/O handle is also closed.
 *
 *	@post	*piop is set to NULL.
 */
void
ntt_pperl_io_destroy(perlio_t *piop)
{
	perlio_t pio = *piop;
	PerlIO *f = pio->pio_f;

	*piop = NULL;

	/*
	 * PerlIO_close() will call ntt_PerlIO_close() which will then
	 * recursively call this routine.  We ignore the recursive call by
	 * checking the PERLIO_F_OPEN flag which is cleared by PerlIO_close().
	 */
	if (f == NULL ||
	    (PerlIOBase(f)->flags & PERLIO_F_OPEN) == 0)
		return;

	PerlIO_close(f);

	pio->pio_f = NULL;
	pio->pio_interp = NULL;
	LIST_REMOVE(pio, pio_link);

	free(pio);
}
