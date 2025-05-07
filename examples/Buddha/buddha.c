
#include "support.h"

static uint16_t *fb;
static uint32_t pitch;
static uint32_t width;
static uint32_t height;

#define render_width    400
#define render_height   400
#define max_work        4000
#define oversample      4
#define buddha          1
#define redraw_step     0xffff

// over 5
// max 2000

uint32_t Begin_Time;
uint32_t End_Time;

uint32_t buffer[render_width * render_height];

void RedrawBuffer()
{
    int start_x = (width - render_width) / 2;
    int start_y = (height - render_height) / 2;

    int out_offset = start_x + (start_y * pitch) / 2;
    int in_offset = 0;

    for (int y=0; y < render_height; y++) {

        for (int x=0; x < render_width; x++) {
            uint32_t rgb = buffer[in_offset + x];
            uint32_t r=96*rgb,g=128*rgb,b=256*rgb;

            b = (b + 1023) / 2048;
            r = (r + 1023) / 2048;
            g = (g + 511) / 1024;

            if (b > 31) b = 31;
            if (r > 31) r = 31;
            if (g > 63) g = 63;

            uint16_t c = b | (g << 5) | (r << 11);
            fb[out_offset + x] = LE16(c);
        }
        in_offset += render_width;
        out_offset += pitch/2;
    }
}

void RedrawBufferLine(int line)
{
    int start_x = (width - render_width) / 2;
    int start_y = line + (height - render_height) / 2;

    int out_offset = start_x + (start_y * pitch) / 2;
    int in_offset = render_width * line;

    for (int x=0; x < render_width; x++) {
        uint32_t rgb = buffer[in_offset + x];
        uint32_t r=96*rgb,g=128*rgb,b=256*rgb;

        b = (b + 1023) / 2048;
        r = (r + 1023) / 2048;
        g = (g + 511) / 1024;

        if (b > 31) b = 31;
        if (r > 31) r = 31;
        if (g > 63) g = 63;

        uint16_t c = b | (g << 5) | (r << 11);
        fb[out_offset + x] = LE16(c);
    }
}

typedef struct {
	double r;
	double i;
} complexno_t;

complexno_t workTrajectories[max_work];

unsigned long calculateTrajectory(double r, double i)
{
    double realNo, imaginaryNo, realNo2, imaginaryNo2, tmp;
    unsigned long trajectory;

    /* Calculate trajectory */
    realNo = 0;
    imaginaryNo = 0;

    for(trajectory = 0; trajectory < max_work; trajectory++)
    {
        /* Check if it's out of circle with radius 2 */
        realNo2 = realNo * realNo;
        imaginaryNo2 = imaginaryNo * imaginaryNo;

        if (realNo2 + imaginaryNo2 > 4.0)
            return trajectory;

        /* Next */
        tmp = realNo2 - imaginaryNo2 + r;
        imaginaryNo = 2.0 * realNo * imaginaryNo + i;
        realNo = tmp;

        if (buddha)
        {
            /* Store */
            workTrajectories[trajectory].r = realNo;
            workTrajectories[trajectory].i = imaginaryNo;
        }
    }

    return 0;
}

void Buddha()
{
    fb = get_fb();
    pitch = get_pitch();
    width = get_width();
    height = get_height();
    unsigned long trajectoryLength;
    unsigned long current;
    unsigned long total_path_lo = 0;
    unsigned long total_path_hi = 0;

#if buddha
    kprintf("[Buddha] Starting Buddhabrot fractal generator\n");
#else
    kprintf("[Buddha] Starting fractal generator\n");
#endif
    kprintf("[Buddha] Screen %dx%d at 0x%08x\n", width, height, fb);
    kprintf("[Buddha] Rendering image with size %dx%d\n", render_width, render_height);

    silence(1);

    for (unsigned ii=0; ii < render_width*render_height; ii++)
        buffer[ii] = 0;

    double x, y;
    double diff = 4.0 / ((double)(render_width * oversample));
    double diff_y = 4.0 / ((double)(render_height * oversample));
    double y_base = 2.0 - (diff / 2.0);
    double diff_sr = 4.0 / (double)render_width;

    Begin_Time = LE32(*(volatile uint32_t*)0xf2003004);
    const unsigned long line_size = render_width * oversample;
    unsigned long line_pos = 0;
    unsigned long line_number = 0;

    for (current = 0; current < (render_width * render_height * oversample * oversample); current++)
    {
        unsigned long val;

        /* Locate the point on the complex plane */
        x = ((double)(current % (render_width * oversample))) * diff - 2.0;
        y = ((double)(current / (render_width * oversample))) * diff - y_base;

        //kprintf("current=%d, x=%d, y=%d, ", current, (int)(1000*x), (int)(1000*y));

        /* Calculate the points trajectory ... */
        trajectoryLength = calculateTrajectory(x, y);

        total_path_lo += trajectoryLength;
        if (total_path_lo >= 1000000000)
        {
            total_path_lo -= 1000000000;
            total_path_hi++;
        }

        //kprintf("len=%d\n", trajectoryLength);

        if (buddha)
        {
            /* Update the display if it escapes */
            if (trajectoryLength > 0)
            {
                unsigned long pos;
                int i;

                for(i = 0; i < trajectoryLength; i++)
                {
                    unsigned long px = (workTrajectories[i].r + 2.0) / diff_sr;
                    unsigned long py = (workTrajectories[i].i + y_base) / diff_sr;

                    if (px < render_width && py < render_height)
                    {
                        pos = (unsigned long)(render_width * py + px);

                        val = buffer[pos];

                        if (val < 0xfff)
                            val++;

                        buffer[pos] = val;
                    }
                }
            }
            if ((current & redraw_step) == 0)
                RedrawBuffer();
        }
        else
        {
            unsigned long px = line_pos / oversample;
            unsigned long py = line_number / oversample;

            val = buffer[px + py * render_width];
            val += trajectoryLength;
            if (val > 0xfff)
                val = 0xfff;
            buffer[px + py * render_width] = val;
            line_pos++;
            if (line_pos == line_size) {
                line_pos = 0;
                RedrawBufferLine(py);
                line_number++;
            }
        }

    }
    RedrawBuffer();

    End_Time = LE32(*(volatile uint32_t*)0xf2003004) - Begin_Time;

    silence(0);

    kprintf("[Buddha] Time consumed: %d.%03d seconds\n", End_Time / 1000000, (End_Time % 1000000) / 1000);
    kprintf("[Buddha] Total calculation iterations: %d'%09d\n", total_path_hi, total_path_lo);
}
