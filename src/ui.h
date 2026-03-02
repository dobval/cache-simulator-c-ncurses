#ifndef UI_H
#define UI_H

#include "cache.h"
#include <ncurses.h>
#include <stdbool.h>

typedef enum {
    MODE_CONFIG,
    MODE_SIMULATE
} UIMode;

typedef struct {
    WINDOW *main_win;
    WINDOW *config_win;
    WINDOW *cache_view_win;
    WINDOW *instruction_win;
    WINDOW *stats_win;
    WINDOW *input_win;
    
    int selected_field;
    int current_menu;
    
    int l1_sets;
    int l1_assoc;
    int l1_block;
    int l2_sets;
    int l2_assoc;
    int l2_block;
    int l1_policy_idx;
    int l2_policy_idx;
    int write_policy_idx;
    bool enable_l2;
    
    char address_input[32];
    int input_pos;
    
    UIMode mode;
    bool needs_redraw;
} UIState;

void ui_init(UIState *ui);
void ui_free(UIState *ui);
void ui_draw(UIState *ui, CacheSystem *sys);
void ui_handle_input(UIState *ui, CacheSystem *sys, int ch);

void draw_box(WINDOW *win, int y1, int x1, int y2, int x2, bool single);
void draw_header(WINDOW *win, const char *title, int y, int width);

#endif
