#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/Xft/Xft.h>

#include "config.h"

#define ARR_COUNT(arr) (sizeof(arr) / sizeof(arr[0]))

typedef struct {
    int x;
    int y;
    int width;
    int height;
} Monitor;

typedef struct {
    Window window;
    Atom atom;
    XftDraw* draw;
} Bar;

void error(const char* msg) {
    fprintf(stderr, "[ERROR] %s\n", msg);
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
        default: error("Character not in hex!"); break;
    }
}

uint8_t parse_byte_hex(const char* text) {
    return parse_char_hex(text[0]) * 16 + parse_char_hex(text[1]);
}

XRenderColor parse_color(const char* text) {
    const char* example_text = "#11223344";
    if(strlen(text) != strlen(example_text)) {
        error("Wrong color lenght! Colors are in format '#RRGGBBAA' hex!");
    }
    if(text[0] != '#') error("Wrong color text! Colors are in format '#RRGGBBAA' hex!");
    XRenderColor color = {0};
    color.red   = parse_byte_hex(text + 1 + (0 * 2));
    color.green = parse_byte_hex(text + 1 + (1 * 2));
    color.blue  = parse_byte_hex(text + 1 + (2 * 2));
    color.alpha = parse_byte_hex(text + 1 + (3 * 2));
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
                error("ncrtc value of Xrandr output is not 1!");
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

    char* display_name = getenv("DISPLAY");

    Display* display = XOpenDisplay(display_name);
    if (display == NULL) error("Failed to open display!");

    Window root = DefaultRootWindow(display);
    if (root == None) error("No root window found!");
    
    int monitor_count;
    Monitor* monitors;
    query_monitors(display, root, &monitor_count, &monitors);
    
    Visual* visual = DefaultVisual(display, 0);
    Colormap cmap = DefaultColormap(display, 0);

    XftFont* font = XftFontOpenName(display, 0, FONT);
    if(font == NULL) error("Could not load font!");

    XRenderColor fg_rc = parse_color(FG_COLOR);
    XftColor fg_color;
    XftColorAllocValue(display, visual, cmap, &fg_rc, &fg_color);
    XRenderColor bg_rc = parse_color(BG_COLOR);
    XftColor bg_color;
    XftColorAllocValue(display, visual, cmap, &bg_rc, &bg_color);

    Bar bars[monitor_count];

    for (int i = 0; i < monitor_count; i++) {
        bars[i].window = XCreateSimpleWindow(display, root, 0, 0, monitors[i].width, BAR_HEIGHT, 0, 0, 0xffffffff);
        if (bars[i].window == None) error("Failed to create window!");
        
        XSetWindowAttributes attr = {0};
        attr.override_redirect = true;
        attr.event_mask = ExposureMask;
        XChangeWindowAttributes(display, bars[i].window, CWOverrideRedirect | CWEventMask, &attr);

        const char* base_window_name = "limebar ";
        size_t window_name_size = strlen(base_window_name) + 3;
        char* window_name = malloc(window_name_size);
        snprintf(window_name, window_name_size, "limebar %d", i);
        XStoreName(display, bars[i].window, window_name);
        free(window_name);

        XMapWindow(display, bars[i].window);
        
        bars[i].atom = XInternAtom(display, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(display, bars[i].window, &(bars[i].atom), 1);
        
        XMoveWindow(display, bars[i].window, monitors[i].x, monitors[i].y);
    
        bars[i].draw = XftDrawCreate(display, bars[i].window, visual, cmap);
        if(bars[i].draw == NULL) error("Could not create xft draw!");
    }

    const char* bar_text = "Hello";
    size_t bar_text_len = strlen(bar_text);

    int run = monitor_count;
    XEvent event;
    while (run > 0) {
        XNextEvent(display, &event);

        switch(event.type) {
        case Expose:
            for (int i = 0; i < monitor_count; i++) {
                if (event.xexpose.window == bars[i].window) {
                    // \/ WTF?
                    printf("COL: %lu\n", fg_color.pixel);
                    XftDrawStringUtf8(bars[i].draw, &fg_color, font, 0, font->ascent, (const FcChar8 *)bar_text, bar_text_len);
                }
            }
            break;
        case ClientMessage:
            for(int i = 0; i < monitor_count; i++){
                if(event.xclient.window == bars[i].window) {
                    if((unsigned long)event.xclient.data.l[0] == bars[i].atom) {
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
