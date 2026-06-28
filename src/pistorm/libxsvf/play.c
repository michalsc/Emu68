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

int libxsvf_play(struct libxsvf_host *h, enum libxsvf_mode mode)
{
	int rc = -1;

	h->tap_state = LIBXSVF_TAP_INIT;
	if (LIBXSVF_HOST_SETUP() < 0) {
		LIBXSVF_HOST_REPORT_ERROR("Setup of JTAG interface failed.");
		return -1;
	}

	if (mode == LIBXSVF_MODE_SVF) {
#ifdef LIBXSVF_WITHOUT_SVF
		LIBXSVF_HOST_REPORT_ERROR("SVF support in libxsvf is disabled.");
#else
		rc = libxsvf_svf(h);
#endif
	}

	if (mode == LIBXSVF_MODE_XSVF) {
#ifdef LIBXSVF_WITHOUT_XSVF
		LIBXSVF_HOST_REPORT_ERROR("XSVF support in libxsvf is disabled.");
#else
		rc = libxsvf_xsvf(h);
#endif
	}

	if (mode == LIBXSVF_MODE_SCAN) {
#ifdef LIBXSVF_WITHOUT_SCAN
		LIBXSVF_HOST_REPORT_ERROR("SCAN support in libxsvf is disabled.");
#else
		rc = libxsvf_scan(h);
#endif
	}

	if (mode == LIBXSVF_MODE_READ_USERCODE) {
#ifdef LIBXSVF_WITHOUT_READ_USERCODE
		LIBXSVF_HOST_REPORT_ERROR("READ_USERCODE support in libxsvf is disabled.");
#else
		rc = libxsvf_read_usercode(h);
#endif
	}

	libxsvf_tap_walk(h, LIBXSVF_TAP_RESET);
	if (LIBXSVF_HOST_SYNC() != 0 && rc >= 0 ) {
		LIBXSVF_HOST_REPORT_ERROR("TDO mismatch in TAP reset. (this is not possible!)");
		rc = -1;
	}

	int shutdown_rc = LIBXSVF_HOST_SHUTDOWN();

	if (shutdown_rc < 0) {
		LIBXSVF_HOST_REPORT_ERROR("Shutdown of JTAG interface failed.");
		rc = rc < 0 ? rc : shutdown_rc;
	}

	return rc;
}
