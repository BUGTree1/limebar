#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <signal.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#include <X11/Xft/Xft.h>

#include "config.h"

#define UNUSED(val) (void)(val)
#define ARR_COUNT(arr) (sizeof(arr) / sizeof(arr[0]))
#define UNREACHABLE() do { fprintf(stderr, "[ERROR] UNREACHABLE REACHED AT %s:%d:\n", __FILE__, __LINE__); exit(1); } while(0)

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
	timer_t id;
} Timer_Data;

typedef struct {
    Display* display;
    Window root;
    XVisualInfo vinfo;
    Colormap cmap;
    XftFont* font;

    XRenderColor fg_rc;
    XftColor fg_color;
    XRenderColor bg_rc;
    XftColor bg_color;

    int monitor_count;
    Monitor* monitors;
    Bar* bars;

    void** blocks_data;
    char** blocks_text;
    size_t* blocks_text_len;
    Timer_Data* timers_data;
    char* bar_text;
    size_t bar_text_len;
} Bar_State;

typedef struct {
    Bar_State* state;
    size_t idx;
} Block_Timer_Data;

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

void draw_bar(Bar_State* state, int window_idx) {
    Picture pict = XftDrawPicture(state->bars[window_idx].draw);
    if (pict == None) error("Could not get the draw picture!");
    XRenderFillRectangle(state->display, PictOpSrc, pict, &state->bg_rc, 0, 0, state->monitors[window_idx].width, BAR_HEIGHT);
    XftDrawStringUtf8(state->bars[window_idx].draw, &state->fg_color, state->font, 0, state->font->ascent, (const FcChar8 *)state->bar_text, state->bar_text_len);
    XFlush(state->display);
}

void update_bar_text(Bar_State* state) {
    size_t progress = 0;
    for(size_t i = 0; i < ARR_COUNT(blocks); i++) {
        memcpy((void*)(state->bar_text + progress), state->blocks_text[i], state->blocks_text_len[i] + 1);
        progress += state->blocks_text_len[i];
    }
}

void update_block(Bar_State* state, size_t block_idx){
    char* new_text = blocks[block_idx].display(state->blocks_data[block_idx]);
    size_t new_text_len = strlen(new_text);
    
    // TODO: \/ Can be optimized to memcpy text before this block, memcpy this block text, memcpy text after this block
    if(state->blocks_text_len[block_idx] != new_text_len) {
        char* new_bar_text = malloc((state->bar_text_len - state->blocks_text_len[block_idx]) + new_text_len);
        state->blocks_text_len[block_idx] = new_text_len;
        update_bar_text(state);
    } else {
        state->blocks_text[block_idx] = new_text;
        update_bar_text(state);
    }

    for (int i = 0; i < state->monitor_count; i++) {
        draw_bar(state, i);
    }
}

void block_timer_expired(int sig, siginfo_t *si, void *uc){
    UNUSED(sig);
    UNUSED(uc);
    Block_Timer_Data* data = si->_sifields._rt.si_sigval.sival_ptr;
    update_block(data->state, data->idx);
}

void init_blocks(Bar_State* state) {
    for(size_t i = 0; i < ARR_COUNT(blocks); i++) {
        if(blocks[i].init != NULL) state->blocks_data[i] = blocks[i].init();
        if(blocks[i].interval != 0.f) {
            time_t int_sec  = floor(blocks[i].interval);
            time_t int_nsec = (blocks[i].interval - floor(blocks[i].interval)) * 1000000000;
            struct itimerspec its = {{int_sec,int_nsec},{int_sec,int_nsec}};
            struct sigevent event = {0};
            struct sigaction sa = {0};
            event.sigev_notify = SIGEV_SIGNAL;
            event.sigev_signo = SIGRTMIN;
            Block_Timer_Data* timer_data = malloc(sizeof(Block_Timer_Data));
            timer_data->state = state;
            timer_data->idx = i;
            event.sigev_value.sival_ptr = timer_data;
            timer_create(CLOCK_MONOTONIC, &event, &state->timers_data[i].id);
            sa.sa_flags = SA_SIGINFO;
            sa.sa_sigaction = block_timer_expired;
            sigemptyset(&sa.sa_mask);
            sigaction(SIGRTMIN, &sa, NULL);
            timer_settime(state->timers_data[i].id, 0, &its, NULL);
        }
        if(blocks[i].display != NULL) {
            state->blocks_text[i] = blocks[i].display(state->blocks_data[i]);
            state->blocks_text_len[i] = strlen(state->blocks_text[i]);
            state->bar_text_len += state->blocks_text_len[i];
        }
    }

    if(state->bar_text == NULL){
        state->bar_text = malloc(state->bar_text_len + 1);
    }
    
    update_bar_text(state);
}

int main(int argc, char** argv) {
    (void) argc;
    (void) argv;
    int ret = 0;

    Bar_State* state = malloc(sizeof(Bar_State));

    char* display_name = getenv("DISPLAY");
    state->display = XOpenDisplay(display_name);
    if (state->display == NULL) error("%p: Failed to open display!", state->display);

    state->root = DefaultRootWindow(state->display);
    if (state->root == None) error("%lu: No root window found!", state->root);
    
    query_monitors(state->display, state->root, &state->monitor_count, &state->monitors);
    
    XVisualInfo vinfo_template = {0};
    vinfo_template.screen = 0;
    vinfo_template.depth = 32;
    vinfo_template.class = TrueColor;
    int vinfo_count = 0;
    XVisualInfo* vinfo_arr = XGetVisualInfo(state->display, VisualScreenMask | VisualDepthMask | VisualClassMask, &vinfo_template, &vinfo_count);
    if (!vinfo_arr) error("No 32bit TrueColor visuals found!");
    XRenderPictFormat* fmt;
    for (int i = 0; i < vinfo_count; i++) {
        fmt = XRenderFindVisualFormat(state->display, vinfo_arr[i].visual);
        if (fmt && fmt->type == PictTypeDirect && fmt->direct.alphaMask) {
            state->vinfo = vinfo_arr[i];
            break;
        }
    }
    if (state->vinfo.visual == NULL) error("Could not find any visual with alpha!");

    state->cmap = XCreateColormap(state->display, state->root, state->vinfo.visual, AllocNone);
    if (state->cmap == 0) error("Could not create the colormap!");

    state->font = XftFontOpenName(state->display, 0, FONT);
    if(state->font == NULL) error("%p: Could not load font!", state->font);

    state->fg_rc = parse_color(FG_COLOR);
    ret = XftColorAllocValue(state->display, state->vinfo.visual, state->cmap, &state->fg_rc, &state->fg_color);
    if(ret != True) error("%d: Could not allocate color!", ret);
    state->bg_rc = parse_color(BG_COLOR);
    ret = XftColorAllocValue(state->display, state->vinfo.visual, state->cmap, &state->bg_rc, &state->bg_color);
    if(ret != True) error("%d: Could not allocate color!", ret);
    
    Atom atom_destroy;
    Atom atom_type;
    Atom atom_type_dock;
    Atom atom_strut;
    atom_destroy   = XInternAtom(state->display, "WM_DELETE_WINDOW",         False);
    atom_type      = XInternAtom(state->display, "_NET_WM_WINDOW_TYPE",      False);
    atom_type_dock = XInternAtom(state->display, "_NET_WM_WINDOW_TYPE_DOCK", False);
    atom_strut     = XInternAtom(state->display, "_NET_WM_STRUT",            False);

    state->bars = malloc(state->monitor_count * sizeof(Bar));
    for (int i = 0; i < state->monitor_count; i++) {
        XSetWindowAttributes attr = {0};
        attr.override_redirect = True;
        attr.event_mask = ExposureMask;
        attr.colormap = state->cmap;
        attr.border_pixel = 0;
        attr.background_pixel = 0;

        state->bars[i].window = XCreateWindow(state->display, state->root, 0, 0, state->monitors[i].width, BAR_HEIGHT, 0, state->vinfo.depth, InputOutput, state->vinfo.visual, CWColormap | CWBorderPixel | CWBackPixel | CWEventMask | CWOverrideRedirect, &attr);
        if (state->bars[i].window == None) error("%lu: Failed to create window!", state->bars[i].window);

        const char* base_window_name = "limebar ";
        size_t window_name_size = strlen(base_window_name) + 3;
        char* window_name = malloc(window_name_size);
        snprintf(window_name, window_name_size, "limebar %d", i);
        XStoreName(state->display, state->bars[i].window, window_name);
        free(window_name);

        XChangeProperty(state->display, state->bars[i].window, atom_type, XA_ATOM, 32, PropModeReplace, (unsigned char *)&atom_type_dock, 1);
        unsigned long strut[4] = {0, 0, BAR_HEIGHT, 0};
        XChangeProperty(state->display, state->bars[i].window, atom_strut, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)strut, 4);
        
        XSetWMProtocols(state->display, state->bars[i].window, &atom_destroy, 1);

        XMapWindow(state->display, state->bars[i].window);
        
        XMoveWindow(state->display, state->bars[i].window, state->monitors[i].x, state->monitors[i].y);
    
        state->bars[i].draw = XftDrawCreate(state->display, state->bars[i].window, state->vinfo.visual, state->cmap);
        if(state->bars[i].draw == NULL) error("%d: Could not create xft draw!", state->bars[i].draw);
    }

    state->timers_data     = malloc(ARR_COUNT(blocks) * sizeof(Timer_Data*));
    state->blocks_data     = malloc(ARR_COUNT(blocks) * sizeof(void**));
    state->blocks_text     = malloc(ARR_COUNT(blocks) * sizeof(char**));
    state->blocks_text_len = malloc(ARR_COUNT(blocks) * sizeof(size_t*));
    init_blocks(state);

    int run = state->monitor_count;
    XEvent event;
    while (run > 0) {
        XNextEvent(state->display, &event);
        switch(event.type) {
        case Expose:
            for (int i = 0; i < state->monitor_count; i++) {
                if (event.xexpose.window == state->bars[i].window) {
                    draw_bar(state, i);
                }
            }
            break;
        case ClientMessage:
            for(int i = 0; i < state->monitor_count; i++){
                if(event.xclient.window == state->bars[i].window) {
                    if((unsigned long)event.xclient.data.l[0] == atom_destroy) {
                        XDestroyWindow(state->display, state->bars[i].window);
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
