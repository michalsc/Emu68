

#define DEBUG 1
#if 0
#include <math.h>   // smallpt, a Path Tracer by Kevin Beason, 2008
#endif
#include <stdlib.h>
#include <stdint.h>

inline double sqrt(double a)
{
    double ret;

    asm volatile("fsqrt %1, %0":"=f"(ret):"f"(a));

    return ret;
}

inline double fabs(double a)
{
    double ret;

    asm volatile("fabs %1, %0":"=f"(ret):"f"(a));

    return ret;
}

inline double cos(double a)
{
    double ret;

    asm volatile("fcos %1, %0":"=f"(ret):"f"(a));

    return ret;
}

inline double sin(double a)
{
    double ret;

    asm volatile("fsin %1, %0":"=f"(ret):"f"(a));

    return ret;
}

inline double _M_PI()
{
    double ret;

    asm volatile("fmovecr %1,%0":"=f"(ret):"i"(0));

    return ret;
}

static const double
two54   =  1.80143985094819840000e+16, /* 0x43500000, 0x00000000 */
twom54  =  5.55111512312578270212e-17, /* 0x3C900000, 0x00000000 */
huge   = 1.0e+300,
tiny   = 1.0e-300;

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

#define M_PI (_M_PI())
#define M_1_PI 0.31830988618379067154

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
    n.x[0] = 0;

    Xi[0] = n.x[1];
    Xi[1] = n.x[2];
    Xi[2] = n.x[3];

    n1.u64 = (n.u64 << 4) | 0x3fe0000000000000ULL;

    return n1.d;
}

struct Vec {        // Usage: time ./smallpt 5000 && xv image.ppm
    double x;
    double y;
    double z; // position, also color (r,g,b)

    Vec(double x_ = 0, double y_ = 0, double z_ = 0)
    {
        x = x_;
        y = y_;
        z = z_;
    }

    Vec operator+(const Vec &b) const { return Vec(x + b.x, y + b.y, z + b.z); }
    Vec operator-(const Vec &b) const { return Vec(x - b.x, y - b.y, z - b.z); }
    Vec operator*(double b) const { return Vec(x * b, y * b, z * b); }
    Vec mult(const Vec &b) const { return Vec(x * b.x, y * b.y, z * b.z); }
    Vec& norm() { return *this = *this * (1/sqrt(x * x + y * y + z * z)); }
    double dot(const Vec &b) const { return x * b.x + y * b.y + z * b.z; } // cross:
    Vec operator%(Vec &b){ return Vec(y * b.z - z * b.y, z * b.x - x * b.z, x * b.y - y * b.x);}
};

struct Ray {
    Vec o, d;
    Ray(Vec o_, Vec d_) : o(o_), d(d_) {}
};

enum Refl_t
{
    DIFF,
    SPEC,
    REFR
}; // material types, used in radiance()

struct Sphere
{
    double rad;  // radius
    Vec p, e, c; // position, emission, color
    Refl_t refl; // reflection type (DIFFuse, SPECular, REFRactive)
    Sphere(double rad_, Vec p_, Vec e_, Vec c_, Refl_t refl_) : rad(rad_), p(p_), e(e_), c(c_), refl(refl_) {}
    double intersect(const Ray &r) const
    {                     // returns distance, 0 if nohit
        Vec op = p - r.o; // Solve t^2*d.d + 2*t*(o-p).d + (o-p).(o-p)-R^2 = 0
        double t, eps = 1e-4, b = op.dot(r.d), det = b * b - op.dot(op) + rad * rad;
        if (det < 0)
            return 0;
        else
            det = sqrt(det);
        return (t = b - det) > eps ? t : ((t = b + det) > eps ? t : 0);
    }
};

Sphere spheres[] = {//Scene: radius, position, emission, color, material
   Sphere(600, Vec(50,681.6-.27,81.6),Vec(12,12,12),  Vec(), DIFF), //Lite
   Sphere(1e5, Vec( 1e5+1,40.8,81.6), Vec(),Vec(.75,.25,.25),DIFF),//Left
   Sphere(1e5, Vec(-1e5+99,40.8,81.6),Vec(),Vec(.25,.25,.75),DIFF),//Rght
   Sphere(1e5, Vec(50,40.8, 1e5),     Vec(),Vec(.55,.55,.55),DIFF),//Back
   Sphere(1e5, Vec(50,40.8,-1e5+170), Vec(),Vec(),           DIFF),//Frnt
   Sphere(1e5, Vec(50, 1e5, 81.6),    Vec(),Vec(.75,.75,.75),DIFF),//Botm
   Sphere(1e5, Vec(50,-1e5+81.6,81.6),Vec(),Vec(.75,.75,.75),DIFF),//Top
   Sphere(16.5,Vec(27,16.5,47),       Vec(),Vec(.4,.4,.3)*.999, SPEC),//Mirr
   Sphere(16.5,Vec(73,16.5,78),       Vec(),Vec(.8,.7,.95)*.999, REFR),//Glas
   Sphere(10.5,Vec(23,10.5,98),       Vec(),Vec(.6,1,0.7)*.999, REFR),//Glas
   Sphere(8.,Vec(50,8.,108),       Vec(),Vec(1,0.6,0.7)*.999, REFR),//Glas
   Sphere(6.5, Vec(53,6.5,48),       Vec(),Vec(0.3,.4,.4)*.999, SPEC),//Mirr
 };

const int numSpheres = sizeof(spheres)/sizeof(Sphere);

inline double clamp(double x)
{
    return x<0 ? 0 : x>1 ? 1 : x;
}

inline int toInt(double x)
{
    return int(pow(clamp(x),1/2.2)*255+.5);
}

inline bool intersect(const Ray &r, double &t, int &id)
{
    double n=numSpheres, d, inf=t=1e20;
    for(int i=int(n);i--;) if((d=spheres[i].intersect(r))&&d<t){t=d;id=i;}
    return t<inf;
}
int maximal_ray_depth = 1000;

// ca. 650bytes per ray depth, 650KB stack required for max ray depth of 1000

Vec radiance_expl(struct Task *me, const Ray &r, int depth, unsigned short *Xi,int E=1){
  double t;                               // distance to intersection
  int id=0;                               // id of intersected object
  if (!intersect(r, t, id)) return Vec(); // if miss, return black
  const Sphere &obj = spheres[id];        // the hit object
  Vec x=r.o+r.d*t, n=(x-obj.p).norm(), nl=n.dot(r.d)<0?n:n*-1, f=obj.c;
  double p = f.x>f.y && f.x>f.z ? f.x : f.y>f.z ? f.y : f.z; // max refl
  depth++;

  if (depth > maximal_ray_depth)
    return obj.e*E;
  else
  if (depth>10||!p) { // From depth 10 start Russian roulette
       if (erand48(Xi)<p) f=f*(1/p); else return obj.e*E;
    }
  if (obj.refl == DIFF){                  // Ideal DIFFUSE reflection
    double r1=2.0*M_PI*erand48(Xi), r2=erand48(Xi), r2s=sqrt(r2);
    Vec w=nl, u=((fabs(w.x)>.1?Vec(0,1):Vec(1))%w).norm(), v=w%u;
    Vec d = (u*cos(r1)*r2s + v*sin(r1)*r2s + w*sqrt(1-r2)).norm();

    // Loop over any lights
    Vec e;
    for (int i=0; i<numSpheres; i++){
      const Sphere &s = spheres[i];
      if (s.e.x<=0 && s.e.y<=0 && s.e.z<=0) continue; // skip non-lights

      Vec sw=s.p-x, su=((fabs(sw.x)>.1?Vec(0,1):Vec(1))%sw).norm(), sv=sw%su;
      double cos_a_max = sqrt(1-s.rad*s.rad/(x-s.p).dot(x-s.p));
      double eps1 = erand48(Xi), eps2 = erand48(Xi);
      double cos_a = 1-eps1+eps1*cos_a_max;
      double sin_a = sqrt(1-cos_a*cos_a);
      double phi = 2*M_PI*eps2;
      Vec l = su*cos(phi)*sin_a + sv*sin(phi)*sin_a + sw*cos_a;
      l.norm();
      if (intersect(Ray(x,l), t, id) && id==i){  // shadow ray
        double omega = 2*M_PI*(1-cos_a_max);
        e = e + f.mult(s.e*l.dot(nl)*omega)*M_1_PI;  // 1/pi for brdf
      }
    }

    return obj.e*E+e+f.mult(radiance_expl(me, Ray(x,d),depth,Xi,0));
  } else if (obj.refl == SPEC)              // Ideal SPECULAR reflection
    return obj.e + f.mult(radiance_expl(me, Ray(x,r.d-n*2*n.dot(r.d)),depth,Xi));
  Ray reflRay(x, r.d-n*2*n.dot(r.d));     // Ideal dielectric REFRACTION
  bool into = n.dot(nl)>0;                // Ray from outside going in?
  double nc=1, nt=1.5, nnt=into?nc/nt:nt/nc, ddn=r.d.dot(nl), cos2t;
  if ((cos2t=1-nnt*nnt*(1-ddn*ddn))<0)    // Total internal reflection
    return obj.e + f.mult(radiance_expl(me, reflRay,depth,Xi));
  Vec tdir = (r.d*nnt - n*((into?1:-1)*(ddn*nnt+sqrt(cos2t)))).norm();
  double a=nt-nc, b=nt+nc, R0=a*a/(b*b), c = 1-(into?-ddn:tdir.dot(n));
  double Re=R0+(1-R0)*c*c*c*c*c,Tr=1-Re,P=.25+.5*Re,RP=Re/P,TP=Tr/(1-P);
  return obj.e + f.mult(depth>2 ? (erand48(Xi)<P ?   // Russian roulette
    radiance_expl(me, reflRay,depth,Xi)*RP:radiance_expl(me, Ray(x,tdir),depth,Xi)*TP) :
    radiance_expl(me, reflRay,depth,Xi)*Re+radiance_expl(me, Ray(x,tdir),depth,Xi)*Tr);
}

Vec radiance(struct Task *me, const Ray &r, int depth, unsigned short *Xi)
{
    double t; // distance to intersection
    int id=0; // id of intersected object

    if (!intersect(r, t, id))
        return Vec(); // if miss, return black

    const Sphere &obj = spheres[id];        // the hit object

    Vec x=r.o+r.d*t, n=(x-obj.p).norm(), nl=n.dot(r.d)<0?n:n*-1, f=obj.c;

    depth++;

    // Above maximal_ray_depth break recursive loop unconditionally
    if (depth > maximal_ray_depth)
        return obj.e;
    else
    if (depth>5) // From depth of 5 start Russian roulette
    {
        double p = f.x>f.y && f.x>f.z ? f.x : f.y>f.z ? f.y : f.z; // max refl

        if (erand48(Xi)<p)
            f=f*(1/p);
        else return obj.e; //R.R.
    }
    if (obj.refl == DIFF){                  // Ideal DIFFUSE reflection
        double r1=2*M_PI*erand48(Xi), r2=erand48(Xi), r2s=sqrt(r2);
        Vec w=nl, u=((fabs(w.x)>.1?Vec(0,1):Vec(1))%w).norm(), v=w%u;
        Vec d = (u*cos(r1)*r2s + v*sin(r1)*r2s + w*sqrt(1-r2)).norm();
        return obj.e + f.mult(radiance(me, Ray(x,d),depth,Xi));
    } else if (obj.refl == SPEC)            // Ideal SPECULAR reflection
        return obj.e + f.mult(radiance(me, Ray(x,r.d-n*2*n.dot(r.d)),depth,Xi));
    Ray reflRay(x, r.d-n*2*n.dot(r.d));     // Ideal dielectric REFRACTION
    bool into = n.dot(nl)>0;                // Ray from outside going in?
    double nc=1, nt=1.5, nnt=into?nc/nt:nt/nc, ddn=r.d.dot(nl), cos2t;
    if ((cos2t=1-nnt*nnt*(1-ddn*ddn))<0)    // Total internal reflection
        return obj.e + f.mult(radiance(me, reflRay,depth,Xi));
    Vec tdir = (r.d*nnt - n*((into?1:-1)*(ddn*nnt+sqrt(cos2t)))).norm();
    double a=nt-nc, b=nt+nc, R0=a*a/(b*b), c = 1-(into?-ddn:tdir.dot(n));
    double Re=R0+(1-R0)*c*c*c*c*c,Tr=1-Re,P=.25+.5*Re,RP=Re/P,TP=Tr/(1-P);
    return obj.e + f.mult(depth>2 ? (erand48(Xi)<P ?   // Russian roulette
        radiance(me, reflRay,depth,Xi)*RP:radiance(me, Ray(x,tdir),depth,Xi)*TP) :
        radiance(me, reflRay,depth,Xi)*Re+radiance(me, Ray(x,tdir),depth,Xi)*Tr);
}
#if 0

extern "C" void RenderTile(struct ExecBase *SysBase, struct MsgPort *masterPort, struct MsgPort **myPort)
{
    Vec *c;
    struct MyMessage *msg;
    struct MyMessage *m;
    struct MsgPort *port = CreateMsgPort();
    struct MsgPort *syncPort = CreateMsgPort();
    struct MinList msgPool;
    struct Task *me = FindTask(NULL);

    c = (Vec *)AllocMem(sizeof(Vec) * TILE_SIZE * TILE_SIZE, MEMF_ANY | MEMF_CLEAR);

    *myPort = port;

    FreeSignal(syncPort->mp_SigBit);
    syncPort->mp_SigBit = -1;
    syncPort->mp_Flags = PA_IGNORE;

    NEWLIST(&msgPool);

    D(bug("[SMP-SmallPT-Task] hello, msgport=%p\n", port));

    msg = (struct MyMessage *)AllocMem(sizeof(struct MyMessage) * 20, MEMF_PUBLIC | MEMF_CLEAR);
    for (int i=0; i < 20; i++)
        FreeMsg(&msgPool, &msg[i]);

    if (port)
    {
        ULONG signals;
        BOOL doWork = TRUE;
        BOOL redraw = TRUE;

        m = AllocMsg(&msgPool);
        if (m)
        {
            /* Tell renderer that we are bored and want to do some work */
            m->mm_Message.mn_ReplyPort = port;
            m->mm_Type = MSG_HUNGRY;
            PutMsg(masterPort, &m->mm_Message);
        }

        D(bug("[SMP-SmallPT-Task] Just told renderer I'm hungry\n"));

        do {
            signals = Wait(SIGBREAKF_CTRL_C | (1 << port->mp_SigBit));

            if (signals & (1 << port->mp_SigBit))
            {
                while ((m = (struct MyMessage *)GetMsg(syncPort)))
                {
                    FreeMsg(&msgPool, m);
                    redraw = TRUE;
                }
                while ((m = (struct MyMessage *)GetMsg(port)))
                {
                    if (m->mm_Message.mn_Node.ln_Type == NT_REPLYMSG)
                    {
                        FreeMsg(&msgPool, m);
                        continue;
                    }
                    else
                    {
                        if (m->mm_Type == MSG_DIE)
                        {
                            doWork = FALSE;
                            ReplyMsg(&m->mm_Message);
                        }
                        else if (m->mm_Type == MSG_RENDERTILE)
                        {
                            struct tileWork *tile = m->mm_Body.RenderTile.tile;
                            int w = m->mm_Body.RenderTile.width;
                            int h = m->mm_Body.RenderTile.height;
                            int samps = m->mm_Body.RenderTile.numberOfSamples;
                            ULONG *buffer = m->mm_Body.RenderTile.buffer;
                            int tile_x = m->mm_Body.RenderTile.tile->x;
                            int tile_y = m->mm_Body.RenderTile.tile->y;
                            struct MsgPort *guiPort = m->mm_Body.RenderTile.guiPort;
                            int explicit_mode = m->mm_Body.RenderTile.explicitMode;

                            if (explicit_mode)
                                spheres[0] = Sphere(5, Vec(50,81.6-16.5,81.6),Vec(4,4,4)*20,  Vec(), DIFF);
                            else
                                spheres[0] = Sphere(600, Vec(50,681.6-.27,81.6),Vec(12,12,12),  Vec(), DIFF);

                            ReplyMsg(&m->mm_Message);

//__prepare();

                            Ray cam(Vec(50, 52, 295.6), Vec(0, -0.042612, -1).norm()); // cam pos, dir
                            Vec cx = Vec(w * .5135 / h), cy = (cx % cam.d).norm() * .5135, r;

                            for (int i=0; i < TILE_SIZE * TILE_SIZE; i++)
                                c[i] = Vec();

                            for (int _y=tile_y * 32; _y < (tile_y + 1) * 32; _y++)
                            {
                                int y = h - _y - 1;
                                for (unsigned short _x=tile_x * 32, Xi[3]={0,0,(UWORD)(y*y*y)}; _x < (tile_x + 1) * 32; _x++)   // Loop cols
                                {
                                    int x = _x; // w - _x - 1;
                                    for (int sy=0, i=(_y-tile_y*32)*32+_x-tile_x*32; sy<2; sy++)     // 2x2 subpixel rows
                                    {
                                        for (int sx=0; sx<2; sx++, r=Vec())
                                        {        // 2x2 subpixel cols
                                            for (int s=0; s<samps; s++)
                                            {
                                                double r1=2*erand48(Xi), dx=r1<1 ? sqrt(r1)-1: 1-sqrt(2-r1);
                                                double r2=2*erand48(Xi), dy=r2<1 ? sqrt(r2)-1: 1-sqrt(2-r2);
                                                Vec d = cx*( ( (sx+.5 + dx)/2 + x)/w - .5) +
                                                        cy*( ( (sy+.5 + dy)/2 + y)/h - .5) + cam.d;
                                                if (explicit_mode)
                                                    r = r + radiance_expl(me, Ray(cam.o+d*140,d.norm()),0,Xi)*(1./samps);
                                                else
                                                    r = r + radiance(me, Ray(cam.o+d*140,d.norm()),0,Xi)*(1./samps);
                                            } // Camera rays are pushed ^^^^^ forward to start in interior
                                            c[i] = c[i] + Vec(clamp(r.x),clamp(r.y),clamp(r.z))*.25;
                                        }
                                    }
                                }
                                int start_ptr = tile_y*32*w + tile_x*32;
                                for (int yy=0; yy < 32; yy++)
                                {
                                    for (int xx=0; xx < 32; xx++)
                                    {
                                        buffer[start_ptr+xx] = ((toInt(c[(xx+32*yy)].z) & 0xff) << 24) |
                                            ((toInt(c[(xx+32*yy)].y) & 0xff) << 16) | ((toInt(c[(xx + 32*yy)].x) & 0xff) << 8) | 0xff;
                                    }
                                    start_ptr += w;
                                }

    #if 1
                                if (redraw)
                                {
                                    m = AllocMsg(&msgPool);
                                    if (m)
                                    {
                                        m->mm_Message.mn_ReplyPort = syncPort;
                                        m->mm_Type = MSG_REDRAWTILE;
                                        m->mm_Body.RedrawTile.TileX = tile_x;
                                        m->mm_Body.RedrawTile.TileY = tile_y;
                                        PutMsg(guiPort, &m->mm_Message);
                                        redraw = FALSE;
                                    }
                                }
                                else if ((m = (struct MyMessage *)GetMsg(syncPort)))
                                {
                                    FreeMsg(&msgPool, m);
                                    redraw = TRUE;
                                }
    #else
                                (void)syncPort;
                                Signal((struct Task *)guiPort->mp_SigTask, SIGBREAKF_CTRL_D);
    #endif
                            }
//__test();
//                            Signal((struct Task *)guiPort->mp_SigTask, SIGBREAKF_CTRL_D);

                            m = AllocMsg(&msgPool);
                            if (m)
                            {
                                m->mm_Message.mn_ReplyPort = port;
                                m->mm_Type = MSG_REDRAWTILE;
                                m->mm_Body.RedrawTile.TileX = tile_x;
                                m->mm_Body.RedrawTile.TileY = tile_y;
                                PutMsg(guiPort, &m->mm_Message);

                                redraw = TRUE;
                            }

                            m = AllocMsg(&msgPool);
                            if (m)
                            {
                                m->mm_Message.mn_ReplyPort = port;
                                m->mm_Type = MSG_RENDERREADY;
                                m->mm_Body.RenderTile.tile = tile;
                                PutMsg(masterPort, &m->mm_Message);
                            }

                            m = AllocMsg(&msgPool);
                            if (m)
                            {
                                m->mm_Message.mn_ReplyPort = port;
                                m->mm_Type = MSG_HUNGRY;
                                PutMsg(masterPort, &m->mm_Message);
                            }
                        }
                    }
                }
            }

            if (signals & SIGBREAKB_CTRL_C)
                doWork = FALSE;

        } while(doWork);
    }
    D(bug("[SMP-SmallPT-Task] cleaning up stuff\n"));
    FreeMem(msg, sizeof(struct MyMessage) * 20);
    DeleteMsgPort(port);
    DeleteMsgPort(syncPort);

    FreeMem(c, sizeof(Vec) * TILE_SIZE * TILE_SIZE);
}
#endif
