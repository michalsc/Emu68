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

static void tap_transition(struct libxsvf_host *h, int v)
{
	LIBXSVF_HOST_PULSE_TCK(v, -1, -1, 0, 0);
}

int libxsvf_tap_walk(struct libxsvf_host *h, enum libxsvf_tap_state s)
{
	int i, j;
	for (i=0; s != h->tap_state; i++)
	{
		switch (h->tap_state)
		{
		/* Special States */
		case LIBXSVF_TAP_INIT:
			for (j = 0; j < 6; j++)
				tap_transition(h, 1);
			h->tap_state = LIBXSVF_TAP_RESET;
			break;
		case LIBXSVF_TAP_RESET:
			tap_transition(h, 0);
			h->tap_state = LIBXSVF_TAP_IDLE;
			break;
		case LIBXSVF_TAP_IDLE:
			tap_transition(h, 1);
			h->tap_state = LIBXSVF_TAP_DRSELECT;
			break;

		/* DR States */
		case LIBXSVF_TAP_DRSELECT:
			if (s >= LIBXSVF_TAP_IRSELECT || s == LIBXSVF_TAP_RESET) {
				tap_transition(h, 1);
				h->tap_state = LIBXSVF_TAP_IRSELECT;
			} else {
				tap_transition(h, 0);
				h->tap_state = LIBXSVF_TAP_DRCAPTURE;
			}
			break;
		case LIBXSVF_TAP_DRCAPTURE:
			if (s == LIBXSVF_TAP_DRSHIFT) {
				tap_transition(h, 0);
				h->tap_state = LIBXSVF_TAP_DRSHIFT;
			} else {
				tap_transition(h, 1);
				h->tap_state = LIBXSVF_TAP_DREXIT1;
			}
			break;
		case LIBXSVF_TAP_DRSHIFT:
			tap_transition(h, 1);
			h->tap_state = LIBXSVF_TAP_DREXIT1;
			break;
		case LIBXSVF_TAP_DREXIT1:
			if (s == LIBXSVF_TAP_DRPAUSE) {
				tap_transition(h, 0);
				h->tap_state = LIBXSVF_TAP_DRPAUSE;
			} else {
				tap_transition(h, 1);
				h->tap_state = LIBXSVF_TAP_DRUPDATE;
			}
			break;
		case LIBXSVF_TAP_DRPAUSE:
			tap_transition(h, 1);
			h->tap_state = LIBXSVF_TAP_DREXIT2;
			break;
		case LIBXSVF_TAP_DREXIT2:
			if (s == LIBXSVF_TAP_DRSHIFT) {
				tap_transition(h, 0);
				h->tap_state = LIBXSVF_TAP_DRSHIFT;
			} else {
				tap_transition(h, 1);
				h->tap_state = LIBXSVF_TAP_DRUPDATE;
			}
			break;
		case LIBXSVF_TAP_DRUPDATE:
			if (s == LIBXSVF_TAP_IDLE) {
				tap_transition(h, 0);
				h->tap_state = LIBXSVF_TAP_IDLE;
			} else {
				tap_transition(h, 1);
				h->tap_state = LIBXSVF_TAP_DRSELECT;
			}
			break;

		/* IR States */
		case LIBXSVF_TAP_IRSELECT:
			if (s == LIBXSVF_TAP_RESET) {
				tap_transition(h, 1);
				h->tap_state = LIBXSVF_TAP_RESET;
			} else {
				tap_transition(h, 0);
				h->tap_state = LIBXSVF_TAP_IRCAPTURE;
			}
			break;
		case LIBXSVF_TAP_IRCAPTURE:
			if (s == LIBXSVF_TAP_IRSHIFT) {
				tap_transition(h, 0);
				h->tap_state = LIBXSVF_TAP_IRSHIFT;
			} else {
				tap_transition(h, 1);
				h->tap_state = LIBXSVF_TAP_IREXIT1;
			}
			break;
		case LIBXSVF_TAP_IRSHIFT:
			tap_transition(h, 1);
			h->tap_state = LIBXSVF_TAP_IREXIT1;
			break;
		case LIBXSVF_TAP_IREXIT1:
			if (s == LIBXSVF_TAP_IRPAUSE) {
				tap_transition(h, 0);
				h->tap_state = LIBXSVF_TAP_IRPAUSE;
			} else {
				tap_transition(h, 1);
				h->tap_state = LIBXSVF_TAP_IRUPDATE;
			}
			break;
		case LIBXSVF_TAP_IRPAUSE:
			tap_transition(h, 1);
			h->tap_state = LIBXSVF_TAP_IREXIT2;
			break;
		case LIBXSVF_TAP_IREXIT2:
			if (s == LIBXSVF_TAP_IRSHIFT) {
				tap_transition(h, 0);
				h->tap_state = LIBXSVF_TAP_IRSHIFT;
			} else {
				tap_transition(h, 1);
				h->tap_state = LIBXSVF_TAP_IRUPDATE;
			}
			break;
		case LIBXSVF_TAP_IRUPDATE:
			if (s == LIBXSVF_TAP_IDLE) {
				tap_transition(h, 0);
				h->tap_state = LIBXSVF_TAP_IDLE;
			} else {
				tap_transition(h, 1);
				h->tap_state = LIBXSVF_TAP_DRSELECT;
			}
			break;

		default:
			LIBXSVF_HOST_REPORT_ERROR("Illegal tap state.");
			return -1;
		}
		if (h->report_tapstate)
			LIBXSVF_HOST_REPORT_TAPSTATE();
		if (i>10) {
			LIBXSVF_HOST_REPORT_ERROR("Loop in tap walker.");
			return -1;
		}
	}

	return 0;
}
