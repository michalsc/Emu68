#define DEBUG 1

#include <stdlib.h>
#include <stdint.h>

#include "support.h"

extern "C" void SmallPT();

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
int maximal_ray_depth = 10;

// ca. 650bytes per ray depth, 650KB stack required for max ray depth of 1000

Vec radiance_expl(const Ray &r, int depth, unsigned short *Xi,int E=1){
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

    return obj.e*E+e+f.mult(radiance_expl(Ray(x,d),depth,Xi,0));
  } else if (obj.refl == SPEC)              // Ideal SPECULAR reflection
    return obj.e + f.mult(radiance_expl(Ray(x,r.d-n*2*n.dot(r.d)),depth,Xi));
  Ray reflRay(x, r.d-n*2*n.dot(r.d));     // Ideal dielectric REFRACTION
  bool into = n.dot(nl)>0;                // Ray from outside going in?
  double nc=1, nt=1.5, nnt=into?nc/nt:nt/nc, ddn=r.d.dot(nl), cos2t;
  if ((cos2t=1-nnt*nnt*(1-ddn*ddn))<0)    // Total internal reflection
    return obj.e + f.mult(radiance_expl(reflRay,depth,Xi));
  Vec tdir = (r.d*nnt - n*((into?1:-1)*(ddn*nnt+sqrt(cos2t)))).norm();
  double a=nt-nc, b=nt+nc, R0=a*a/(b*b), c = 1-(into?-ddn:tdir.dot(n));
  double Re=R0+(1-R0)*c*c*c*c*c,Tr=1-Re,P=.25+.5*Re,RP=Re/P,TP=Tr/(1-P);
  return obj.e + f.mult(depth>2 ? (erand48(Xi)<P ?   // Russian roulette
    radiance_expl(reflRay,depth,Xi)*RP:radiance_expl(Ray(x,tdir),depth,Xi)*TP) :
    radiance_expl(reflRay,depth,Xi)*Re+radiance_expl(Ray(x,tdir),depth,Xi)*Tr);
}

Vec radiance(const Ray &r, int depth, unsigned short *Xi)
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
        return obj.e + f.mult(radiance(Ray(x,d),depth,Xi));
    } else if (obj.refl == SPEC)            // Ideal SPECULAR reflection
        return obj.e + f.mult(radiance(Ray(x,r.d-n*2*n.dot(r.d)),depth,Xi));
    Ray reflRay(x, r.d-n*2*n.dot(r.d));     // Ideal dielectric REFRACTION
    bool into = n.dot(nl)>0;                // Ray from outside going in?
    double nc=1, nt=1.5, nnt=into?nc/nt:nt/nc, ddn=r.d.dot(nl), cos2t;
    if ((cos2t=1-nnt*nnt*(1-ddn*ddn))<0)    // Total internal reflection
        return obj.e + f.mult(radiance(reflRay,depth,Xi));
    Vec tdir = (r.d*nnt - n*((into?1:-1)*(ddn*nnt+sqrt(cos2t)))).norm();
    double a=nt-nc, b=nt+nc, R0=a*a/(b*b), c = 1-(into?-ddn:tdir.dot(n));
    double Re=R0+(1-R0)*c*c*c*c*c,Tr=1-Re,P=.25+.5*Re,RP=Re/P,TP=Tr/(1-P);
    return obj.e + f.mult(depth>2 ? (erand48(Xi)<P ?   // Russian roulette
        radiance(reflRay,depth,Xi)*RP:radiance(Ray(x,tdir),depth,Xi)*TP) :
        radiance(reflRay,depth,Xi)*Re+radiance(Ray(x,tdir),depth,Xi)*Tr);
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

static uint16_t *fb;
static uint32_t pitch;
static uint32_t width;
static uint32_t height;

#define TILE_SIZE   32

static const uint32_t render_width = 640;
static const uint32_t render_height = 480;
static const int samps = 10;
static const int explicit_mode = 0;

uint32_t Begin_Time;
uint32_t End_Time;

Vec c[TILE_SIZE * TILE_SIZE];
uint32_t buffer[render_width * render_height];

void RedrawTile(int tile_x, int tile_y)
{
    int start_x = (width - render_width) / 2;
    int start_y = (height - render_height) / 2;

    int out_offset = start_x + tile_x * TILE_SIZE + ((start_y + tile_y * TILE_SIZE) * pitch) / 2;
    int in_offset = tile_x * TILE_SIZE + (tile_y * TILE_SIZE) * render_width;

    kprintf("RedrawTile(%d, %d)\n", tile_x, tile_y);

    for (int y=0; y < TILE_SIZE; y++) {

        for (int x=0; x < TILE_SIZE; x++) {
            uint32_t rgb = buffer[in_offset + x];
            uint8_t r = (rgb & 0xff) >> 3;
            uint8_t g = ((rgb >> 8) & 0xff) >> 2;
            uint8_t b = ((rgb >> 16) & 0xff) >> 3;
            uint16_t c = r | (g << 5) | (b << 11);
            fb[out_offset + x] = LE16(c);
        }

        in_offset += render_width;
        out_offset += pitch/2;
    }
}

void SmallPT()
{
    fb = get_fb();
    pitch = get_pitch();
    width = get_width();
    height = get_height();

    kprintf("[SmallPT] Starting SmallPT Path Tracer\n");
    kprintf("[SmallPT] Screen %dx%d at 0x%08x\n", width, height, fb);
    kprintf("[SmallPT] Rendering image with size %dx%d in %d tiles\n", render_width, render_height,
        (render_width / TILE_SIZE) * (render_height / TILE_SIZE));

    silence(1);

    for (unsigned i=0; i < render_width*render_height; i++)
        buffer[i] = 0;

    Begin_Time = LE32(*(volatile uint32_t*)0xf2003004);

    for (unsigned tile_y = 0; tile_y < (render_height / TILE_SIZE); tile_y++)
    {
        for (unsigned tile_x = 0; tile_x < (render_width / TILE_SIZE); tile_x++)
        {
            if (explicit_mode)
                spheres[0] = Sphere(5, Vec(50,81.6-16.5,81.6),Vec(4,4,4)*20,  Vec(), DIFF);
            else
                spheres[0] = Sphere(600, Vec(50,681.6-.27,81.6),Vec(12,12,12),  Vec(), DIFF);

            Ray cam(Vec(50, 52, 295.6), Vec(0, -0.042612, -1).norm()); // cam pos, dir
            Vec cx = Vec(render_width * .5135 / render_height), cy = (cx % cam.d).norm() * .5135, r;

            for (int i=0; i < TILE_SIZE * TILE_SIZE; i++)
                c[i] = Vec();

            kprintf("tile_y*32 = %d, (tile_y+1)*32=%d\n", tile_y*32, (tile_y+1)*32);

            for (int _y=tile_y * 32; _y < (tile_y + 1) * 32; _y++)
            {
                int y = render_height - _y - 1;

                kprintf(" tile_y*32 = %d, (tile_y+1)*32=%d\n", tile_y*32, (tile_y+1)*32);
                kprintf(" _y=%d, y=%d\n", _y, y);

                unsigned short Xi[3];

                for (int _x=tile_x * 32; _x < (tile_x + 1) * 32; _x++)   // Loop cols
                {
                    Xi[0] = 0;
                    Xi[1] = 0;
                    Xi[2] = y*y*y;
                    int x = _x; // w - _x - 1;
                    for (unsigned sy=0, i=(_y-tile_y*32)*32+_x-tile_x*32; sy<2; sy++)     // 2x2 subpixel rows
                    {
                        for (unsigned sx=0; sx<2; sx++, r=Vec())
                        {        // 2x2 subpixel cols
//                            for (int s=0; s<samps; s++)
                            {
                                double r1=2*erand48(Xi), dx=r1<1 ? sqrt(r1)-1: 1-sqrt(2-r1);
                                double r2=2*erand48(Xi), dy=r2<1 ? sqrt(r2)-1: 1-sqrt(2-r2);
                                Vec d = cx*( ( (sx+.5 + dx)/2 + x)/render_width - .5) +
                                        cy*( ( (sy+.5 + dy)/2 + y)/render_height - .5) + cam.d;
//*(int*)0x100000 = ~0;
                                if (explicit_mode)
                                    r = r + radiance_expl(Ray(cam.o+d*140,d.norm()),0,Xi)*(1./samps);
                                else
                                    r = r + radiance(Ray(cam.o+d*140,d.norm()),0,Xi)*(1./samps);
//*(int*)0x100000 = 0;
                            } // Camera rays are pushed ^^^^^ forward to start in interior
                            r.x = 0.001;
                            r.y = 0.0001;
                            r.z = 0.001;
                            c[i] = c[i] + Vec(clamp(r.x),clamp(r.y),clamp(r.z))*.25;
                        }
                    }
                }
#if 0
                int start_ptr = tile_y*32*render_width + tile_x*32;
                for (int yy=0; yy < 32; yy++)
                {
                    for (int xx=0; xx < 32; xx++)
                    {
                        buffer[start_ptr+xx] = ((toInt(c[(xx+32*yy)].z) & 0xff)) |
                            ((toInt(c[(xx+32*yy)].y) & 0xff) << 8) | ((toInt(c[(xx + 32*yy)].x) & 0xff) << 16);
                    }
                    start_ptr += render_width;
                }
#endif
            }
            RedrawTile(tile_x, tile_y);
        }
    }

    End_Time = LE32(*(volatile uint32_t*)0xf2003004) - Begin_Time;

    silence(0);

    kprintf("[SmallPT] Time consumed: %d.%03d seconds\n", End_Time / 1000000, (End_Time % 1000000) / 1000);
}
