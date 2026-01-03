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

#ifndef LIBXSVF_H
#define LIBXSVF_H

enum libxsvf_mode {
	LIBXSVF_MODE_SVF = 1,
	LIBXSVF_MODE_XSVF = 2,
	LIBXSVF_MODE_SCAN = 3,
	LIBXSVF_MODE_READ_USERCODE = 4
};

enum libxsvf_tap_state {
	/* Special States */
	LIBXSVF_TAP_INIT = 0,
	LIBXSVF_TAP_RESET = 1,
	LIBXSVF_TAP_IDLE = 2,
	/* DR States */
	LIBXSVF_TAP_DRSELECT = 3,
	LIBXSVF_TAP_DRCAPTURE = 4,
	LIBXSVF_TAP_DRSHIFT = 5,
	LIBXSVF_TAP_DREXIT1 = 6,
	LIBXSVF_TAP_DRPAUSE = 7,
	LIBXSVF_TAP_DREXIT2 = 8,
	LIBXSVF_TAP_DRUPDATE = 9,
	/* IR States */
	LIBXSVF_TAP_IRSELECT = 10,
	LIBXSVF_TAP_IRCAPTURE = 11,
	LIBXSVF_TAP_IRSHIFT = 12,
	LIBXSVF_TAP_IREXIT1 = 13,
	LIBXSVF_TAP_IRPAUSE = 14,
	LIBXSVF_TAP_IREXIT2 = 15,
	LIBXSVF_TAP_IRUPDATE = 16
};

enum libxsvf_mem {
	LIBXSVF_MEM_XSVF_TDI_DATA = 0,
	LIBXSVF_MEM_XSVF_TDO_DATA = 1,
	LIBXSVF_MEM_XSVF_TDO_MASK = 2,
	LIBXSVF_MEM_XSVF_ADDR_MASK = 3,
	LIBXSVF_MEM_XSVF_DATA_MASK = 4,
	LIBXSVF_MEM_SVF_COMMANDBUF = 5,
	LIBXSVF_MEM_SVF_SDR_TDI_DATA = 6,
	LIBXSVF_MEM_SVF_SDR_TDI_MASK = 7,
	LIBXSVF_MEM_SVF_SDR_TDO_DATA = 8,
	LIBXSVF_MEM_SVF_SDR_TDO_MASK = 9,
	LIBXSVF_MEM_SVF_SDR_RET_MASK = 10,
	LIBXSVF_MEM_SVF_SIR_TDI_DATA = 11,
	LIBXSVF_MEM_SVF_SIR_TDI_MASK = 12,
	LIBXSVF_MEM_SVF_SIR_TDO_DATA = 13,
	LIBXSVF_MEM_SVF_SIR_TDO_MASK = 14,
	LIBXSVF_MEM_SVF_SIR_RET_MASK = 15,
	LIBXSVF_MEM_SVF_HDR_TDI_DATA = 16,
	LIBXSVF_MEM_SVF_HDR_TDI_MASK = 17,
	LIBXSVF_MEM_SVF_HDR_TDO_DATA = 18,
	LIBXSVF_MEM_SVF_HDR_TDO_MASK = 19,
	LIBXSVF_MEM_SVF_HDR_RET_MASK = 20,
	LIBXSVF_MEM_SVF_HIR_TDI_DATA = 21,
	LIBXSVF_MEM_SVF_HIR_TDI_MASK = 22,
	LIBXSVF_MEM_SVF_HIR_TDO_DATA = 23,
	LIBXSVF_MEM_SVF_HIR_TDO_MASK = 24,
	LIBXSVF_MEM_SVF_HIR_RET_MASK = 25,
	LIBXSVF_MEM_SVF_TDR_TDI_DATA = 26,
	LIBXSVF_MEM_SVF_TDR_TDI_MASK = 27,
	LIBXSVF_MEM_SVF_TDR_TDO_DATA = 28,
	LIBXSVF_MEM_SVF_TDR_TDO_MASK = 29,
	LIBXSVF_MEM_SVF_TDR_RET_MASK = 30,
	LIBXSVF_MEM_SVF_TIR_TDI_DATA = 31,
	LIBXSVF_MEM_SVF_TIR_TDI_MASK = 32,
	LIBXSVF_MEM_SVF_TIR_TDO_DATA = 33,
	LIBXSVF_MEM_SVF_TIR_TDO_MASK = 34,
	LIBXSVF_MEM_SVF_TIR_RET_MASK = 35,
	LIBXSVF_MEM_NUM = 36
};

struct libxsvf_host {
	int (*setup)(struct libxsvf_host *h);
	int (*shutdown)(struct libxsvf_host *h);
	void (*udelay)(struct libxsvf_host *h, long usecs, int tms, long num_tck);
	int (*getbyte)(struct libxsvf_host *h);
	int (*sync)(struct libxsvf_host *h);
	int (*pulse_tck)(struct libxsvf_host *h, int tms, int tdi, int tdo, int rmask, int sync);
	void (*pulse_sck)(struct libxsvf_host *h);
	void (*set_trst)(struct libxsvf_host *h, int v);
	int (*set_frequency)(struct libxsvf_host *h, int v);
	void (*report_tapstate)(struct libxsvf_host *h);
	void (*report_device)(struct libxsvf_host *h, unsigned long idcode);
	void (*report_usercode)(struct libxsvf_host *h, unsigned long usercode);
	void (*report_status)(struct libxsvf_host *h, const char *message);
	void (*report_error)(struct libxsvf_host *h, const char *file, int line, const char *message);
	void *(*realloc)(struct libxsvf_host *h, void *ptr, int size, enum libxsvf_mem which);
	enum libxsvf_tap_state tap_state;
	void *user_data;
};

int libxsvf_play(struct libxsvf_host *, enum libxsvf_mode mode);
const char *libxsvf_state2str(enum libxsvf_tap_state tap_state);
const char *libxsvf_mem2str(enum libxsvf_mem which);

/* Internal API */ 
int libxsvf_svf(struct libxsvf_host *h);
int libxsvf_xsvf(struct libxsvf_host *h);
int libxsvf_scan(struct libxsvf_host *h);
int libxsvf_read_usercode(struct libxsvf_host *h);
int libxsvf_tap_walk(struct libxsvf_host *, enum libxsvf_tap_state);

/* Host accessor macros (see README) */
#define LIBXSVF_HOST_SETUP() h->setup(h)
#define LIBXSVF_HOST_SHUTDOWN() h->shutdown(h)
#define LIBXSVF_HOST_UDELAY(_usecs, _tms, _num_tck) h->udelay(h, _usecs, _tms, _num_tck)
#define LIBXSVF_HOST_GETBYTE() h->getbyte(h)
#define LIBXSVF_HOST_SYNC() (h->sync ? h->sync(h) : 0)
#define LIBXSVF_HOST_PULSE_TCK(_tms, _tdi, _tdo, _rmask, _sync) h->pulse_tck(h, _tms, _tdi, _tdo, _rmask, _sync)
#define LIBXSVF_HOST_PULSE_SCK() do { if (h->pulse_sck) h->pulse_sck(h); } while (0)
#define LIBXSVF_HOST_SET_TRST(_v) do { if (h->set_trst) h->set_trst(h, _v); } while (0)
#define LIBXSVF_HOST_SET_FREQUENCY(_v) (h->set_frequency ? h->set_frequency(h, _v) : -1)
#define LIBXSVF_HOST_REPORT_TAPSTATE() do { if (h->report_tapstate) h->report_tapstate(h); } while (0)
#define LIBXSVF_HOST_REPORT_DEVICE(_v) do { if (h->report_device) h->report_device(h, _v); } while (0)
#define LIBXSVF_HOST_REPORT_USERCODE(_v) do { if (h->report_usercode) h->report_usercode(h, _v); } while (0)
#define LIBXSVF_HOST_REPORT_STATUS(_msg) do { if (h->report_status) h->report_status(h, _msg); } while (0)
#define LIBXSVF_HOST_REPORT_ERROR(_msg) h->report_error(h, __FILE__, __LINE__, _msg)
#define LIBXSVF_HOST_REALLOC(_ptr, _size, _which) h->realloc(h, _ptr, _size, _which)


#endif
