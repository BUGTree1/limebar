#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <signal.h>
#include <time.h>

#define BAR_HEIGHT 20

// \/ Colors defined as string of "#RRGGBBAA" in hex
#define BG_COLOR "#1E1E2EE0"
#define FG_COLOR "#CDD6F4FF"

#define FONT "Hack Nerd Font Mono:style=Bold:size=10"

typedef struct {
    void* (*init)();
    char* (*display)(void*); // The returned string has to be null terminated!
    void (*click)(void*);
    void (*deinit)(void*);
    float interval;
} Block;

void* block_time_init();
char* block_time_display(void*);
void  block_time_click(void*);
void  block_time_deinit(void*);

static Block blocks[] = {
    {block_time_init, block_time_display, block_time_click, block_time_deinit,  0.5f},
};

// \/ Implementations

typedef struct {
    char* text;
    time_t now;
} Block_Time;
void* block_time_init() {
    // "ddd yyyy-mm-dd hh:mm:ss" -> 23 char + \0
    Block_Time* data = malloc(sizeof(Block_Time));
    data->text = malloc(24 * sizeof(char));
    return data;
}
char* block_time_display(void* data) {
    Block_Time* time_data = data;
    time(&time_data->now);
    struct tm* lt = localtime(&time_data->now);
    strftime(time_data->text, 24, "%a %Y-%m-%d %H:%M:%S %Z", lt);
    return time_data->text;
}
void  block_time_click(void* data) {
}
void  block_time_deinit(void* data) {
    free(((Block_Time*)data)->text);
    free(data);
}
