#define BAR_HEIGHT 20

// \/ Colors defined as string of "#RRGGBBAA" in hex
#define BG_COLOR "#1E1E2EE0"
#define FG_COLOR "#CDD6F4FF"

#define FONT "Hack Nerd Font Mono:style=Bold:size=10"

typedef struct {
    void* (*block_init)();
    char* (*block_display)(void*);
    void (*block_click)(void*);
    void (*block_deinit)(void*);
    float interval;
} Block;

void* block_time_init();
char* block_time_display(void*);
void  block_time_click(void*);
void  block_time_deinit(void*);

static Block blocks[] = {
    {block_time_init, block_time_display, block_time_click, block_time_deinit,  1.0f}
};

// \/ Implementations

void* block_time_init() {
    // "12:43:21" -> 8 char + \0
    return malloc(9 * sizeof(char));
}
char* block_time_display(void* data) {
    char* data_text = (char*)data;
    double since_epoch  = (double)time(NULL);
    uint8_t hours   = ((uint8_t)floor(since_epoch / 3600.0)) % 25;
    uint8_t minutes = ((uint8_t)floor(since_epoch / 60.0)  ) % 61;
    uint8_t seconds = ((uint8_t)floor(since_epoch)         ) % 61;
    sprintf(data_text + 0, "%d", hours);
    *(data_text + 2) = ':';
    sprintf(data_text + 3, "%d", minutes);
    *(data_text + 5) = ':';
    sprintf(data_text + 6, "%d", seconds);
    *(data_text + 8) = '\0';
    return data_text;
}
void  block_time_click(void* data) {
}
void  block_time_deinit(void* data) {
    free(data);
}
