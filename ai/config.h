#ifndef CONFIG_H
#define CONFIG_H

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Appearance */
#define BG_COLOR  0x1e1e2e /* Background color (Hex) */
#define FG_COLOR  0xcdd6f4 /* Foreground/text color (Hex) */
#define FONT_NAME "fixed"  /* X11 core font name */
#define BAR_HEIGHT 22      /* Height of the statusbar in pixels */

/* 
 * Block Structure:
 * - update:  Function called every interval, must return a malloc'd string.
 * - click:   Function called when the block is clicked (can be NULL).
 * - interval: Time in seconds between update calls (float).
 */
typedef struct {
    char* (*update)(void);
    void  (*click)(void);
    float interval;
} Block;

/* --- Block Update Functions --- */
static char* get_time(void) {
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    
    char *buf = malloc(32);
    if (buf) strftime(buf, 32, " 🕒 %H:%M:%S ", timeinfo);
    return buf;
}

static char* get_date(void) {
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    
    char *buf = malloc(32);
    if (buf) strftime(buf, 32, " 📅 %a, %b %d ", timeinfo);
    return buf;
}

/* --- Block Click Functions --- */
static void click_time(void) {
    /* Add custom click behavior here (e.g., spawn a calendar app) */
}

static void click_date(void) {
    /* Add custom click behavior here */
}

/* Blocks Array */
static Block blocks[] = {
    { get_date, click_date, 60.0f },
    { get_time, click_time,  1.0f },
};

#endif /* CONFIG_H */
