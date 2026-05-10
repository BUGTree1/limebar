#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
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

void error(const char* msg) {
    fprintf(stderr, "[ERROR] %s\n", msg);
    exit(1);
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

    Window windows[monitor_count];
    Atom atoms[monitor_count];

    for (int i = 0; i < monitor_count; i++) {
        windows[i] = XCreateSimpleWindow(display, root, 0, 0, monitors[i].width, BAR_HEIGHT, 0, 0, 0xffffffff);
        if (windows[i] == None) error("Failed to create window!");
        
        XSetWindowAttributes attr = {0};
        attr.override_redirect = true;
        XChangeWindowAttributes(display, windows[i], CWOverrideRedirect, &attr);

        const char* base_window_name = "limebar ";
        size_t window_name_size = strlen(base_window_name) + 3;
        char* window_name = malloc(window_name_size);
        snprintf(window_name, window_name_size, "limebar %d", i);
        XStoreName(display, windows[i], window_name);

        XMapWindow(display, windows[i]);
        
        atoms[i] = XInternAtom(display, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(display, windows[i], &(atoms[i]), 1);
        
        XMoveWindow(display, windows[i], monitors[i].x, monitors[i].y);
    
        Visual *visual = DefaultVisual(display, i);
        Colormap cmap = DefaultColormap(display, i);
        
        XftDraw *draw = XftDrawCreate(display, windows[i], visual, cmap);
        if(draw == NULL) error("Could not create xft draw!");

        XftFont *font = XftFontOpenName(display, i, FONT);
        if(font == NULL) error("Could not load font!");

        XRenderColor rc = {0xffff, 0x0000, 0x0000, 0xffff}; // red
        XftColor color;
        XftColorAllocValue(display, visual, cmap, &rc, &color);
        printf("HELP ME\n");
        
        XftDrawStringUtf8(draw, &color, font, 20, 40, (const FcChar8 *)"Hello, Xft", 10);

        XftColorFree(display, visual, cmap, &color);
        XftFontClose(display, font);
        XftDrawDestroy(draw);
    }

    int run = monitor_count;
    XEvent event;
    while (run > 0) {
        XNextEvent(display, &event);

        switch(event.type) {
        case ClientMessage:
            for(int i = 0; i < monitor_count; i++){
                if(event.xclient.window == windows[i]) {
                    if((unsigned long)event.xclient.data.l[0] == atoms[i]) {
                        XDestroyWindow(display, windows[i]);
                        run--;
                        break;
                    }
                }
            }
            break;
        }
    }

    XCloseDisplay(display);

    return 0;
}
