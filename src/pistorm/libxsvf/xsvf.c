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

/* command codes as defined in xilinx xapp503 */
enum xsvf_cmd {
	XCOMPLETE       = 0x00,
	XTDOMASK        = 0x01,
	XSIR            = 0x02,
	XSDR            = 0x03,
	XRUNTEST        = 0x04,
	XREPEAT         = 0x07,
	XSDRSIZE        = 0x08,
	XSDRTDO         = 0x09,
	XSETSDRMASKS    = 0x0A,
	XSDRINC         = 0x0B,
	XSDRB           = 0x0C,
	XSDRC           = 0x0D,
	XSDRE           = 0x0E,
	XSDRTDOB        = 0x0F,
	XSDRTDOC        = 0x10,
	XSDRTDOE        = 0x11,
	XSTATE          = 0x12,
	XENDIR          = 0x13,
	XENDDR          = 0x14,
	XSIR2           = 0x15,
	XCOMMENT        = 0x16,
	XWAIT           = 0x17,
	/* Extensions used in svf2xsvf.py */
	XWAITSTATE      = 0x18,
	XTRST           = 0x1c
};

// This is to not confuse the VIM syntax highlighting
#define VAL_OPEN (
#define VAL_CLOSE )

#define READ_BITS(_buf, _len) do {                                          \
	unsigned char *_p = _buf; int _i;                                   \
	for (_i=0; _i<(_len); _i+=8) {                                      \
		int tmp = LIBXSVF_HOST_GETBYTE();                           \
		if (tmp < 0) {                                              \
			LIBXSVF_HOST_REPORT_ERROR("Unexpected EOF.");       \
			goto error;                                         \
		}                                                           \
		*(_p++) = tmp;                                              \
	}                                                                   \
} while (0)

#define READ_LONG() VAL_OPEN{                                               \
	long _buf = 0; int _i;                                              \
	for (_i=0; _i<4; _i++) {                                            \
		int tmp = LIBXSVF_HOST_GETBYTE();                           \
		if (tmp < 0) {                                              \
			LIBXSVF_HOST_REPORT_ERROR("Unexpected EOF.");       \
			goto error;                                         \
		}                                                           \
		_buf = _buf << 8 | tmp;                                     \
	}                                                                   \
	_buf;                                                               \
}VAL_CLOSE

#define READ_BYTE() VAL_OPEN{                                               \
	int _tmp = LIBXSVF_HOST_GETBYTE();                                  \
	if (_tmp < 0) {                                                     \
		LIBXSVF_HOST_REPORT_ERROR("Unexpected EOF.");               \
		goto error;                                                 \
	}                                                                   \
	_tmp;                                                               \
}VAL_CLOSE

#define SHIFT_DATA(_inp, _outp, _maskp, _len, _state, _estate, _edelay, _ret) do { \
	if (shift_data(h, _inp, _outp, _maskp, _len, _state, _estate, _edelay, _ret) < 0) { \
		goto error;                                                 \
	}                                                                   \
} while (0)

#define TAP(_state) do {                                                    \
	if (libxsvf_tap_walk(h, _state) < 0)                                \
		goto error;                                                 \
} while (0)

static int bits2bytes(int bits)
{
	return (bits+7) / 8;
}

static int getbit(unsigned char *data, int n)
{
	return (data[n/8] & (1 << (7 - n%8))) ? 1 : 0;
}

static void setbit(unsigned char *data, int n, int v)
{
	unsigned char mask = 1 << (7 - n%8);
	if (v)
		data[n/8] |= mask;
	else
		data[n/8] &= ~mask;
}

static int xilinx_tap(int state)
{
	/* state codes as defined in xilinx xapp503 */
	switch (state)
	{
	case 0x00:
		return LIBXSVF_TAP_RESET;
		break;
	case 0x01:
		return LIBXSVF_TAP_IDLE;
		break;
	case 0x02:
		return LIBXSVF_TAP_DRSELECT;
		break;
	case 0x03:
		return LIBXSVF_TAP_DRCAPTURE;
		break;
	case 0x04:
		return LIBXSVF_TAP_DRSHIFT;
		break;
	case 0x05:
		return LIBXSVF_TAP_DREXIT1;
		break;
	case 0x06:
		return LIBXSVF_TAP_DRPAUSE;
		break;
	case 0x07:
		return LIBXSVF_TAP_DREXIT2;
		break;
	case 0x08:
		return LIBXSVF_TAP_DRUPDATE;
		break;
	case 0x09:
		return LIBXSVF_TAP_IRSELECT;
		break;
	case 0x0A:
		return LIBXSVF_TAP_IRCAPTURE;
		break;
	case 0x0B:
		return LIBXSVF_TAP_IRSHIFT;
		break;
	case 0x0C:
		return LIBXSVF_TAP_IREXIT1;
		break;
	case 0x0D:
		return LIBXSVF_TAP_IRPAUSE;
		break;
	case 0x0E:
		return LIBXSVF_TAP_IREXIT2;
		break;
	case 0x0F:
		return LIBXSVF_TAP_IRUPDATE;
		break;
	}
	return -1;
}

static int shift_data(struct libxsvf_host *h, unsigned char *inp, unsigned char *outp, unsigned char *maskp, int len, enum libxsvf_tap_state state, enum libxsvf_tap_state estate, int edelay, int retries)
{
	int left_padding = (8 - len % 8) % 8;
	int with_retries = retries > 0;
	int i;

	if (with_retries && LIBXSVF_HOST_SYNC() < 0) {
		LIBXSVF_HOST_REPORT_ERROR("TDO mismatch.");
		return -1;
	}

	while (1)
	{
		int tdo_error = 0;
		int tms = 0;

		TAP(state);
		tms = 0;

		for (i=len+left_padding-1; i>=left_padding; i--) {
			if (i == left_padding && h->tap_state != estate) {
				h->tap_state++;
				tms = 1;
			}
			int tdi = getbit(inp, i);
			int tdo = -1;
			if (maskp && getbit(maskp, i))
				tdo = outp && getbit(outp, i);
			int sync = with_retries && i == left_padding;
			if (LIBXSVF_HOST_PULSE_TCK(tms, tdi, tdo, 0, sync) < 0)
				tdo_error = 1;
		}

		if (tms)
			LIBXSVF_HOST_REPORT_TAPSTATE();
	
		if (edelay) {
			TAP(LIBXSVF_TAP_IDLE);
			LIBXSVF_HOST_UDELAY(edelay, 0, edelay);
		} else {
			TAP(estate);
		}

		if (!tdo_error)
			return 0;

		if (retries <= 0) {
			LIBXSVF_HOST_REPORT_ERROR("TDO mismatch.");
			return -1;
		}

		retries--;
	}

error:
	return -1;
}

int libxsvf_xsvf(struct libxsvf_host *h)
{
	int rc = 0;
	int i, j;

	unsigned char *buf_tdi_data = (void*)0;
	unsigned char *buf_tdo_data = (void*)0;
	unsigned char *buf_tdo_mask = (void*)0;
	unsigned char *buf_addr_mask = (void*)0;
	unsigned char *buf_data_mask = (void*)0;

	long state_dr_size = 0;
	long state_data_size = 0;
	long state_runtest = 0;
	unsigned char state_xendir = 0;
	unsigned char state_xenddr = 0;
	unsigned char state_retries = 0;
	unsigned char cmd = 0;

	while (1)
	{
		unsigned char last_cmd = cmd;
		cmd = LIBXSVF_HOST_GETBYTE();

#define STATUS(_c) LIBXSVF_HOST_REPORT_STATUS("XSVF Command " #_c);

		switch (cmd)
		{
		case XCOMPLETE: {
			STATUS(XCOMPLETE);
			goto got_complete_command;
		  }
		case XTDOMASK: {
			STATUS(XTDOMASK);
			READ_BITS(buf_tdo_mask, state_dr_size);
			break;
		  }
		case XSIR: {
			STATUS(XSIR);
			int length = READ_BYTE();
			unsigned char buf[bits2bytes(length)];
			READ_BITS(buf, length);
			SHIFT_DATA(buf, (void*)0, (void*)0, length, LIBXSVF_TAP_IRSHIFT,
					state_xendir ? LIBXSVF_TAP_IRPAUSE : LIBXSVF_TAP_IDLE,
					state_runtest, state_retries);
			break;
		  }
		case XSDR: {
			STATUS(XSDR);
			READ_BITS(buf_tdi_data, state_dr_size);
			SHIFT_DATA(buf_tdi_data, buf_tdo_data, buf_tdo_mask, state_dr_size, LIBXSVF_TAP_DRSHIFT,
					state_xenddr ? LIBXSVF_TAP_DRPAUSE : LIBXSVF_TAP_IDLE,
					state_runtest, state_retries);
			break;
		  }
		case XRUNTEST: {
			STATUS(XRUNTEST);
			state_runtest = READ_LONG();
			break;
		  }
		case XREPEAT: {
			STATUS(XREPEAT);
			state_retries = READ_BYTE();
			break;
		  }
		case XSDRSIZE: {
			STATUS(XSDRSIZE);
			state_dr_size = READ_LONG();
			buf_tdi_data = LIBXSVF_HOST_REALLOC(buf_tdi_data, bits2bytes(state_dr_size), LIBXSVF_MEM_XSVF_TDI_DATA);
			buf_tdo_data = LIBXSVF_HOST_REALLOC(buf_tdo_data, bits2bytes(state_dr_size), LIBXSVF_MEM_XSVF_TDO_DATA);
			buf_tdo_mask = LIBXSVF_HOST_REALLOC(buf_tdo_mask, bits2bytes(state_dr_size), LIBXSVF_MEM_XSVF_TDO_MASK);
			buf_addr_mask = LIBXSVF_HOST_REALLOC(buf_addr_mask, bits2bytes(state_dr_size), LIBXSVF_MEM_XSVF_ADDR_MASK);
			buf_data_mask = LIBXSVF_HOST_REALLOC(buf_data_mask, bits2bytes(state_dr_size), LIBXSVF_MEM_XSVF_DATA_MASK);
			if (!buf_tdi_data || !buf_tdo_data || !buf_tdo_mask || !buf_addr_mask || !buf_data_mask) {
				LIBXSVF_HOST_REPORT_ERROR("Allocating memory failed.");
				goto error;
			}
			break;
		  }
		case XSDRTDO: {
			STATUS(XSDRTDO);
			READ_BITS(buf_tdi_data, state_dr_size);
			READ_BITS(buf_tdo_data, state_dr_size);
			SHIFT_DATA(buf_tdi_data, buf_tdo_data, buf_tdo_mask, state_dr_size, LIBXSVF_TAP_DRSHIFT,
					state_xenddr ? LIBXSVF_TAP_DRPAUSE : LIBXSVF_TAP_IDLE,
					state_runtest, state_retries);
			break;
		  }
		case XSETSDRMASKS: {
			STATUS(XSETSDRMASKS);
			READ_BITS(buf_addr_mask, state_dr_size);
			READ_BITS(buf_data_mask, state_dr_size);
			state_data_size = 0;
			for (i=0; i<state_dr_size; i++)
				state_data_size += getbit(buf_data_mask, i);
			break;
		  }
		case XSDRINC: {
			STATUS(XSDRINC);
			READ_BITS(buf_tdi_data, state_dr_size);
			int num = READ_BYTE();
			while (1) {
				SHIFT_DATA(buf_tdi_data, buf_tdo_data, buf_tdo_mask, state_dr_size, LIBXSVF_TAP_DRSHIFT,
						state_xenddr ? LIBXSVF_TAP_DRPAUSE : LIBXSVF_TAP_IDLE,
						state_runtest, state_retries);
				if (num-- <= 0)
					break;
				int carry = 1;
				for (i=state_dr_size-1; i>=0; i--) {
					if (getbit(buf_addr_mask, i) == 0)
						continue;
					if (getbit(buf_tdi_data, i)) {
						setbit(buf_tdi_data, i, !carry);
					} else {
						setbit(buf_tdi_data, i, carry);
						carry = 0;
					}
				}
				unsigned char this_byte = 0;
				for (i=0, j=0; i<state_data_size; i++) {
					if (i%8 == 0)
						this_byte = READ_BYTE();
					while (getbit(buf_data_mask, j) == 0)
						j++;
					setbit(buf_tdi_data, j++, getbit(&this_byte, i%8));
				}
			}
			break;
		  }
		case XSDRB: {
			STATUS(XSDRB);
			READ_BITS(buf_tdi_data, state_dr_size);
			SHIFT_DATA(buf_tdi_data, (void*)0, (void*)0, state_dr_size, LIBXSVF_TAP_DRSHIFT, LIBXSVF_TAP_DRSHIFT, 0, 0);
			break;
		  }
		case XSDRC: {
			STATUS(XSDRC);
			READ_BITS(buf_tdi_data, state_dr_size);
			SHIFT_DATA(buf_tdi_data, (void*)0, (void*)0, state_dr_size, LIBXSVF_TAP_DRSHIFT, LIBXSVF_TAP_DRSHIFT, 0, 0);
			break;
		  }
		case XSDRE: {
			STATUS(XSDRE);
			READ_BITS(buf_tdi_data, state_dr_size);
			SHIFT_DATA(buf_tdi_data, (void*)0, (void*)0, state_dr_size, LIBXSVF_TAP_DRSHIFT,
					state_xenddr ? LIBXSVF_TAP_DRPAUSE : LIBXSVF_TAP_IDLE, 0, 0);
			break;
		  }
		case XSDRTDOB: {
			STATUS(XSDRTDOB);
			READ_BITS(buf_tdi_data, state_dr_size);
			READ_BITS(buf_tdo_data, state_dr_size);
			SHIFT_DATA(buf_tdi_data, buf_tdo_data, (void*)0, state_dr_size, LIBXSVF_TAP_DRSHIFT, LIBXSVF_TAP_DRSHIFT, 0, 0);
			break;
		  }
		case XSDRTDOC: {
			STATUS(XSDRTDOC);
			READ_BITS(buf_tdi_data, state_dr_size);
			READ_BITS(buf_tdo_data, state_dr_size);
			SHIFT_DATA(buf_tdi_data, buf_tdo_data, (void*)0, state_dr_size, LIBXSVF_TAP_DRSHIFT, LIBXSVF_TAP_DRSHIFT, 0, 0);
			break;
		  }
		case XSDRTDOE: {
			STATUS(XSDRTDOE);
			READ_BITS(buf_tdi_data, state_dr_size);
			READ_BITS(buf_tdo_data, state_dr_size);
			SHIFT_DATA(buf_tdi_data, buf_tdo_data, (void*)0, state_dr_size, LIBXSVF_TAP_DRSHIFT,
					state_xenddr ? LIBXSVF_TAP_DRPAUSE : LIBXSVF_TAP_IDLE, 0, 0);
			break;
		  }
		case XSTATE: {
			STATUS(XSTATE);
			if (state_runtest && last_cmd == XRUNTEST) {
				TAP(LIBXSVF_TAP_IDLE);
				LIBXSVF_HOST_UDELAY(state_runtest, 0, state_runtest);
			}
			unsigned char state = READ_BYTE();
			TAP(xilinx_tap(state));
			break;
		  }
		case XENDIR: {
			STATUS(XENDIR);
			state_xendir = READ_BYTE();
			break;
		  }
		case XENDDR: {
			STATUS(XENDDR);
			state_xenddr = READ_BYTE();
			break;
		  }
		case XSIR2: {
			STATUS(XSIR2);
			int length = READ_BYTE();
			length = length << 8 | READ_BYTE();
			unsigned char buf[bits2bytes(length)];
			READ_BITS(buf, length);
			SHIFT_DATA(buf, (void*)0, (void*)0, length, LIBXSVF_TAP_IRSHIFT,
					state_xendir ? LIBXSVF_TAP_IRPAUSE : LIBXSVF_TAP_IDLE,
					state_runtest, state_retries);
			break;
		  }
		case XCOMMENT: {
			STATUS(XCOMMENT);
			unsigned char this_byte;
			do {
				this_byte = READ_BYTE();
			} while (this_byte);
			break;
		  }
		case XWAIT:
		case XWAITSTATE: {
			STATUS(XWAIT);
			unsigned char state1 = READ_BYTE();
			unsigned char state2 = READ_BYTE();
			long usecs = READ_LONG();
			TAP(xilinx_tap(state1));
			LIBXSVF_HOST_UDELAY(usecs, 0, 0);
			TAP(xilinx_tap(state2));
			if (cmd==XWAITSTATE) {
				READ_LONG();   /* XWAITSTATE has count, time arguments */
			}
			break;
		  }
		case XTRST: {
			STATUS(XTRST);
			READ_BYTE();  /* enum: ON, OFF, Z, ABSENT */
			break;
		}
		default:
			LIBXSVF_HOST_REPORT_ERROR("Unknown XSVF command.");
			goto error;
		}
	}

error:
	rc = -1;

got_complete_command:
	if (LIBXSVF_HOST_SYNC() != 0 && rc >= 0 ) {
		LIBXSVF_HOST_REPORT_ERROR("TDO mismatch.");
		rc = -1;
	}

	LIBXSVF_HOST_REALLOC(buf_tdi_data, 0, LIBXSVF_MEM_XSVF_TDI_DATA);
	LIBXSVF_HOST_REALLOC(buf_tdo_data, 0, LIBXSVF_MEM_XSVF_TDO_DATA);
	LIBXSVF_HOST_REALLOC(buf_tdo_mask, 0, LIBXSVF_MEM_XSVF_TDO_MASK);
	LIBXSVF_HOST_REALLOC(buf_addr_mask, 0, LIBXSVF_MEM_XSVF_ADDR_MASK);
	LIBXSVF_HOST_REALLOC(buf_data_mask, 0, LIBXSVF_MEM_XSVF_DATA_MASK);

	return rc;
}

