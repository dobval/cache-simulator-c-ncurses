#include "ui.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char *POLICY_OPTIONS[] = {"LRU", "FIFO", "LFU", "RANDOM"};
static const int NUM_POLICIES = 4;
static const char *WRITE_POLICY_OPTIONS[] = {"Write-Through", "Write-Back"};
static const int NUM_WRITE_POLICIES = 2;

#define CONFIG_WIN_H   27
#define CONFIG_WIN_Y   2
#define CONFIG_WIN_X   2

#define STATS_WIN_H    12
#define STATS_WIN_Y    (LINES - STATS_WIN_H - 1)
#define STATS_WIN_X    2
#define STATS_WIN_W    (COLS - 4)

void ui_init(UIState *ui) {
    initscr();
    
    if (!stdscr) {
        fprintf(stderr, "Failed to initialize ncurses\n");
        exit(1);
    }
    
    if (LINES < 24 || COLS < 80) {
        endwin();
        fprintf(stderr, "Terminal too small. Need at least 80x24\n");
        exit(1);
    }
    
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    leaveok(stdscr, TRUE);
    halfdelay(1);
    
    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(1, COLOR_CYAN, COLOR_BLACK);
        init_pair(2, COLOR_GREEN, COLOR_BLACK);
        init_pair(3, COLOR_RED, COLOR_BLACK);
        init_pair(4, COLOR_YELLOW, COLOR_BLACK);
        init_pair(5, COLOR_WHITE, COLOR_BLUE);
    }
    
    memset(ui, 0, sizeof(UIState));
    
    ui->l1_sets = 32;
    ui->l1_assoc = 4;
    ui->l1_block = 64;
    ui->l1_policy_idx = 0;
    ui->write_policy_idx = 0;
    
    ui->l2_sets = 128;
    ui->l2_assoc = 8;
    ui->l2_block = 64;
    ui->l2_policy_idx = 0;
    ui->enable_l2 = 0;
    
    ui->trace_type = TRACE_LOOP;
    ui->trace_access_count = 1000;
    ui->trace_stride = 4;
    
    ui->mode = MODE_CONFIG;
    
    ui->main_win = newwin(LINES, COLS, 0, 0);
    ui->config_win = newwin(CONFIG_WIN_H, COLS - 4, CONFIG_WIN_Y, CONFIG_WIN_X);
    ui->stats_win = newwin(STATS_WIN_H, STATS_WIN_W, STATS_WIN_Y, STATS_WIN_X);
}

void ui_free(UIState *ui) {
    if (ui->main_win) delwin(ui->main_win);
    if (ui->config_win) delwin(ui->config_win);
    if (ui->stats_win) delwin(ui->stats_win);
    endwin();
}

static void draw_config_panel(UIState *ui) {
    WINDOW *win = ui->config_win;
    int w = COLS-4;
    
    werase(win);
    box(win, 0, 0);
    mvwprintw(win, 0, (w - 14) / 2, " CACHE CONFIG ");
    
    mvwprintw(win, 2, 2, "L1 Sets:      [%3d]", ui->l1_sets);
    mvwprintw(win, 3, 2, "L1 Assoc:     [%3d]", ui->l1_assoc);
    mvwprintw(win, 4, 2, "L1 Block:     [%3d] B", ui->l1_block);
    mvwprintw(win, 5, 2, "L1 Policy:    [%s]", POLICY_OPTIONS[ui->l1_policy_idx]);
    mvwprintw(win, 6, 2, "Write Policy: [%s]", WRITE_POLICY_OPTIONS[ui->write_policy_idx]);
    
    mvwprintw(win, 8, 2, "L2 Cache:");
    mvwprintw(win, 9, 2, "Enable L2:    [%s]", ui->enable_l2 ? "Yes" : "No ");
    mvwprintw(win, 10, 2, "L2 Sets:      [%3d]", ui->l2_sets);
    mvwprintw(win, 11, 2, "L2 Assoc:     [%3d]", ui->l2_assoc);
    mvwprintw(win, 12, 2, "L2 Block:     [%3d] B", ui->l2_block);
    mvwprintw(win, 13, 2, "L2 Policy:    [%s]", POLICY_OPTIONS[ui->l2_policy_idx]);
    
    mvwprintw(win, 15, 2, "Trace Pattern:");
    for (int i = 0; i < TRACE_NUM; i++) {
        if ((TraceType)i == ui->trace_type) {
            wattron(win, A_REVERSE);
            mvwprintw(win, 16 + i, 4, " %c %s", '1' + i, trace_type_name(i));
            wattroff(win, A_REVERSE);
        } else {
            mvwprintw(win, 16 + i, 4, " %c %s", '1' + i, trace_type_name(i));
        }
    }
    
    mvwprintw(win, 20, 2, "Accesses:                [%5d]", ui->trace_access_count);
    mvwprintw(win, 21, 2, "Stride (for Sequential): [%5d]", ui->trace_stride);
    
    mvwprintw(win, 25, 2, "[ENTER] Run  |  + - / < > adjust values  |  1-3 select trace  |  Q quit");
    
    if (ui->mode == MODE_CONFIG && ui->selected_field >= 0) {
        int highlight_row = 0;
        switch (ui->selected_field) {
            case 0: highlight_row = 2; break;
            case 1: highlight_row = 3; break;
            case 2: highlight_row = 4; break;
            case 3: highlight_row = 5; break;
            case 4: highlight_row = 6; break;
            case 5: highlight_row = 9; break;
            case 6: highlight_row = 10; break;
            case 7: highlight_row = 11; break;
            case 8: highlight_row = 12; break;
            case 9: highlight_row = 13; break;
            case 10: highlight_row = 16 + ui->trace_type; break;
            case 11: highlight_row = 20; break;
            case 12: highlight_row = 21; break;
        }
        mvwchgat(win, highlight_row, 2, 25, A_REVERSE, 0, NULL);
    }
    
    wnoutrefresh(win);
}

static void draw_stats_panel(UIState *ui, CacheSystem *sys) {
    WINDOW *win = ui->stats_win;
    
    werase(win);
    box(win, 0, 0);
    mvwprintw(win, 0, (STATS_WIN_W - 12) / 2, " STATISTICS ");
    
    CacheStats *stats = &sys->l1_stats;
    uint64_t total = stats->hits + stats->misses;
    double hit_rate = total > 0 ? (double)stats->hits / total * 100.0 : 0.0;
    
    mvwprintw(win, 2, 2, "L1 Cache:");
    
    wattron(win, COLOR_PAIR(2));
    mvwprintw(win, 3, 4, "Hits:   %6llu", (unsigned long long)stats->hits);
    wattroff(win, COLOR_PAIR(2));
    
    wattron(win, COLOR_PAIR(3));
    mvwprintw(win, 3, 20, "Misses: %6llu", (unsigned long long)stats->misses);
    wattroff(win, COLOR_PAIR(3));
    
    int rate_color = (hit_rate >= 80.0) ? 2 : (hit_rate >= 50.0) ? 4 : 3;
    wattron(win, COLOR_PAIR(rate_color));
    mvwprintw(win, 3, 40, "Hit Rate: %6.2f%%", hit_rate);
    wattroff(win, COLOR_PAIR(rate_color));
    
    wattron(win, COLOR_PAIR(4));
    mvwprintw(win, 4, 4, "Evictions: %4llu", (unsigned long long)stats->evictions);
    wattroff(win, COLOR_PAIR(4));
    
    mvwprintw(win, 4, 24, "Mem Reads: %llu", (unsigned long long)stats->memory_reads);
    mvwprintw(win, 4, 46, "Writes: %llu", (unsigned long long)stats->memory_writes);
    
    if (sys->enable_l2) {
        CacheStats *l2_stats = &sys->l2_stats;
        uint64_t l2_total = l2_stats->hits + l2_stats->misses;
        double l2_hit_rate = l2_total > 0 ? (double)l2_stats->hits / l2_total * 100.0 : 0.0;
        
        mvwprintw(win, 6, 2, "L2 Cache:");
        
        wattron(win, COLOR_PAIR(2));
        mvwprintw(win, 7, 4, "Hits:   %6llu", (unsigned long long)l2_stats->hits);
        wattroff(win, COLOR_PAIR(2));
        
        wattron(win, COLOR_PAIR(3));
        mvwprintw(win, 7, 20, "Misses: %6llu", (unsigned long long)l2_stats->misses);
        wattroff(win, COLOR_PAIR(3));
        
        rate_color = (l2_hit_rate >= 80.0) ? 2 : (l2_hit_rate >= 50.0) ? 4 : 3;
        wattron(win, COLOR_PAIR(rate_color));
        mvwprintw(win, 7, 40, "Hit Rate: %6.2f%%", l2_hit_rate);
        wattroff(win, COLOR_PAIR(rate_color));
        
        mvwprintw(win, 8, 4, "Evictions: %4llu", (unsigned long long)l2_stats->evictions);
    }
    
    mvwprintw(win, 10, 2, "[ENTER] Back to config  |  Q quit");
    
    wnoutrefresh(win);
}

static void draw_main_border(UIState *ui) {
    WINDOW *win = ui->main_win;
    werase(win);
    box(win, 0, 0);
    
    wattron(win, A_BOLD | COLOR_PAIR(1));
    mvwprintw(win, 0, (COLS - 17) / 2, " CACHE SIMULATOR ");
    wattroff(win, A_BOLD | COLOR_PAIR(1));
    
    const char *mode_str = (ui->mode == MODE_CONFIG) ? "CONFIG" : "RESULTS";
    mvwprintw(win, 0, COLS - 12, "[%s]", mode_str);
    
    wnoutrefresh(win);
}

void ui_draw(UIState *ui, CacheSystem *sys) {
    draw_main_border(ui);
    
    draw_config_panel(ui);
    
    if (ui->mode == MODE_RESULTS) {
        draw_stats_panel(ui, sys);
    }
    
    doupdate();
}

static void run_simulation(UIState *ui, CacheSystem *sys) {
    WritePolicy wp = (ui->write_policy_idx == 0) ? WRITE_THROUGH : WRITE_BACK;
    EvictionPolicy l1_policy = (EvictionPolicy)ui->l1_policy_idx;
    EvictionPolicy l2_policy = (EvictionPolicy)ui->l2_policy_idx;
    
    cache_system_free(sys);
    cache_system_init(sys, ui->l1_sets, ui->l1_assoc, ui->l1_block,
                      ui->l2_sets, ui->l2_assoc, ui->l2_block,
                      l1_policy, l2_policy, wp, ui->enable_l2);
    
    trace_gen_init(&ui->generator, ui->trace_type, 0x1000, ui->trace_stride, ui->trace_access_count);
    
    TraceEntry entry;
    while (trace_gen_next(&ui->generator, &entry)) {
        AccessType type = (entry.type == 'S') ? ACCESS_STORE : ACCESS_LOAD;
        cache_system_access(sys, entry.address, type);
    }
    
    ui->mode = MODE_RESULTS;
}

void ui_handle_input(UIState *ui, CacheSystem *sys, int ch) {
    if (ch == 'q' || ch == 'Q') {
        return;
    }
    
    if (ui->mode == MODE_RESULTS) {
        if (ch == '\n' || ch == '\r') {
            ui->mode = MODE_CONFIG;
            cache_system_clear(sys);
        }
        return;
    }
    
    if (ch >= '1' && ch <= '3') {
        int trace_idx = ch - '1';
        if (trace_idx < TRACE_NUM) {
            ui->trace_type = (TraceType)trace_idx;
        }
        return;
    }
    
    switch (ch) {
        case KEY_UP:
            ui->selected_field--;
            if (ui->selected_field < 0) ui->selected_field = 12;
            break;
        case KEY_DOWN:
            ui->selected_field++;
            if (ui->selected_field > 12) ui->selected_field = 0;
            break;
        case '+':
        case KEY_RIGHT:
            switch (ui->selected_field) {
                case 0: {
                    ui->l1_sets = ui->l1_sets * 2;
                    if (ui->l1_sets > 256) ui->l1_sets = 256;
                    break;
                }
                case 1: {
                    ui->l1_assoc = ui->l1_assoc * 2;
                    if (ui->l1_assoc > 16) ui->l1_assoc = 16;
                    break;
                }
                case 2: {
                    ui->l1_block = ui->l1_block * 2;
                    if (ui->l1_block > 128) ui->l1_block = 128;
                    break;
                }
                case 3:
                    ui->l1_policy_idx = (ui->l1_policy_idx + 1) % NUM_POLICIES;
                    break;
                case 4:
                    ui->write_policy_idx = (ui->write_policy_idx + 1) % NUM_WRITE_POLICIES;
                    break;
                case 5:
                    ui->enable_l2 = !ui->enable_l2;
                    break;
                case 6: {
                    ui->l2_sets = ui->l2_sets * 2;
                    if (ui->l2_sets > 256) ui->l2_sets = 256;
                    break;
                }
                case 7: {
                    ui->l2_assoc = ui->l2_assoc * 2;
                    if (ui->l2_assoc > 16) ui->l2_assoc = 16;
                    break;
                }
                case 8: {
                    ui->l2_block = ui->l2_block * 2;
                    if (ui->l2_block > 128) ui->l2_block = 128;
                    break;
                }
                case 9:
                    ui->l2_policy_idx = (ui->l2_policy_idx + 1) % NUM_POLICIES;
                    break;
                case 11: {
                    ui->trace_access_count = ui->trace_access_count * 2;
                    if (ui->trace_access_count > 10000) ui->trace_access_count = 10000;
                    break;
                }
                case 12: {
                    ui->trace_stride = ui->trace_stride * 2;
                    if (ui->trace_stride > 1024) ui->trace_stride = 1024;
                    break;
                }
            }
            break;
        case '-':
        case KEY_LEFT:
            switch (ui->selected_field) {
                case 0: {
                    ui->l1_sets = ui->l1_sets / 2;
                    if (ui->l1_sets < 1) ui->l1_sets = 1;
                    break;
                }
                case 1: {
                    ui->l1_assoc = ui->l1_assoc / 2;
                    if (ui->l1_assoc < 1) ui->l1_assoc = 1;
                    break;
                }
                case 2: {
                    ui->l1_block = ui->l1_block / 2;
                    if (ui->l1_block < 4) ui->l1_block = 4;
                    break;
                }
                case 3:
                    ui->l1_policy_idx = (ui->l1_policy_idx - 1 + NUM_POLICIES) % NUM_POLICIES;
                    break;
                case 4:
                    ui->write_policy_idx = (ui->write_policy_idx - 1 + NUM_WRITE_POLICIES) % NUM_WRITE_POLICIES;
                    break;
                case 5:
                    ui->enable_l2 = !ui->enable_l2;
                    break;
                case 6: {
                    ui->l2_sets = ui->l2_sets / 2;
                    if (ui->l2_sets < 1) ui->l2_sets = 1;
                    break;
                }
                case 7: {
                    ui->l2_assoc = ui->l2_assoc / 2;
                    if (ui->l2_assoc < 1) ui->l2_assoc = 1;
                    break;
                }
                case 8: {
                    ui->l2_block = ui->l2_block / 2;
                    if (ui->l2_block < 4) ui->l2_block = 4;
                    break;
                }
                case 9:
                    ui->l2_policy_idx = (ui->l2_policy_idx - 1 + NUM_POLICIES) % NUM_POLICIES;
                    break;
                case 11: {
                    ui->trace_access_count = ui->trace_access_count / 2;
                    if (ui->trace_access_count < 100) ui->trace_access_count = 100;
                    break;
                }
                case 12: {
                    ui->trace_stride = ui->trace_stride / 2;
                    if (ui->trace_stride < 4) ui->trace_stride = 4;
                    break;
                }
            }
            break;
        case '\n':
        case '\r':
            run_simulation(ui, sys);
            break;
    }
}
