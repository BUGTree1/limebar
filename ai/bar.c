#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <poll.h>
#include <xcb/xcb.h>
#include <xcb/randr.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_aux.h>
#include "config.h"

#define LEN(arr) (sizeof(arr) / sizeof(arr[0]))

typedef struct {
    xcb_window_t win;
    int16_t x;
    int16_t y;
    uint16_t width;
} MonitorWin;

static xcb_connection_t *conn;
static xcb_screen_t *screen;
static xcb_font_t font;
static xcb_gcontext_t gc;
static uint32_t fg_pixel, bg_pixel;
static MonitorWin *mons = NULL;
static int num_mons = 0;
static int block_x_offsets[LEN(blocks)];

/* Helper to allocate X11 color pixels from hex */
static uint32_t get_color_pixel(uint32_t hex) {
    xcb_alloc_color_reply_t *rep = xcb_alloc_color_reply(conn,
        xcb_alloc_color(conn, screen->default_colormap,
                        (hex >> 16) * 257, ((hex >> 8) & 0xFF) * 257, (hex & 0xFF) * 257),
        NULL);
    uint32_t pixel = rep ? rep->pixel : 0;
    free(rep);
    return pixel;
}

/* Helper to intern X11 atoms */
static xcb_atom_t get_atom(const char *name) {
    xcb_intern_atom_reply_t *rep = xcb_intern_atom_reply(conn,
        xcb_intern_atom(conn, 0, strlen(name), name), NULL);
    xcb_atom_t atom = rep ? rep->atom : XCB_NONE;
    free(rep);
    return atom;
}

static void init_x(void) {
    int scrno;
    conn = xcb_connect(NULL, &scrno);
    if (xcb_connection_has_error(conn)) {
        fprintf(stderr, "Cannot connect to X server\n");
        exit(1);
    }
    screen = xcb_aux_get_screen(conn, scrno);
    fg_pixel = get_color_pixel(FG_COLOR);
    bg_pixel = get_color_pixel(BG_COLOR);

    font = xcb_generate_id(conn);
    xcb_open_font(conn, font, strlen(FONT_NAME), FONT_NAME);

    gc = xcb_generate_id(conn);
    uint32_t mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT;
    uint32_t vals[] = { fg_pixel, bg_pixel, font };
    xcb_create_gc(conn, gc, screen->root, mask, vals);
}

static void init_monitors(void) {
    const xcb_query_extension_reply_t *randr_ext = xcb_get_extension_data(conn, &xcb_randr_id);
    if (!randr_ext || !randr_ext->present) {
        /* Fallback to single screen if RandR is missing */
        mons = malloc(sizeof(MonitorWin));
        mons[0].win = xcb_generate_id(conn);
        mons[0].x = 0;
        mons[0].y = 0;
        mons[0].width = screen->width_in_pixels;
        num_mons = 1;
    } else {
        xcb_randr_get_monitors_cookie_t c = xcb_randr_get_monitors(conn, screen->root, 1);
        xcb_randr_get_monitors_reply_t *rep = xcb_randr_get_monitors_reply(conn, c, NULL);
        if (!rep) return;

        num_mons = xcb_randr_get_monitors_monitors_length(rep);
        mons = malloc(num_mons * sizeof(MonitorWin));

        int i = 0;
        xcb_randr_monitor_info_iterator_t it = xcb_randr_get_monitors_monitors_iterator(rep);
        for (; it.rem; xcb_randr_monitor_info_next(&it), i++) {
            xcb_randr_monitor_info_t *info = it.data;
            mons[i].win = xcb_generate_id(conn);
            mons[i].x = info->x;
            mons[i].y = info->y;
            mons[i].width = info->width;
        }
        free(rep);
    }

    xcb_atom_t wm_type = get_atom("_NET_WM_WINDOW_TYPE");
    xcb_atom_t wm_dock = get_atom("_NET_WM_WINDOW_TYPE_DOCK");

    for (int i = 0; i < num_mons; i++) {
        uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
        uint32_t vals[] = {
            bg_pixel,
            XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS
        };

        xcb_create_window(conn, screen->root_depth, mons[i].win, screen->root,
                          mons[i].x, mons[i].y, mons[i].width, BAR_HEIGHT, 0,
                          XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, mask, vals);

        /* Set as Dock (always on top, unresizable by WM) */
        xcb_change_property(conn, XCB_PROP_MODE_REPLACE, mons[i].win, wm_type, XCB_ATOM_ATOM, 32, 1, &wm_dock);

        /* Prevent resizing */
        xcb_size_hints_t hints;
        memset(&hints, 0, sizeof(hints));
        hints.flags = XCB_ICCCM_SIZE_HINT_P_MIN_SIZE | XCB_ICCCM_SIZE_HINT_P_MAX_SIZE;
        hints.min_width = mons[i].width;
        hints.max_width = mons[i].width;
        hints.min_height = BAR_HEIGHT;
        hints.max_height = BAR_HEIGHT;
        xcb_icccm_set_wm_normal_hints(conn, mons[i].win, &hints);

        xcb_map_window(conn, mons[i].win);
    }
}

static void draw_bar(void) {
    for (int m = 0; m < num_mons; m++) {
        xcb_rectangle_t rect = {0, 0, mons[m].width, BAR_HEIGHT};
        xcb_change_gc(conn, gc, XCB_GC_FOREGROUND, &bg_pixel);
        xcb_poly_fill_rectangle(conn, mons[m].win, gc, 1, &rect);

        xcb_change_gc(conn, gc, XCB_GC_FOREGROUND, &fg_pixel);
        
        int current_x = 6;
        for (int i = 0; i < (int)LEN(blocks); i++) {
            char *text = blocks[i].update ? blocks[i].update() : NULL;
            if (!text) continue;

            int len = strlen(text);
            block_x_offsets[i] = current_x;

            /* Draw Text */
            xcb_image_text_8(conn, len, mons[m].win, gc, current_x, BAR_HEIGHT - 5, text);

            /* Calculate Text Width for next block offset */
            xcb_char2b_t *text_16 = malloc(len * sizeof(xcb_char2b_t));
            for (int j = 0; j < len; j++) {
                text_16[j].byte1 = 0;
                text_16[j].byte2 = text[j];
            }

            xcb_query_text_extents_reply_t *ext = xcb_query_text_extents_reply(conn,
                xcb_query_text_extents(conn, font, len, text_16), NULL);
            free(text_16);

            if (ext) {
                current_x += ext->overall_width;
                free(ext);
            } else {
                current_x += len * 6; /* Fallback estimation */
            }

            free(text);
        }
    }
    xcb_flush(conn);
}

static void handle_button_press(xcb_button_press_event_t *ev) {
    for (int i = LEN(blocks) - 1; i >= 0; i--) {
        if (ev->event_x >= block_x_offsets[i] && blocks[i].click) {
            blocks[i].click();
            return;
        }
    }
}

int main(void) {
    init_x();
    init_monitors();

    char *outputs[LEN(blocks)];
    struct timespec last_update[LEN(blocks)];
    memset(last_update, 0, sizeof(last_update));

    int xfd = xcb_get_file_descriptor(conn);

    while (1) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        int needs_redraw = 0;
        float min_sleep = 10.0f; /* Max poll wait time */

        /* Check intervals and update blocks */
        for (int i = 0; i < (int)LEN(blocks); i++) {
            float elapsed = (now.tv_sec - last_update[i].tv_sec) + 
                            (now.tv_nsec - last_update[i].tv_nsec) / 1e9f;
            
            if (elapsed >= blocks[i].interval) {
                if (outputs[i]) free(outputs[i]);
                outputs[i] = blocks[i].update();
                last_update[i] = now;
                needs_redraw = 1;
            } else {
                float remaining = blocks[i].interval - elapsed;
                if (remaining < min_sleep) min_sleep = remaining;
            }
        }

        if (needs_redraw) draw_bar();

        /* Poll X events efficiently without consuming CPU */
        struct pollfd pfd = { xfd, POLLIN, 0 };
        int poll_ret = poll(&pfd, 1, (int)(min_sleep * 1000.0f));

        if (poll_ret > 0) {
            xcb_generic_event_t *ev;
            while ((ev = xcb_poll_for_event(conn))) {
                switch (ev->response_type & ~0x80) {
                    case XCB_EXPOSE:
                        draw_bar();
                        break;
                    case XCB_BUTTON_PRESS:
                        handle_button_press((xcb_button_press_event_t *)ev);
                        break;
                }
                free(ev);
            }
        }
    }

    /* Cleanup (unreachable in this simple main loop, but good practice) */
    for (int i = 0; i < num_mons; i++) xcb_destroy_window(conn, mons[i].win);
    xcb_close_font(conn, font);
    xcb_free_gc(conn, gc);
    xcb_disconnect(conn);
    free(mons);
    return 0;
}
