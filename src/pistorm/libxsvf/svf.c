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

static int read_command(struct libxsvf_host *h, char **buffer_p, int *len_p)
{
	char *buffer = *buffer_p;
	int braket_mode = 0;
	int len = *len_p;
	int p = 0;

	while (1)
	{
		if (len < p+10) {
			len = len < 64 ? 96 : len*2;
			buffer = LIBXSVF_HOST_REALLOC(buffer, len, LIBXSVF_MEM_SVF_COMMANDBUF);
			*buffer_p = buffer;
			*len_p = len;
			if (!buffer) {
				LIBXSVF_HOST_REPORT_ERROR("Allocating memory failed.");
				return -1;
			}
		}
		buffer[p] = 0;

		int ch = LIBXSVF_HOST_GETBYTE();
		if (ch < 0) {
handle_eof:
			if (p == 0)
				return 0;
			LIBXSVF_HOST_REPORT_ERROR("Unexpected EOF.");
			return -1;
		}
		if (ch <= ' ') {
insert_eol:
			if (!braket_mode && p > 0 && buffer[p-1] != ' ')
				buffer[p++] = ' ';
			continue;
		}
		if (ch == '!') {
skip_to_eol:
			while (1) {
				ch = LIBXSVF_HOST_GETBYTE();
				if (ch < 0)
					goto handle_eof;
				if (ch < ' ' && ch != '\t')
					goto insert_eol;
			}
		}
		if (ch == '/' && p > 0 && buffer[p-1] == '/') {
			p--;
			goto skip_to_eol;
		}
		if (ch == ';')
			break;
		if (ch == '(') {
			if (!braket_mode && p > 0 && buffer[p-1] != ' ')
				buffer[p++] = ' ';
			braket_mode++;
		}
		if (ch >= 'a' && ch <= 'z')
			buffer[p++] = ch - ('a' - 'A');
		else
			buffer[p++] = ch;
		if (ch == ')') {
			braket_mode--;
			if (!braket_mode)
				buffer[p++] = ' ';
		}
	}
	return 1;
}

static int strtokencmp(const char *str1, const char *str2)
{
	int i = 0;
	while (1) {
		if ((str1[i] == ' ' || str1[i] == 0) && (str2[i] == ' ' || str2[i] == 0))
			return 0;
		if (str1[i] < str2[i])
			return -1;
		if (str1[i] > str2[i])
			return +1;
		i++;
	}
}

static int strtokenskip(const char *str1)
{
	int i = 0;
	while (str1[i] != 0 && str1[i] != ' ') i++;
	while (str1[i] == ' ') i++;
	return i;
}

static int token2tapstate(const char *str1)
{
#define X(_t) if (!strtokencmp(str1, #_t)) return LIBXSVF_TAP_ ## _t;
	X(RESET)
	X(IDLE)
	X(DRSELECT)
	X(DRCAPTURE)
	X(DRSHIFT)
	X(DREXIT1)
	X(DRPAUSE)
	X(DREXIT2)
	X(DRUPDATE)
	X(IRSELECT)
	X(IRCAPTURE)
	X(IRSHIFT)
	X(IREXIT1)
	X(IRPAUSE)
	X(IREXIT2)
	X(IRUPDATE)
#undef X
	return -1;
}

struct bitdata_s {
	int len, alloced_len;
	int alloced_bytes;
	unsigned char *tdi_data;
	unsigned char *tdi_mask;
	unsigned char *tdo_data;
	unsigned char *tdo_mask;
	unsigned char *ret_mask;
	int has_tdo_data;
};

static void bitdata_free(struct libxsvf_host *h, struct bitdata_s *bd, int offset)
{
	LIBXSVF_HOST_REALLOC(bd->tdi_data, 0, offset+0);
	LIBXSVF_HOST_REALLOC(bd->tdi_mask, 0, offset+1);
	LIBXSVF_HOST_REALLOC(bd->tdo_data, 0, offset+2);
	LIBXSVF_HOST_REALLOC(bd->tdo_mask, 0, offset+3);
	LIBXSVF_HOST_REALLOC(bd->ret_mask, 0, offset+4);

	bd->tdi_data = (void*)0;
	bd->tdi_mask = (void*)0;
	bd->tdo_data = (void*)0;
	bd->tdo_mask = (void*)0;
	bd->ret_mask = (void*)0;
}

static int hex(char ch)
{
	if (ch >= 'A' && ch <= 'Z')
		return (ch - 'A') + 10;
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	return 0;
}

static const char *bitdata_parse(struct libxsvf_host *h, const char *p, struct bitdata_s *bd, int offset)
{
	int i, j;
	bd->len = 0;
	bd->has_tdo_data = 0;
	while (*p >= '0' && *p <= '9') {
		bd->len = bd->len * 10 + (*p - '0');
		p++;
	}
	while (*p == ' ') {
		p++;
	}
	if (bd->len != bd->alloced_len) {
		bitdata_free(h, bd, offset);
		bd->alloced_len = bd->len;
		bd->alloced_bytes = (bd->len+7) / 8;
	}
	while (*p)
	{
		int memnum = 0;
		unsigned char **dp = (void*)0;
		if (!strtokencmp(p, "TDI")) {
			p += strtokenskip(p);
			dp = &bd->tdi_data;
			memnum = 0;
		}
		if (!strtokencmp(p, "TDO")) {
			p += strtokenskip(p);
			dp = &bd->tdo_data;
			bd->has_tdo_data = 1;
			memnum = 1;
		}
		if (!strtokencmp(p, "SMASK")) {
			p += strtokenskip(p);
			dp = &bd->tdi_mask;
			memnum = 2;
		}
		if (!strtokencmp(p, "MASK")) {
			p += strtokenskip(p);
			dp = &bd->tdo_mask;
			memnum = 3;
		}
		if (!strtokencmp(p, "RMASK")) {
			p += strtokenskip(p);
			dp = &bd->ret_mask;
			memnum = 4;
		}
		if (!dp)
			return (void*)0;
		if (*dp == (void*)0) {
			*dp = LIBXSVF_HOST_REALLOC(*dp, bd->alloced_bytes, offset+memnum);
		}
		if (*dp == (void*)0) {
			LIBXSVF_HOST_REPORT_ERROR("Allocating memory failed.");
			return (void*)0;
		}

		unsigned char *d = *dp;
		for (i=0; i<bd->alloced_bytes; i++)
			d[i] = 0;

		if (*p != '(')
			return (void*)0;
		p++;

		int hexdigits = 0;
		for (i=0; (p[i] >= 'A' && p[i] <= 'F') || (p[i] >= '0' && p[i] <= '9'); i++)
			hexdigits++;

		i = bd->alloced_bytes*2 - hexdigits;
		for (j=0; j<hexdigits; j++, i++, p++) {
			if (i%2 == 0) {
				d[i/2] |= hex(*p) << 4;
			} else {
				d[i/2] |= hex(*p);
			}
		}

		if (*p != ')')
			return (void*)0;
		p++;
		while (*p == ' ') {
			p++;
		}
	}
#if 0
	/* Debugging Output, needs <stdio.h> */
	printf("--- Parsed bitdata [%d] ---\n", bd->len);
	if (bd->tdi_data) {
		printf("TDI DATA:");
		for (i=0; i<bd->alloced_bytes; i++)
			printf(" %02x", bd->tdi_data[i]);
		printf("\n");
	}
	if (bd->tdo_data && has_tdo_data) {
		printf("TDO DATA:");
		for (i=0; i<bd->alloced_bytes; i++)
			printf(" %02x", bd->tdo_data[i]);
		printf("\n");
	}
	if (bd->tdi_mask) {
		printf("TDI MASK:");
		for (i=0; i<bd->alloced_bytes; i++)
			printf(" %02x", bd->tdi_mask[i]);
		printf("\n");
	}
	if (bd->tdo_mask) {
		printf("TDO MASK:");
		for (i=0; i<bd->alloced_bytes; i++)
			printf(" %02x", bd->tdo_mask[i]);
		printf("\n");
	}
#endif
	return p;
}

static int getbit(unsigned char *data, int n)
{
	return (data[n/8] & (1 << (7 - n%8))) ? 1 : 0;
}

static int bitdata_play(struct libxsvf_host *h, struct bitdata_s *bd, enum libxsvf_tap_state estate)
{
	int left_padding = (8 - bd->len % 8) % 8;
	int tdo_error = 0;
	int tms = 0;
	int i;

	for (i=bd->len+left_padding-1; i >= left_padding; i--) {
		if (i == left_padding && h->tap_state != estate) {
			h->tap_state++;
			tms = 1;
		}
		int tdi = -1;
		if (bd->tdi_data) {
			if (!bd->tdi_mask || getbit(bd->tdi_mask, i))
				tdi = getbit(bd->tdi_data, i);
		}
		int tdo = -1;
		if (bd->tdo_data && bd->has_tdo_data && (!bd->tdo_mask || getbit(bd->tdo_mask, i)))
			tdo = getbit(bd->tdo_data, i);
		int rmask = bd->ret_mask && getbit(bd->ret_mask, i);
		if (LIBXSVF_HOST_PULSE_TCK(tms, tdi, tdo, rmask, 0) < 0)
			tdo_error = 1;
	}

	if (tms)
		LIBXSVF_HOST_REPORT_TAPSTATE();

	if (!tdo_error)
		return 0;

	LIBXSVF_HOST_REPORT_ERROR("TDO mismatch.");
	return -1;
}

int libxsvf_svf(struct libxsvf_host *h)
{
	char *command_buffer = (void*)0;
	int command_buffer_len = 0;
	int rc, i;
/*
	struct bitdata_s bd_hdr = { 0, 0, 0, (void*)0, (void*)0, (void*)0, (void*)0, (void*)0 };
	struct bitdata_s bd_hir = { 0, 0, 0, (void*)0, (void*)0, (void*)0, (void*)0, (void*)0 };
	struct bitdata_s bd_tdr = { 0, 0, 0, (void*)0, (void*)0, (void*)0, (void*)0, (void*)0 };
	struct bitdata_s bd_tir = { 0, 0, 0, (void*)0, (void*)0, (void*)0, (void*)0, (void*)0 };
	struct bitdata_s bd_sdr = { 0, 0, 0, (void*)0, (void*)0, (void*)0, (void*)0, (void*)0 };
	struct bitdata_s bd_sir = { 0, 0, 0, (void*)0, (void*)0, (void*)0, (void*)0, (void*)0 };
*/

	struct bitdata_s bd_hdr = { 0 };
	struct bitdata_s bd_hir = { 0 };
	struct bitdata_s bd_tdr = { 0 };
	struct bitdata_s bd_tir = { 0 };
	struct bitdata_s bd_sdr = { 0 };
	struct bitdata_s bd_sir = { 0 };

	int state_endir = LIBXSVF_TAP_IDLE;
	int state_enddr = LIBXSVF_TAP_IDLE;
	int state_run = LIBXSVF_TAP_IDLE;
	int state_endrun = LIBXSVF_TAP_IDLE;

	while (1)
	{
		rc = read_command(h, &command_buffer, &command_buffer_len);

		if (rc <= 0)
			break;

		const char *p = command_buffer;

		LIBXSVF_HOST_REPORT_STATUS(command_buffer);

		if (!strtokencmp(p, "ENDIR")) {
			p += strtokenskip(p);
			state_endir = token2tapstate(p);
			if (state_endir < 0)
				goto syntax_error;
			p += strtokenskip(p);
			goto eol_check;
		}

		if (!strtokencmp(p, "ENDDR")) {
			p += strtokenskip(p);
			state_enddr = token2tapstate(p);
			if (state_endir < 0)
				goto syntax_error;
			p += strtokenskip(p);
			goto eol_check;
		}

		if (!strtokencmp(p, "FREQUENCY")) {
			p += strtokenskip(p);

			unsigned long mantissa_val = 0;
			int fraction_len = 0;
			int in_fraction = 0;
			int exp_val = 0;
			int exp_sign = 1;
			unsigned long final_freq;
			int i;

			// Parse mantissa (as integer, ignoring '.')
			while ((*p >= '0' && *p <= '9') || *p == '.') {
				if (*p == '.') {
					in_fraction = 1;
					p++;
					continue;
				}
				mantissa_val = mantissa_val * 10 + (*p - '0');
				if (in_fraction) {
					fraction_len++;
				}
				p++;
			}

			// Parse exponent
			if (*p == 'E' || *p == 'e') {
				p++;
				if (*p == '+') {
					p++;
				} else if (*p == '-') {
					exp_sign = -1;
					p++;
				}
				while (*p >= '0' && *p <= '9') {
					exp_val = exp_val * 10 + (*p - '0');
					p++;
				}
			}

			int final_exp = (exp_val * exp_sign) - fraction_len;
			final_freq = mantissa_val;

			if (final_exp > 0) {
				for (i = 0; i < final_exp; i++) {
					final_freq *= 10;
				}
			} else if (final_exp < 0) {
				for (i = 0; i > final_exp; i--) {
					final_freq /= 10;
				}
			}
			
			// Skip trailing space and "HZ"
			while (*p == ' ') p++;
			if (*p == 'H' && *(p+1) == 'Z') p += 2;
			while (*p == ' ') p++;

			if (LIBXSVF_HOST_SET_FREQUENCY(final_freq) < 0) {
				LIBXSVF_HOST_REPORT_ERROR("FREQUENCY command failed!");
				goto error;
			}
			goto eol_check;
		}

		if (!strtokencmp(p, "HDR")) {
			p += strtokenskip(p);
			p = bitdata_parse(h, p, &bd_hdr, LIBXSVF_MEM_SVF_HDR_TDI_DATA);
			if (!p)
				goto syntax_error;
			goto eol_check;
		}

		if (!strtokencmp(p, "HIR")) {
			p += strtokenskip(p);
			p = bitdata_parse(h, p, &bd_hir, LIBXSVF_MEM_SVF_HIR_TDI_DATA);
			if (!p)
				goto syntax_error;
			goto eol_check;
		}

		if (!strtokencmp(p, "PIO") || !strtokencmp(p, "PIOMAP")) {
			goto unsupported_error;
		}

		if (!strtokencmp(p, "RUNTEST")) {
			p += strtokenskip(p);
			int tck_count = -1;
			int sck_count = -1;
			int min_time = -1;
			int max_time = -1;
			while (*p) {
				int got_maximum = 0;
				if (!strtokencmp(p, "MAXIMUM")) {
					p += strtokenskip(p);
					got_maximum = 1;
				}
				int got_endstate = 0;
				if (!strtokencmp(p, "ENDSTATE")) {
					p += strtokenskip(p);
					got_endstate = 1;
				}
				int st = token2tapstate(p);
				if (st >= 0) {
					p += strtokenskip(p);
					if (got_endstate)
						state_endrun = st;
					else
						state_run = st;
					continue;
				}
				if (*p < '0' || *p > '9')
					goto syntax_error;
				int number = 0;
				int exp = 0, expsign = 1;
				int number_e6, exp_e6;
				while (*p >= '0' && *p <= '9') {
					number = number*10 + (*p - '0');
					p++;
				}
				if(*p == 'E' || *p == 'e') {
					p++;
					if(*p == '-') {
						expsign = -1;
						p++;
					}
					while (*p >= '0' && *p <= '9') {
						exp = exp*10 + (*p - '0');
						p++;
					}
					exp = exp * expsign;
					number_e6 = number;
					exp_e6 = exp + 6;
					while (exp < 0) {
						number /= 10;
						exp++;
					}
					while (exp > 0) {
						number *= 10;
						exp--;
					}
					while (exp_e6 < 0) {
						number_e6 /= 10;
						exp_e6++;
					}
					while (exp_e6 > 0) {
						number_e6 *= 10;
						exp_e6--;
					}
				} else {
					number_e6 = number * 1000000;
				}
				while (*p == ' ') {
					p++;
				}
				if (!strtokencmp(p, "SEC")) {
					p += strtokenskip(p);
					if (got_maximum)
						max_time = number_e6;
					else
						min_time = number_e6;
					continue;
				}
				if (!strtokencmp(p, "TCK")) {
					p += strtokenskip(p);
					tck_count = number;
					continue;
				}
				if (!strtokencmp(p, "SCK")) {
					p += strtokenskip(p);
					sck_count = number;
					continue;
				}
				goto syntax_error;
			}
			if (libxsvf_tap_walk(h, state_run) < 0)
				goto error;
			if (max_time >= 0) {
				LIBXSVF_HOST_REPORT_ERROR("WARNING: Maximum time in SVF RUNTEST command is ignored.");
			}
			if (sck_count >= 0) {
				for (i=0; i < sck_count; i++) {
					LIBXSVF_HOST_PULSE_SCK();
				}
			}
			if (min_time >= 0 || tck_count >= 0) {
				LIBXSVF_HOST_UDELAY(min_time >= 0 ? min_time : 0, 0, tck_count >= 0 ? tck_count : 0);
			}
			if (libxsvf_tap_walk(h, state_endrun) < 0)
				goto error;
			goto eol_check;
		}

		if (!strtokencmp(p, "SDR")) {
			p += strtokenskip(p);
			p = bitdata_parse(h, p, &bd_sdr, LIBXSVF_MEM_SVF_SDR_TDI_DATA);
			if (!p)
				goto syntax_error;
			if (libxsvf_tap_walk(h, LIBXSVF_TAP_DRSHIFT) < 0)
				goto error;
			if (bitdata_play(h, &bd_hdr, bd_sdr.len+bd_tdr.len > 0 ? LIBXSVF_TAP_DRSHIFT : state_enddr) < 0)
				goto error;
			if (bitdata_play(h, &bd_sdr, bd_tdr.len > 0 ? LIBXSVF_TAP_DRSHIFT : state_enddr) < 0)
				goto error;
			if (bitdata_play(h, &bd_tdr, state_enddr) < 0)
				goto error;
			if (libxsvf_tap_walk(h, state_enddr) < 0)
				goto error;
			goto eol_check;
		}

		if (!strtokencmp(p, "SIR")) {
			p += strtokenskip(p);
			p = bitdata_parse(h, p, &bd_sir, LIBXSVF_MEM_SVF_SIR_TDI_DATA);
			if (!p)
				goto syntax_error;
			if (libxsvf_tap_walk(h, LIBXSVF_TAP_IRSHIFT) < 0)
				goto error;
			if (bitdata_play(h, &bd_hir, bd_sir.len+bd_tir.len > 0 ? LIBXSVF_TAP_IRSHIFT : state_endir) < 0)
				goto error;
			if (bitdata_play(h, &bd_sir, bd_tir.len > 0 ? LIBXSVF_TAP_IRSHIFT : state_endir) < 0)
				goto error;
			if (bitdata_play(h, &bd_tir, state_endir) < 0)
				goto error;
			if (libxsvf_tap_walk(h, state_endir) < 0)
				goto error;
			goto eol_check;
		}

		if (!strtokencmp(p, "STATE")) {
			p += strtokenskip(p);
			while (*p) {
				int st = token2tapstate(p);
				if (st < 0)
					goto syntax_error;
				if (libxsvf_tap_walk(h, st) < 0)
					goto error;
				p += strtokenskip(p);
			}
			goto eol_check;
		}

		if (!strtokencmp(p, "TDR")) {
			p += strtokenskip(p);
			p = bitdata_parse(h, p, &bd_tdr, LIBXSVF_MEM_SVF_TDR_TDI_DATA);
			if (!p)
				goto syntax_error;
			goto eol_check;
		}

		if (!strtokencmp(p, "TIR")) {
			p += strtokenskip(p);
			p = bitdata_parse(h, p, &bd_tir, LIBXSVF_MEM_SVF_TIR_TDI_DATA);
			if (!p)
				goto syntax_error;
			goto eol_check;
		}

		if (!strtokencmp(p, "TRST")) {
			p += strtokenskip(p);
			if (!strtokencmp(p, "ON")) {
				p += strtokenskip(p);
				LIBXSVF_HOST_SET_TRST(1);
				goto eol_check;
			}
			if (!strtokencmp(p, "OFF")) {
				p += strtokenskip(p);
				LIBXSVF_HOST_SET_TRST(0);
				goto eol_check;
			}
			if (!strtokencmp(p, "Z")) {
				p += strtokenskip(p);
				LIBXSVF_HOST_SET_TRST(-1);
				goto eol_check;
			}
			if (!strtokencmp(p, "ABSENT")) {
				p += strtokenskip(p);
				LIBXSVF_HOST_SET_TRST(-2);
				goto eol_check;
			}
			goto syntax_error;
		}

eol_check:
		while (*p == ' ')
			p++;
		if (*p == 0)
			continue;

syntax_error:
		LIBXSVF_HOST_REPORT_ERROR("SVF Syntax Error:");
		if (0) {
unsupported_error:
			LIBXSVF_HOST_REPORT_ERROR("Error in SVF input: unsupported command:");
		}
		LIBXSVF_HOST_REPORT_ERROR(command_buffer);
error:
		rc = -1;
		break;
	}

	if (LIBXSVF_HOST_SYNC() != 0 && rc >= 0 ) {
		LIBXSVF_HOST_REPORT_ERROR("TDO mismatch.");
		rc = -1;
	}

	bitdata_free(h, &bd_hdr, LIBXSVF_MEM_SVF_HDR_TDI_DATA);
	bitdata_free(h, &bd_hir, LIBXSVF_MEM_SVF_HIR_TDI_DATA);
	bitdata_free(h, &bd_tdr, LIBXSVF_MEM_SVF_TDR_TDI_DATA);
	bitdata_free(h, &bd_tir, LIBXSVF_MEM_SVF_TIR_TDI_DATA);
	bitdata_free(h, &bd_sdr, LIBXSVF_MEM_SVF_SDR_TDI_DATA);
	bitdata_free(h, &bd_sir, LIBXSVF_MEM_SVF_SIR_TDI_DATA);

	LIBXSVF_HOST_REALLOC(command_buffer, 0, LIBXSVF_MEM_SVF_COMMANDBUF);

	return rc;
}

