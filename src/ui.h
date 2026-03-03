#ifndef UI_H
#define UI_H

#include "cache.h"
#include "trace.h"
#include <ncurses.h>
#include <stdbool.h>

typedef enum {
    MODE_CONFIG,
    MODE_RESULTS
} UIMode;

typedef struct {
    WINDOW *main_win;
    WINDOW *config_win;
    WINDOW *stats_win;
    
    int selected_field;
    
    int l1_sets;
    int l1_assoc;
    int l1_block;
    int l1_policy_idx;
    int write_policy_idx;
    
    int l2_sets;
    int l2_assoc;
    int l2_block;
    int l2_policy_idx;
    int enable_l2;
    
    TraceType trace_type;
    int trace_access_count;
    int trace_stride;
    
    TraceGenerator generator;
    
    UIMode mode;
    bool needs_redraw;
} UIState;

void ui_init(UIState *ui);
void ui_free(UIState *ui);
void ui_draw(UIState *ui, CacheSystem *sys);
void ui_handle_input(UIState *ui, CacheSystem *sys, int ch);

#endif
