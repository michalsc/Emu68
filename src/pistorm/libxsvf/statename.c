/*
 *  Lib(X)SVF  -  A library for implementing SVF and XSVF JTAG players
 *
 *  Copyright (C) 2009  RIEGL Research ForschungsGmbH
 *  Copyright (C) 2009  Clifford Wolf <clifford@clifford.at>
 *  
 *  Permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include "libxsvf.h"

const char *libxsvf_state2str(enum libxsvf_tap_state tap_state)
{
#define X(_s) if (tap_state == _s) return #_s;
	X(LIBXSVF_TAP_INIT)
	X(LIBXSVF_TAP_RESET)
	X(LIBXSVF_TAP_IDLE)
	X(LIBXSVF_TAP_DRSELECT)
	X(LIBXSVF_TAP_DRCAPTURE)
	X(LIBXSVF_TAP_DRSHIFT)
	X(LIBXSVF_TAP_DREXIT1)
	X(LIBXSVF_TAP_DRPAUSE)
	X(LIBXSVF_TAP_DREXIT2)
	X(LIBXSVF_TAP_DRUPDATE)
	X(LIBXSVF_TAP_IRSELECT)
	X(LIBXSVF_TAP_IRCAPTURE)
	X(LIBXSVF_TAP_IRSHIFT)
	X(LIBXSVF_TAP_IREXIT1)
	X(LIBXSVF_TAP_IRPAUSE)
	X(LIBXSVF_TAP_IREXIT2)
	X(LIBXSVF_TAP_IRUPDATE)
#undef X
	return "UNKOWN_STATE";
}

