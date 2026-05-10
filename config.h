
#define BAR_HEIGHT 20

#define BG_COLOR "#1E1E2EE0"
#define FG_COLOR "#CDD6F4FF"

#define FONT "Hack Nerd Font Mono:style=Bold:size=10"
//#define FONT "*fira*nerd*mono*-bold*ascii*"

typedef struct {
    void* (*block_init)();
    char* (*block_display)(void*);
    void (*block_click)(void*);
    void (*block_deinit)(void*);
    float interval;
} Block;

static Block blocks[] = {
    { NULL, NULL, NULL, NULL,  0.f }
};
