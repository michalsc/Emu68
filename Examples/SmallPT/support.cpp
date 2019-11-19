#include "support.h"
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>

double copysign (double x, double y)
{
    union {
        uint32_t u32[2];
        double d;
    } u;

    u.d = y;

    if (u.u32[0] & 0x80000000) {
        u.d = x;
        u.u32[0] |= 0x80000000;
        x = u.d;
    }

    return x;
}



static const double
two54   =  1.80143985094819840000e+16, /* 0x43500000, 0x00000000 */
twom54  =  5.55111512312578270212e-17, /* 0x3C900000, 0x00000000 */
huge   = 1.0e+300,
tiny   = 1.0e-300;



double scalbn (double x, int n)
{
    int64_t ix;
    int64_t k;

    union {
        double d;
        uint64_t u;
    } un;

    un.d = x;
    ix = un.u;

    k = (ix >> 52) & 0x7ff;                        /* extract exponent */
    if (__builtin_expect(k==0, 0)) {        /* 0 or subnormal x */
        if ((ix & (uint64_t)(0xfffffffffffffULL))==0) return x; /* +-0 */
        x *= two54;
        un.d = x;
        ix = un.u;

        k = ((ix >> 52) & 0x7ff) - 54;
    }
    if (__builtin_expect(k==0x7ff, 0)) return x+x;        /* NaN or Inf */
    if (__builtin_expect(n< -50000, 0))
        return tiny*copysign(tiny,x); /*underflow*/
    if (__builtin_expect(n> 50000 || k+n > 0x7fe, 0))
        return huge*copysign(huge,x); /* overflow  */
    /* Now k and n are bounded we know that k = k+n does not
       overflow.  */
    k = k+n;
    if (__builtin_expect(k > 0, 1))                /* normal result */
    {
        un.u = (ix&(uint64_t)(0x800fffffffffffffULL))|(k<<52);
        x = un.d;
        return x;
    }

    if (k <= -54)
        return tiny*copysign(tiny,x);        /*underflow*/
    k += 54;                                /* subnormal result */
    un.u = (ix&(uint64_t)(0x800fffffffffffffULL))|(k<<52);
    x = un.d;
    return x*twom54;
}

static const double
bp[] = {1.0, 1.5,},
dp_h[] = { 0.0, 5.84962487220764160156e-01,}, /* 0x3FE2B803, 0x40000000 */
dp_l[] = { 0.0, 1.35003920212974897128e-08,}, /* 0x3E4CFDEB, 0x43CFD006 */
zero    =  0.0,
one     =  1.0,
two	=  2.0,
two53	=  9007199254740992.0,	/* 0x43400000, 0x00000000 */
    /* poly coefs for (3/2)*(log(x)-2s-2/3*s**3 */
L1  =  5.99999999999994648725e-01, /* 0x3FE33333, 0x33333303 */
L2  =  4.28571428578550184252e-01, /* 0x3FDB6DB6, 0xDB6FABFF */
L3  =  3.33333329818377432918e-01, /* 0x3FD55555, 0x518F264D */
L4  =  2.72728123808534006489e-01, /* 0x3FD17460, 0xA91D4101 */
L5  =  2.30660745775561754067e-01, /* 0x3FCD864A, 0x93C9DB65 */
L6  =  2.06975017800338417784e-01, /* 0x3FCA7E28, 0x4A454EEF */
P1   =  1.66666666666666019037e-01, /* 0x3FC55555, 0x5555553E */
P2   = -2.77777777770155933842e-03, /* 0xBF66C16C, 0x16BEBD93 */
P3   =  6.61375632143793436117e-05, /* 0x3F11566A, 0xAF25DE2C */
P4   = -1.65339022054652515390e-06, /* 0xBEBBBD41, 0xC5D26BF1 */
P5   =  4.13813679705723846039e-08, /* 0x3E663769, 0x72BEA4D0 */
lg2  =  6.93147180559945286227e-01, /* 0x3FE62E42, 0xFEFA39EF */
lg2_h  =  6.93147182464599609375e-01, /* 0x3FE62E43, 0x00000000 */
lg2_l  = -1.90465429995776804525e-09, /* 0xBE205C61, 0x0CA86C39 */
ovt =  8.0085662595372944372e-0017, /* -(1024-log2(ovfl+.5ulp)) */
cp    =  9.61796693925975554329e-01, /* 0x3FEEC709, 0xDC3A03FD =2/(3ln2) */
cp_h  =  9.61796700954437255859e-01, /* 0x3FEEC709, 0xE0000000 =(float)cp */
cp_l  = -7.02846165095275826516e-09, /* 0xBE3E2FE0, 0x145B01F5 =tail of cp_h*/
ivln2    =  1.44269504088896338700e+00, /* 0x3FF71547, 0x652B82FE =1/ln2 */
ivln2_h  =  1.44269502162933349609e+00, /* 0x3FF71547, 0x60000000 =24b 1/ln2*/
ivln2_l  =  1.92596299112661746887e-08; /* 0x3E54AE0B, 0xF85DDF44 =1/ln2 tail*/

double pow(double x, double y)
{
    double z,ax,z_h,z_l,p_h,p_l;
    double y1,t1,t2,r,s,t,u,v,w;
    __int32_t i,j,k,yisint,n;
    __int32_t hx,hy,ix,iy;
    __uint32_t lx,ly;

    union {
	uint32_t u32[2];
	double d;
    } un;

    un.d = x;
    hx = un.u32[0];
    lx = un.u32[1];

    un.d = y;
    hy = un.u32[0];
    ly = un.u32[1];

    ix = hx&0x7fffffff;  iy = hy&0x7fffffff;
    /* y==zero: x**0 = 1 */
    if((iy|ly)==0) return one;
    /* x|y==NaN return NaN unless x==1 then return 1 */
    if(ix > 0x7ff00000 || ((ix==0x7ff00000)&&(lx!=0)) ||
       iy > 0x7ff00000 || ((iy==0x7ff00000)&&(ly!=0))) {
        if(((ix-0x3ff00000)|lx)==0) return one;
        else return zero;
    }
    /* determine if y is an odd int when x < 0
     * yisint = 0	... y is not an integer
     * yisint = 1	... y is an odd int
     * yisint = 2	... y is an even int
     */
    yisint  = 0;
    if(hx<0) {
        if(iy>=0x43400000) yisint = 2; /* even integer y */
        else if(iy>=0x3ff00000) {
	k = (iy>>20)-0x3ff;	   /* exponent */
	if(k>20) {
	    j = ly>>(52-k);
	    if((j<<(52-k))==ly) yisint = 2-(j&1);
	} else if(ly==0) {
	    j = iy>>(20-k);
	    if((j<<(20-k))==iy) yisint = 2-(j&1);
	}
        }
    }
    /* special value of y */
    if(ly==0) {
        if (iy==0x7ff00000) {	/* y is +-inf */
            if(((ix-0x3ff00000)|lx)==0)
	    return one;		/* +-1**+-inf = 1 */
            else if (ix >= 0x3ff00000)/* (|x|>1)**+-inf = inf,0 */
	    return (hy>=0)? y: zero;
            else			/* (|x|<1)**-,+inf = inf,0 */
	    return (hy<0)?-y: zero;
        }
        if(iy==0x3ff00000) {	/* y is  +-1 */
	if(hy<0) return one/x; else return x;
        }
        if(hy==0x40000000) return x*x; /* y is  2 */
        if(hy==0x3fe00000) {	/* y is  0.5 */
	if(hx>=0)	/* x >= +0 */
	return sqrt(x);
        }
    }
    ax   = fabs(x);
    /* special value of x */
    if(lx==0) {
        if(ix==0x7ff00000||ix==0||ix==0x3ff00000){
	z = ax;			/*x is +-0,+-inf,+-1*/
	if(hy<0) z = one/z;	/* z = (1/|x|) */
	if(hx<0) {
	    if(((ix-0x3ff00000)|yisint)==0) {
	    z = (z-z)/(z-z); /* (-1)**non-int is NaN */
	    } else if(yisint==1)
	    z = -z;		/* (x<0)**odd = -(|x|**odd) */
	}
	return z;
        }
    }

    /* (x<0)**(non-int) is NaN */
    /* REDHAT LOCAL: This used to be
    if((((hx>>31)+1)|yisint)==0) return (x-x)/(x-x);
       but ANSI C says a right shift of a signed negative quantity is
       implementation defined.  */
    if(((((__uint32_t)hx>>31)-1)|yisint)==0) return (x-x)/(x-x);
    /* |y| is huge */
    if(iy>0x41e00000) { /* if |y| > 2**31 */
        if(iy>0x43f00000){	/* if |y| > 2**64, must o/uflow */
	if(ix<=0x3fefffff) return (hy<0)? huge*huge:tiny*tiny;
	if(ix>=0x3ff00000) return (hy>0)? huge*huge:tiny*tiny;
        }
    /* over/underflow if x is not close to one */
        if(ix<0x3fefffff) return (hy<0)? huge*huge:tiny*tiny;
        if(ix>0x3ff00000) return (hy>0)? huge*huge:tiny*tiny;
    /* now |1-x| is tiny <= 2**-20, suffice to compute
       log(x) by x-x^2/2+x^3/3-x^4/4 */
        t = ax-1;		/* t has 20 trailing zeros */
        w = (t*t)*(0.5-t*(0.3333333333333333333333-t*0.25));
        u = ivln2_h*t;	/* ivln2_h has 21 sig. bits */
        v = t*ivln2_l-w*ivln2;
        t1 = u+v;
        un.d = t1;
        un.u32[1] = 0;
        t1 = un.d;
//        SET_LOW_WORD(t1,0);
        t2 = v-(t1-u);
    } else {
        double s2,s_h,s_l,t_h,t_l;
        n = 0;
    /* take care subnormal number */
        if(ix<0x00100000)
	{ax *= two53; n -= 53; un.d = ax; ix = un.u32[0]; }
        n  += ((ix)>>20)-0x3ff;
        j  = ix&0x000fffff;
    /* determine interval */
        ix = j|0x3ff00000;		/* normalize ix */
        if(j<=0x3988E) k=0;		/* |x|<sqrt(3/2) */
        else if(j<0xBB67A) k=1;	/* |x|<sqrt(3)   */
        else {k=0;n+=1;ix -= 0x00100000;}
        un.d = ax;
        un.u32[0] = ix;
        ax = un.d;
    /* compute s = s_h+s_l = (x-1)/(x+1) or (x-1.5)/(x+1.5) */
        u = ax-bp[k];		/* bp[0]=1.0, bp[1]=1.5 */
        v = one/(ax+bp[k]);
        s = u*v;
        s_h = s;
        un.d = s_h;
        un.u32[1] = 0;
        s_h = un.d;
    /* t_h=ax+bp[k] High */
        t_h = zero;
        un.d = t_h;
        un.u32[0] = ((ix>>1)|0x20000000)+0x00080000+(k<<18);
        t_h = un.d;
        t_l = ax - (t_h-bp[k]);
        s_l = v*((u-s_h*t_h)-s_h*t_l);
    /* compute log(ax) */
        s2 = s*s;
        r = s2*s2*(L1+s2*(L2+s2*(L3+s2*(L4+s2*(L5+s2*L6)))));
        r += s_l*(s_h+s);
        s2  = s_h*s_h;
        t_h = 3.0+s2+r;
        un.d = t_h;
        un.u32[1] = 0;
        t_h = un.d;
        t_l = r-((t_h-3.0)-s2);
    /* u+v = s*(1+...) */
        u = s_h*t_h;
        v = s_l*t_h+t_l*s;
    /* 2/(3log2)*(s+...) */
        p_h = u+v;
        un.d = p_h;
        un.u32[1] = 0;
        p_h = un.d;
        p_l = v-(p_h-u);
        z_h = cp_h*p_h;		/* cp_h+cp_l = 2/(3*log2) */
        z_l = cp_l*p_h+p_l*cp+dp_l[k];
    /* log2(ax) = (s+..)*2/(3*log2) = n + dp_h + z_h + z_l */
        t = (double)n;
        t1 = (((z_h+z_l)+dp_h[k])+t);
        un.d = t1;
        un.u32[1] = 0;
        t1 = un.d;
        t2 = z_l-(((t1-t)-dp_h[k])-z_h);
    }
    s = one; /* s (sign of result -ve**odd) = -1 else = 1 */
    if(((((__uint32_t)hx>>31)-1)|(yisint-1))==0)
        s = -one;/* (-ve)**(odd int) */
    /* split up y into y1+y2 and compute (y1+y2)*(t1+t2) */
    y1  = y;
    un.d = y1;
    un.u32[1] = 0;
    y1 = un.d;
    p_l = (y-y1)*t1+y*t2;
    p_h = y1*t1;
    z = p_l+p_h;
    un.d = z;
    j = un.u32[0];
    i = un.u32[1];
    if (j>=0x40900000) {				/* z >= 1024 */
        if(((j-0x40900000)|i)!=0)			/* if z > 1024 */
	return s*huge*huge;			/* overflow */
        else {
	if(p_l+ovt>z-p_h) return s*huge*huge;	/* overflow */
        }
    } else if((j&0x7fffffff)>=0x4090cc00 ) {	/* z <= -1075 */
        if(((j-0xc090cc00)|i)!=0) 		/* z < -1075 */
	return s*tiny*tiny;		/* underflow */
        else {
	if(p_l<=z-p_h) return s*tiny*tiny;	/* underflow */
        }
    }
    /*
     * compute 2**(p_h+p_l)
     */
    i = j&0x7fffffff;
    k = (i>>20)-0x3ff;
    n = 0;
    if(i>0x3fe00000) {		/* if |z| > 0.5, set n = [z+0.5] */
        n = j+(0x00100000>>(k+1));
        k = ((n&0x7fffffff)>>20)-0x3ff;	/* new k for n */
        t = zero;
        un.d = t;
        un.u32[0] = n&~(0x000fffff>>k);
        t = un.d;
        n = ((n&0x000fffff)|0x00100000)>>(20-k);
        if(j<0) n = -n;
        p_h -= t;
    }
    t = p_l+p_h;
    un.d = t;
    un.u32[1] = 0;
    t = un.d;
    u = t*lg2_h;
    v = (p_l-(t-p_h))*lg2+t*lg2_l;
    z = u+v;
    w = v-(z-u);
    t  = z*z;
    t1  = z - t*(P1+t*(P2+t*(P3+t*(P4+t*P5))));
    r  = (z*t1)/(t1-two)-(w+z*w);
    z  = one-(r-z);
    un.d = z;
    j = un.u32[0];
    j += (n<<20);
    if((j>>20)<=0) z = scalbn(z,(int)n);	/* subnormal output */
    else {
	un.d = z;
	un.u32[0] = j;
	z = un.d;
    }
    return s*z;
}

double erand48(unsigned short *Xi)
{
    union {
        uint16_t x[4];
        uint64_t u64;
    } n;

    union {
        uint64_t u64;
        double d;
    } n1;

    n.x[0] = 0;
    n.x[1] = Xi[0];
    n.x[2] = Xi[1];
    n.x[3] = Xi[2];

    n.u64 = n.u64 * 0x5deece66dUL + 11;
    n.x[0] = 0x03fe;

    Xi[0] = n.x[1];
    Xi[1] = n.x[2];
    Xi[2] = n.x[3];

    n1.u64 = (n.u64) << 4;

    return n1.d;
}

typedef void (*func_ptr) (void);

extern func_ptr __CTOR_LIST__;

void do_global_ctors(void)
{
  func_ptr *p;
  for (p = (&__CTOR_LIST__) + 1; *p != (func_ptr) 0; p++)
    (*p) ();
}

void memcpy (char *d, char *s, int l)
{
        while (l--) *d++ = *s++;
}

uint16_t *framebuffer;
uint32_t pitch;

extern const uint32_t topaz8_charloc[];
extern const uint8_t topaz8_chardata[];

uint32_t text_x = 0;
uint32_t text_y = 0;
const int modulo = 192;
uint32_t fb_width = 0;
uint32_t fb_height = 0;

void init_screen(uint16_t *fb, uint32_t p, uint32_t w, uint32_t h)
{
    framebuffer = fb;
    pitch = p;
    text_y = 0;
    text_x = 0;
    fb_width = w;
    fb_height = h;
}

uint16_t *get_fb()
{
    return framebuffer;
}

uint32_t get_pitch()
{
    return pitch;
}

uint32_t get_width()
{
    return fb_width;
}

uint32_t get_height()
{
    return fb_height;
}

void put_char(uint8_t c)
{
    //kprintf("put_char(%d), fb=%08x, pitch=%d\n", (int)c, framebuffer, pitch);

    if (framebuffer && pitch)
    {
    uint16_t *pos_in_image = (uint16_t*)((uint32_t)framebuffer + (text_y * 16 + 5)* pitch);
    pos_in_image += 4 + text_x * 8;

    if (c == 10) {
	text_x = 0;
	text_y++;
    }
    else if (c >= 32) {
        uint32_t loc = (topaz8_charloc[c - 32] >> 16) >> 3;
        const uint8_t *data = &topaz8_chardata[loc];

        for (int y = 0; y < 16; y++) {
            const uint8_t byte = *data;

            for (int x=0; x < 8; x++) {
                if (byte & (0x80 >> x)) {
                    pos_in_image[x] = 0;
                }
            }

            if (y & 1)
                data += modulo;
            pos_in_image += pitch / 2;
        }
    text_x++;
    }

    }
}


/*********** SUPPORT *************/


#define PL011_0_BASE              (ARM_PERIIOBASE + 0x201000)
#define PRIMECELLID_PL011       0x011

#define PL011_DR                 (0x00)
#define PL011_RSRECR             (0x04)
#define PL011_FR                 (0x18)
#define PL011_ILPR               (0x20)
#define PL011_IBRD               (0x24)
#define PL011_FBRD               (0x28)
#define PL011_LCRH               (0x2C)
#define PL011_CR                 (0x30)
#define PL011_IFLS               (0x34)
#define PL011_IMSC               (0x38)
#define PL011_RIS                (0x3C)
#define PL011_MIS                (0x40)
#define PL011_ICR                (0x44)
#define PL011_DMACR              (0x48)
#define PL011_ITCR               (0x80)
#define PL011_ITIP               (0x84)
#define PL011_ITOP               (0x88)
#define PL011_TDR                (0x8C)

#define PL011_FR_CTS             (1 << 0)
#define PL011_FR_DSR             (1 << 1)
#define PL011_FR_DCD             (1 << 2)
#define PL011_FR_BUSY            (1 << 3)
#define PL011_FR_RXFE            (1 << 4)
#define PL011_FR_TXFF            (1 << 5)
#define PL011_FR_RXFF            (1 << 6)
#define PL011_FR_TXFE            (1 << 7)

#define PL011_LCRH_BRK           (1 << 0)
#define PL011_LCRH_PEN           (1 << 1)
#define PL011_LCRH_EPS           (1 << 2)
#define PL011_LCRH_STP2          (1 << 3)
#define PL011_LCRH_FEN           (1 << 4)
#define PL011_LCRH_WLEN5         (0 << 5)
#define PL011_LCRH_WLEN6         (1 << 5)
#define PL011_LCRH_WLEN7         (2 << 5)
#define PL011_LCRH_WLEN8         (3 << 5)
#define PL011_LCRH_SPS           (1 << 7)

#define PL011_CR_UARTEN          (1 << 0)
#define PL011_CR_SIREN           (1 << 1)
#define PL011_CR_SIRLP           (1 << 2)
#define PL011_CR_LBE             (1 << 7)
#define PL011_CR_TXE             (1 << 8)
#define PL011_CR_RXE             (1 << 9)
#define PL011_CR_RTSEN           (1 << 14)
#define PL011_CR_CTSEN           (1 << 15)

#define PL011_ICR_RIMIC          (1 << 0)
#define PL011_ICR_CTSMIC         (1 << 1)
#define PL011_ICR_DSRMIC         (1 << 2)
#define PL011_ICR_DCDMIC         (1 << 3)
#define PL011_ICR_RXIC           (1 << 4)
#define PL011_ICR_TXIC           (1 << 5)
#define PL011_ICR_RTIC           (1 << 6)
#define PL011_ICR_FEIC           (1 << 7)
#define PL011_ICR_PEIC           (1 << 8)
#define PL011_ICR_BEIC           (1 << 9)
#define PL011_ICR_OEIC           (1 << 10)

static inline uint32_t rd32le(uint32_t iobase) {
    return LE32(*(volatile uint32_t *)(iobase));
}

static inline uint32_t rd32be(uint32_t iobase) {
    return BE32(*(volatile uint32_t *)(iobase));
}

static inline uint16_t rd16le(uint32_t iobase) {
    return LE16(*(volatile uint16_t *)(iobase));
}

static inline uint16_t rd16be(uint32_t iobase) {
    return BE16(*(volatile uint16_t *)(iobase));
}

static inline uint8_t rd8(uint32_t iobase) {
    return *(volatile uint8_t *)(iobase);
}

static inline void wr32le(uint32_t iobase, uint32_t value) {
    *(volatile uint32_t *)(iobase) = LE32(value);
}

static inline void wr32be(uint32_t iobase, uint32_t value) {
    *(volatile uint32_t *)(iobase) = BE32(value);
}

static inline void wr16le(uint32_t iobase, uint16_t value) {
    *(volatile uint16_t *)(iobase) = LE16(value);
}

static inline void wr16be(uint32_t iobase, uint16_t value) {
    *(volatile uint16_t *)(iobase) = BE16(value);
}

static inline void wr8(uint32_t iobase, uint8_t value) {
    *(volatile uint8_t *)(iobase) = value;
}

typedef void (*putc_func)(void *data, char c);

int int_strlen(char *buf)
{
    int len = 0;

    if (buf)
        while(*buf++)
            len++;

    return len;
}

void int_itoa(char *buf, char base, uintptr_t value, char zero_pad, int precision, int size_mod, char big, int alternate_form, int neg, char sign)
{
    int length = 0;

    do {
        char c = value % base;

        if (c >= 10) {
            if (big)
                c += 'A'-10;
            else
                c += 'a'-10;
        }
        else
            c += '0';

        value = value / base;
        buf[length++] = c;
    } while(value != 0);

    if (precision != 0)
    {
        while (length < precision)
            buf[length++] = '0';
    }
    else if (size_mod != 0 && zero_pad)
    {
        int sz_mod = size_mod;
        if (alternate_form)
        {
            if (base == 16) sz_mod -= 2;
            else if (base == 8) sz_mod -= 1;
        }
        if (neg)
            sz_mod -= 1;

        while (length < sz_mod)
            buf[length++] = '0';
    }
    if (alternate_form)
    {
        if (base == 8)
            buf[length++] = '0';
        if (base == 16) {
            buf[length++] = big ? 'X' : 'x';
            buf[length++] = '0';
        }
    }

    if (neg)
        buf[length++] = '-';
    else {
        if (sign == '+')
            buf[length++] = '+';
        else if (sign == ' ')
            buf[length++] = ' ';
    }

    for (int i=0; i < length/2; i++)
    {
        char tmp = buf[i];
        buf[i] = buf[length - i - 1];
        buf[length - i - 1] = tmp;
    }

    buf[length] = 0;
}

void vkprintf_pc(putc_func putc_f, void *putc_data, const char * format, va_list args)
{
    char tmpbuf[32];

    while(*format)
    {
        char c;
        char alternate_form = 0;
        int size_mod = 0;
        int length_mod = 0;
        int precision = 0;
        char zero_pad = 0;
        char *str;
        char sign = 0;
        char leftalign = 0;
        uintptr_t value = 0;
        intptr_t ivalue = 0;

        char big = 0;

        c = *format++;

        if (c != '%')
        {
            putc_f(putc_data, c);
        }
        else
        {
            c = *format++;

            if (c == '#') {
                alternate_form = 1;
                c = *format++;
            }

            if (c == '-') {
                leftalign = 1;
                c = *format++;
            }

            if (c == ' ' || c == '+') {
                sign = c;
                c = *format++;
            }

            if (c == '0') {
                zero_pad = 1;
                c = *format++;
            }

            while(c >= '0' && c <= '9') {
                size_mod = size_mod * 10;
                size_mod = size_mod + c - '0';
                c = *format++;
            }

            if (c == '.') {
                c = *format++;
                while(c >= '0' && c <= '9') {
                    precision = precision * 10;
                    precision = precision + c - '0';
                    c = *format++;
                }
            }

            big = 0;

            if (c == 'h')
            {
                c = *format++;
                if (c == 'h')
                {
                    c = *format++;
                    length_mod = 1;
                }
                else length_mod = 2;
            }
            else if (c == 'l')
            {
                c = *format++;
                if (c == 'l')
                {
                    c = *format++;
                    length_mod = 8;
                }
                else length_mod = 4;
            }
            else if (c == 'j')
            {
                c = *format++;
                length_mod = 9;
            }
            else if (c == 't')
            {
                c = *format++;
                length_mod = 10;
            }
            else if (c == 'z')
            {
                c = *format++;
                length_mod = 11;
            }

            switch (c) {
                case 0:
                    return;

                case '%':
                    putc_f(putc_data, '%');
                    break;

                case 'X':
                    big = 1;
                    /* fallthrough */
                case 'x':
                    switch (length_mod) {
                        case 8:
                            value = va_arg(args, uint64_t);
                            break;
                        case 9:
                            value = va_arg(args, uintmax_t);
                            break;
                        case 10:
                            value = va_arg(args, uintptr_t);
                            break;
                        case 11:
                            value = va_arg(args, size_t);
                            break;
                        default:
                            value = va_arg(args, unsigned int);
                            break;
                    }
                    int_itoa(tmpbuf, 16, value, zero_pad, precision, size_mod, big, alternate_form, 0, sign);
                    str = tmpbuf;
                    size_mod -= int_strlen(str);
                    if (!leftalign)
                        while(size_mod-- > 0)
                            putc_f(putc_data, ' ');
                    do {
                        putc_f(putc_data, *str);
                    } while(*str++);
                    if (leftalign)
                        while(size_mod-- > 0)
                            putc_f(putc_data, ' ');

                    break;

                case 'u':
                    switch (length_mod) {
                        case 8:
                            value = va_arg(args, uint64_t);
                            break;
                        case 9:
                            value = va_arg(args, uintmax_t);
                            break;
                        case 10:
                            value = va_arg(args, uintptr_t);
                            break;
                        case 11:
                            value = va_arg(args, size_t);
                            break;
                        default:
                            value = va_arg(args, unsigned int);
                            break;
                    }
                    int_itoa(tmpbuf, 10, value, zero_pad, precision, size_mod, 0, alternate_form, 0, sign);
                    str = tmpbuf;
                    size_mod -= int_strlen(str);
                    if (!leftalign)
                        while(size_mod-- > 0)
                            putc_f(putc_data, ' ');
                    do {
                        putc_f(putc_data, *str);
                    } while(*str++);
                    if (leftalign)
                        while(size_mod-- > 0)
                            putc_f(putc_data, ' ');
                    break;

                case 'd':
                case 'i':
                    switch (length_mod) {
                        case 8:
                            ivalue = va_arg(args, int64_t);
                            break;
                        case 9:
                            ivalue = va_arg(args, intmax_t);
                            break;
                        case 10:
                            ivalue = va_arg(args, intptr_t);
                            break;
                        case 11:
                            ivalue = va_arg(args, size_t);
                            break;
                        default:
                            ivalue = va_arg(args, int);
                            break;
                    }
                    if (ivalue < 0)
                        int_itoa(tmpbuf, 10, -ivalue, zero_pad, precision, size_mod, 0, alternate_form, 1, sign);
                    else
                        int_itoa(tmpbuf, 10, ivalue, zero_pad, precision, size_mod, 0, alternate_form, 0, sign);
                    str = tmpbuf;
                    size_mod -= int_strlen(str);
                    if (!leftalign)
                        while(size_mod-- > 0)
                            putc_f(putc_data, ' ');
                    do {
                        putc_f(putc_data, *str);
                    } while(*str++);
                    if (leftalign)
                        while(size_mod-- > 0)
                            putc_f(putc_data, ' ');
                    break;

                case 'o':
                    switch (length_mod) {
                        case 8:
                            value = va_arg(args, uint64_t);
                            break;
                        case 9:
                            value = va_arg(args, uintmax_t);
                            break;
                        case 10:
                            value = va_arg(args, uintptr_t);
                            break;
                        case 11:
                            value = va_arg(args, size_t);
                            break;
                        default:
                            value = va_arg(args, uint32_t);
                            break;
                    }
                    int_itoa(tmpbuf, 8, value, zero_pad, precision, size_mod, 0, alternate_form, 0, sign);
                    str = tmpbuf;
                    size_mod -= int_strlen(str);
                    if (!leftalign)
                        while(size_mod-- > 0)
                            putc_f(putc_data, ' ');
                    do {
                        putc_f(putc_data, *str);
                    } while(*str++);
                    if (leftalign)
                        while(size_mod-- > 0)
                            putc_f(putc_data, ' ');
                    break;

                case 'c':
                    putc_f(putc_data, va_arg(args, int));
                    break;

                case 's':
                    {
                        str = va_arg(args, char *);
                        do {
                            if (*str == 0)
                                break;
                            else
                                putc_f(putc_data, *str);
                        } while(*str++ && --precision);
                    }
                    break;

                default:
                    putc_f(putc_data, c);
                    break;
            }
        }
    }
}

#define ARM_PERIIOBASE ((uint32_t)io_base)

void waitSerOUT(void *io_base)
{
    while(1)
    {
       if ((rd32le(PL011_0_BASE + PL011_FR) & PL011_FR_TXFF) == 0) break;
    }
}

void putByte(void *io_base, char chr)
{
    waitSerOUT(io_base);

    if (chr == '\n')
    {
        wr32le(PL011_0_BASE + PL011_DR, '\r');
        waitSerOUT(io_base);
    }
    wr32le(PL011_0_BASE + PL011_DR, (uint8_t)chr);
    put_char(chr);
}

void kprintf_pc(putc_func putc_f, void *putc_data, const char * format, ...)
{
    va_list v;
    va_start(v, format);
    vkprintf_pc(putc_f, putc_data, format, v);
    va_end(v);
}

void kprintf(const char * format, ...)
{
    va_list v;
    va_start(v, format);
    vkprintf_pc(putByte, (void*)0xf2000000, format, v);
    va_end(v);
}

char *
strcpy(char *s1, const char *s2)
{
    char *s = s1;
    while ((*s++ = *s2++) != 0)
	;
    return (s1);
}

int
strcmp(const char *s1, const char *s2)
{
    for ( ; *s1 == *s2; s1++, s2++)
	if (*s1 == '\0')
	    return 0;
    return ((*(unsigned char *)s1 < *(unsigned char *)s2) ? -1 : +1);
}

void vkprintf(const char * format, va_list args)
{
    vkprintf_pc(putByte, (void*)0xf2000000, format, args);
}
