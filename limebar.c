#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#include <X11/Xft/Xft.h>

#include "config.h"

#define UNREACHABLE() do { fprintf(stderr, "[ERROR] UNREACHABLE REACHED AT %s:%d:\n", __FILE__, __LINE__); exit(1); } while(0)
#define ARR_COUNT(arr) (sizeof(arr) / sizeof(arr[0]))

typedef struct {
    int x;
    int y;
    int width;
    int height;
} Monitor;

typedef struct {
    Window window;
    XftDraw* draw;
} Bar;

typedef struct {
	struct sigevent* event;
	timer_t* id;
} Timer_Data;

unsigned short usclamp(unsigned short d, unsigned short min, unsigned short max) {
  const unsigned short t = d < min ? min : d;
  return t > max ? max : t;
}

void error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);
}

uint8_t parse_char_hex(char ch) {
    switch(ch) {
        case '0': return 0x0; break;
        case '1': return 0x1; break;
        case '2': return 0x2; break;
        case '3': return 0x3; break;
        case '4': return 0x4; break;
        case '5': return 0x5; break;
        case '6': return 0x6; break;
        case '7': return 0x7; break;
        case '8': return 0x8; break;
        case '9': return 0x9; break;
        case 'A': return 0xA; break;
        case 'B': return 0xB; break;
        case 'C': return 0xC; break;
        case 'D': return 0xD; break;
        case 'E': return 0xE; break;
        case 'F': return 0xF; break;
        case 'a': return 0xa; break;
        case 'b': return 0xb; break;
        case 'c': return 0xc; break;
        case 'd': return 0xd; break;
        case 'e': return 0xe; break;
        case 'f': return 0xf; break;
        default: error("Character '%c' not in hex!", ch); UNREACHABLE(); break;
    }
}

uint8_t parse_byte_hex(const char* text) {
    return parse_char_hex(text[0]) * 16 + parse_char_hex(text[1]);
}

XRenderColor parse_color(const char* text) {
    const char* example_text = "#11223344";
    if(strlen(text) != strlen(example_text)) {
        error("Color '%s' has wrong lenght! Colors are in format '#RRGGBBAA' hex!", text);
    }
    if(text[0] != '#') error("Color '%s' has to start with #! Colors are in format '#RRGGBBAA' hex!", text);
    XRenderColor color = {0};
    color.red   = (uint16_t)parse_byte_hex(text + 1 + (0 * 2)) * 257;
    color.green = (uint16_t)parse_byte_hex(text + 1 + (1 * 2)) * 257;
    color.blue  = (uint16_t)parse_byte_hex(text + 1 + (2 * 2)) * 257;
    color.alpha = (uint16_t)parse_byte_hex(text + 1 + (3 * 2)) * 257;
    color.red   = usclamp(color.red,   0, color.alpha);
    color.green = usclamp(color.green, 0, color.alpha);
    color.blue  = usclamp(color.blue,  0, color.alpha);
    return color;
}

void query_monitors(Display* display, Window root, int* monitors_count, Monitor** monitors){
    XRRScreenResources* xrr_res = XRRGetScreenResources(display, root);
    (*monitors_count) = xrr_res->noutput;
    (*monitors) = malloc(sizeof(Monitor) * (*monitors_count));
    for(int i = 0; i < xrr_res->noutput; i++) {
        XRROutputInfo* output_info = XRRGetOutputInfo(display, xrr_res, xrr_res->outputs[i]);
        if (output_info->connection == RR_Connected) {
            if(output_info->ncrtc != 1) {
                error("ncrtc value of Xrandr output '%.*s' is '%d' not 1!", output_info->nameLen, output_info->name, output_info->ncrtc);
            }
            XRRCrtcInfo* crtc_info = XRRGetCrtcInfo(display, xrr_res, output_info->crtcs[0]);
            (*monitors)[i].x = crtc_info->x;
            (*monitors)[i].y = crtc_info->y;
            (*monitors)[i].width = crtc_info->width;
            (*monitors)[i].height = crtc_info->height;
            XRRFreeCrtcInfo(crtc_info);
        }else{
            (*monitors_count)--;
        }
        XRRFreeOutputInfo (output_info);
    }
    XRRFreeScreenResources(xrr_res);
}

int main(int argc, char** argv) {
    (void) argc;
    (void) argv;
    int ret = 0;

    char* display_name = getenv("DISPLAY");

    Display* display = XOpenDisplay(display_name);
    if (display == NULL) error("%p: Failed to open display!", display);

    Window root = DefaultRootWindow(display);
    if (root == None) error("%lu: No root window found!", root);
    
    int monitor_count;
    Monitor* monitors;
    query_monitors(display, root, &monitor_count, &monitors);
    
    XVisualInfo vinfo_template = {0};
    vinfo_template.screen = 0;
    vinfo_template.depth = 32;
    vinfo_template.class = TrueColor;
    int vinfo_count = 0;
    XVisualInfo* vinfo_arr = XGetVisualInfo(display, VisualScreenMask | VisualDepthMask | VisualClassMask, &vinfo_template, &vinfo_count);
    if (!vinfo_arr) error("No 32bit TrueColor visuals found!");
    XVisualInfo vinfo = {0};
    XRenderPictFormat* fmt;
    for (int i = 0; i < vinfo_count; i++) {
        fmt = XRenderFindVisualFormat(display, vinfo_arr[i].visual);
        if (fmt && fmt->type == PictTypeDirect && fmt->direct.alphaMask) {
            vinfo = vinfo_arr[i];
            break;
        }
    }
    if (vinfo.visual == NULL) error("Could not find any visual with alpha!");
    Colormap cmap = {0};
    cmap = XCreateColormap(display, root, vinfo.visual, AllocNone);
    if (cmap == 0) error("Could not create the colormap!");

    XftFont* font = XftFontOpenName(display, 0, FONT);
    if(font == NULL) error("%p: Could not load font!", font);

    XRenderColor fg_rc = parse_color(FG_COLOR);
    XftColor fg_color;
    ret = XftColorAllocValue(display, vinfo.visual, cmap, &fg_rc, &fg_color);
    if(ret != True) error("%d: Could not allocate color!", ret);
    XRenderColor bg_rc = parse_color(BG_COLOR);
    XftColor bg_color;
    ret = XftColorAllocValue(display, vinfo.visual, cmap, &bg_rc, &bg_color);
    if(ret != True) error("%d: Could not allocate color!", ret);
    
    Atom atom_destroy;
    Atom atom_type;
    Atom atom_type_dock;
    Atom atom_strut;
    atom_destroy = XInternAtom(display, "WM_DELETE_WINDOW", False);
    atom_type = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    atom_type_dock = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DOCK", False);
    atom_strut = XInternAtom(display, "_NET_WM_STRUT", False);

    Bar bars[monitor_count];
    for (int i = 0; i < monitor_count; i++) {
        XSetWindowAttributes attr = {0};
        attr.override_redirect = True;
        attr.event_mask = ExposureMask;
        attr.colormap = cmap;
        attr.border_pixel = 0;
        attr.background_pixel = 0;

        bars[i].window = XCreateWindow(display, root, 0, 0, monitors[i].width, BAR_HEIGHT, 0, vinfo.depth, InputOutput, vinfo.visual, CWColormap | CWBorderPixel | CWBackPixel | CWEventMask | CWOverrideRedirect, &attr);
        if (bars[i].window == None) error("%lu: Failed to create window!", bars[i].window);

        const char* base_window_name = "limebar ";
        size_t window_name_size = strlen(base_window_name) + 3;
        char* window_name = malloc(window_name_size);
        snprintf(window_name, window_name_size, "limebar %d", i);
        XStoreName(display, bars[i].window, window_name);
        free(window_name);

        XChangeProperty(display, bars[i].window, atom_type, XA_ATOM, 32, PropModeReplace, (unsigned char *)&atom_type_dock, 1);
        unsigned long strut[4] = {0, 0, BAR_HEIGHT, 0};
        XChangeProperty(display, bars[i].window, atom_strut, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)strut, 4);
        
        XSetWMProtocols(display, bars[i].window, &atom_destroy, 1);

        XMapWindow(display, bars[i].window);
        
        XMoveWindow(display, bars[i].window, monitors[i].x, monitors[i].y);
    
        bars[i].draw = XftDrawCreate(display, bars[i].window, vinfo.visual, cmap);
        if(bars[i].draw == NULL) error("%d: Could not create xft draw!", bars[i].draw);
    }

    clockid_t clockid = CLOCK_MONOTONIC;

    Timer_Data timers_data[ARR_COUNT(blocks)];
    void* blocks_data[ARR_COUNT(blocks)];
    for(size_t i = 0; i < ARR_COUNT(blocks); i++) {
        if(blocks[i].block_init != NULL) blocks_data[i] = blocks[i].block_init();
        //if(blocks[i].interval != 0.f) timer_create(clockid, timers_data[i].event, timers_data[i].id);
    }

    const char* bar_text = "Hello"; // TODO: malloc a string with size of all blocks combined and memcpy them to it.
    size_t bar_text_len = strlen(bar_text);

    int run = monitor_count;
    XEvent event;
    while (run > 0) {
        XNextEvent(display, &event);
        switch(event.type) {
        case Expose:
            for (int i = 0; i < monitor_count; i++) {
                if (event.xexpose.window == bars[i].window) {
                    Picture pict = XftDrawPicture(bars[i].draw);
                    if (pict == None) error("Could not get the draw picture!");
                    XRenderFillRectangle(display, PictOpSrc, pict, &bg_rc, 0, 0, monitors[i].width, BAR_HEIGHT);
                    XftDrawStringUtf8(bars[i].draw, &fg_color, font, 0, font->ascent, (const FcChar8 *)bar_text, bar_text_len);
                }
            }
            break;
        case ClientMessage:
            for(int i = 0; i < monitor_count; i++){
                if(event.xclient.window == bars[i].window) {
                    if((unsigned long)event.xclient.data.l[0] == atom_destroy) {
                        XDestroyWindow(display, bars[i].window);
                        run--;
                        break;
                    }
                }
            }
            break;
        }
    }

    return 0;
}
