#include "ui.h"
#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char *POLICY_OPTIONS[] = {"LRU", "FIFO", "LFU", "RANDOM"};
static const int NUM_POLICIES = 4;
static const char *WRITE_POLICY_OPTIONS[] = {"Write-Through", "Write-Back"};
static const int NUM_WRITE_POLICIES = 2;

void draw_box(WINDOW *win, int y1, int x1, int y2, int x2, bool single) {
    int ch = single ? ACS_ULCORNER : ACS_ULCORNER;
    mvwaddch(win, y1, x1, ch);
    ch = single ? ACS_HLINE : ACS_HLINE;
    mvwaddch(win, y1, x2 - 1, ch);
    ch = single ? ACS_URCORNER : ACS_URCORNER;
    mvwaddch(win, y1, x2 - 1, ch);
    
    for (int i = y1 + 1; i < y2 - 1; i++) {
        mvwaddch(win, i, x1, single ? ACS_VLINE : ACS_VLINE);
        mvwaddch(win, i, x2 - 1, single ? ACS_VLINE : ACS_VLINE);
    }
    
    ch = single ? ACS_LLCORNER : ACS_LLCORNER;
    mvwaddch(win, y2 - 1, x1, ch);
    mvwaddch(win, y2 - 1, x2 - 1, single ? ACS_LRCORNER : ACS_LRCORNER);
    mvwaddch(win, y2 - 1, x2 - 1, ch);
}

void draw_header(WINDOW *win, const char *title, int y, int width) {
    mvwaddch(win, y, 0, ACS_ULCORNER);
    mvwaddch(win, y, width - 1, ACS_URCORNER);
    mvwhline(win, y, 1, ACS_HLINE, width - 2);
    
    int title_len = strlen(title);
    int title_x = (width - title_len) / 2;
    mvwprintw(win, y, title_x, "%s", title);
}

void ui_init(UIState *ui) {
    initscr();
    
    if (!stdscr) {
        fprintf(stderr, "Failed to initialize ncurses\n");
        exit(1);
    }
    
    if (LINES < 24 || COLS < 80) {
        endwin();
        fprintf(stderr, "Terminal too small. Need at least 80x24, got %dx%d\n", COLS, LINES);
        exit(1);
    }
    
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    leaveok(stdscr, TRUE);
    halfdelay(1);
    
    fprintf(stderr, "Terminal: %dx%d, colors=%d\n", LINES, COLS, has_colors());
    
    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(1, COLOR_CYAN, COLOR_BLACK);
        init_pair(2, COLOR_GREEN, COLOR_BLACK);
        init_pair(3, COLOR_RED, COLOR_BLACK);
        init_pair(4, COLOR_YELLOW, COLOR_BLACK);
        init_pair(5, COLOR_WHITE, COLOR_BLACK);
    } else {
        init_pair(1, COLOR_WHITE, COLOR_BLACK);
        init_pair(2, COLOR_WHITE, COLOR_BLACK);
        init_pair(3, COLOR_WHITE, COLOR_BLACK);
        init_pair(4, COLOR_WHITE, COLOR_BLACK);
        init_pair(5, COLOR_WHITE, COLOR_BLACK);
    }
    
    init_pair(1, COLOR_CYAN, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);
    init_pair(3, COLOR_RED, COLOR_BLACK);
    init_pair(4, COLOR_YELLOW, COLOR_BLACK);
    init_pair(5, COLOR_WHITE, COLOR_BLUE);
    
    memset(ui, 0, sizeof(UIState));
    
    ui->l1_sets = 32;
    ui->l1_assoc = 4;
    ui->l1_block = 64;
    ui->l2_sets = 128;
    ui->l2_assoc = 8;
    ui->l2_block = 64;
    ui->l1_policy_idx = 0;
    ui->l2_policy_idx = 0;
    ui->write_policy_idx = 0;
    ui->enable_l2 = false;
    
    strcpy(ui->address_input, "0x00000000");
    ui->input_pos = strlen(ui->address_input);
    
    ui->mode = MODE_CONFIG;
    
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    ui->main_win = newwin(LINES, COLS, 0, 0);
    ui->config_win = newwin(20, 45, 2, 2);
    ui->cache_view_win = newwin(20, max_x - 52, 2, 48);
    if (ui->cache_view_win && max_x - 52 < 30) {
        wresize(ui->cache_view_win, 20, 30);
    }
    ui->instruction_win = newwin(5, max_x - 4, 23, 2);
    ui->stats_win = newwin(8, max_x - 4, max_y - 8, 2);
    ui->input_win = newwin(6, max_x - 4, max_y - 14, 2);
}

void ui_free(UIState *ui) {
    if (ui->main_win) delwin(ui->main_win);
    if (ui->config_win) delwin(ui->config_win);
    if (ui->cache_view_win) delwin(ui->cache_view_win);
    if (ui->instruction_win) delwin(ui->instruction_win);
    if (ui->stats_win) delwin(ui->stats_win);
    if (ui->input_win) delwin(ui->input_win);
    endwin();
}

static void draw_config_panel(UIState *ui, CacheSystem *sys) {
    int panel_width = 45;
    int panel_height = 20;
    
    WINDOW *win = ui->config_win;
    
    werase(win);
    
    box(win, 0, 0);
    mvwprintw(win, 0, (panel_width - 12) / 2, " CACHE CONFIG ");
    
    mvwprintw(win, 2, 2, "L1 Cache:");
    mvwprintw(win, 3, 4, "Sets (S):     [%3d]", ui->l1_sets);
    mvwprintw(win, 4, 4, "Associativity:[%3d]", ui->l1_assoc);
    mvwprintw(win, 5, 4, "Block Size:   [%3d] bytes", ui->l1_block);
    mvwprintw(win, 6, 4, "Policy:       [%s]", POLICY_OPTIONS[ui->l1_policy_idx]);
    
    if (ui->enable_l2) {
        mvwprintw(win, 8, 2, "L2 Cache:");
        mvwprintw(win, 9, 4, "Sets (S):     [%3d]", ui->l2_sets);
        mvwprintw(win, 10, 4, "Associativity:[%3d]", ui->l2_assoc);
        mvwprintw(win, 11, 4, "Block Size:   [%3d] bytes", ui->l2_block);
        mvwprintw(win, 12, 4, "Policy:       [%s]", POLICY_OPTIONS[ui->l2_policy_idx]);
    } else {
        mvwprintw(win, 8, 2, "L2 Cache: [Disabled]");
        mvwprintw(win, 9, 4, "(Press '2' to enable)");
    }
    
    mvwprintw(win, 14, 2, "Write Policy: [%s]", WRITE_POLICY_OPTIONS[ui->write_policy_idx]);
    
    mvwprintw(win, 16, 2, "Navigation: ^/v Select  +/- Change");
    mvwprintw(win, 17, 2, "             ENTER/F5 Apply");
    
    if (ui->selected_field >= 0 && ui->mode == MODE_CONFIG) {
        int highlight_y = 0;
        switch (ui->selected_field) {
            case 0: highlight_y = 3; break;
            case 1: highlight_y = 4; break;
            case 2: highlight_y = 5; break;
            case 3: highlight_y = 6; break;
            case 4: highlight_y = 9; break;
            case 5: highlight_y = 10; break;
            case 6: highlight_y = 11; break;
            case 7: highlight_y = 12; break;
            case 8: highlight_y = 14; break;
        }
        if (ui->enable_l2 || ui->selected_field < 4) {
            mvwchgat(win, highlight_y, 4, 20, A_REVERSE, 0, NULL);
        }
    }
    
    wnoutrefresh(win);
}

static void draw_cache_view(UIState *ui, CacheSystem *sys) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    int panel_width = max_x - 52;
    int panel_height = 20;
    
    if (panel_width < 30) panel_width = 30;
    
    WINDOW *win = ui->cache_view_win;
    
    werase(win);
    
    box(win, 0, 0);
    mvwprintw(win, 0, (panel_width - 15) / 2, " CACHE STATE ");
    
    Cache *cache = &sys->l1_cache;
    int display_sets = cache->num_sets > 10 ? 10 : cache->num_sets;
    
    mvwprintw(win, 2, 2, "Set | V Tag       | LRU/FIFO");
    mvwaddch(win, 3, 2, ACS_LTEE);
    mvwhline(win, 3, 3, ACS_HLINE, panel_width - 5);
    mvwaddch(win, 3, panel_width - 2, ACS_RTEE);
    
    for (int i = 0; i < display_sets && i < cache->num_sets; i++) {
        mvwprintw(win, 4 + i, 2, "%3d |", i);
        for (int j = 0; j < cache->associativity && j < 4; j++) {
            if (cache->lines[i][j].valid) {
                if (cache->lines[i][j].dirty && cache->write_policy == WRITE_BACK) {
                    wattron(win, COLOR_PAIR(3));
                    mvwprintw(win, 4 + i, 6 + j * 13, "V %08x*", cache->lines[i][j].tag);
                    wattroff(win, COLOR_PAIR(3));
                } else {
                    mvwprintw(win, 4 + i, 6 + j * 13, "V %08x", cache->lines[i][j].tag);
                }
            } else {
                mvwprintw(win, 4 + i, 6 + j * 13, "I ..........");
            }
        }
    }
    
    int total_lines = display_sets * cache->associativity;
    mvwprintw(win, panel_height - 3, 2, "Showing %d of %d sets, %d ways",
              display_sets, cache->num_sets, cache->associativity);
    mvwprintw(win, panel_height - 2, 2, "* = dirty (write-back)");
    
    wnoutrefresh(win);
}

static void draw_input_panel(UIState *ui, CacheSystem *sys) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    int panel_width = max_x - 4;
    int panel_height = 6;
    
    WINDOW *win = ui->input_win;
    
    werase(win);
    
    box(win, 0, 0);
    mvwprintw(win, 0, (panel_width - 20) / 2, " MEMORY ACCESS ");
    
    mvwprintw(win, 2, 2, "Address: ");
    wattron(win, A_BOLD);
    mvwprintw(win, 2, 12, "%-20s", ui->address_input);
    wattroff(win, A_BOLD);
    
    if (ui->mode == MODE_SIMULATE) {
        wattron(win, COLOR_PAIR(5));
        mvwprintw(win, 2, 35, "[ENTER] Execute");
        wattroff(win, COLOR_PAIR(5));
    } else {
        mvwprintw(win, 2, 35, "(Press Enter to simulate)");
    }
    
    mvwprintw(win, 3, 2, "Keys: L=Load  S=Store  C=Clear  F6:Clear  F10=Quit");
    
    wnoutrefresh(win);
}

static void draw_instruction_panel(UIState *ui) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    int panel_width = max_x - 4;
    
    WINDOW *win = ui->instruction_win;
    
    werase(win);
    
    box(win, 0, 0);
    wattron(win, A_BOLD);
    mvwprintw(win, 0, (panel_width - 14) / 2, " INSTRUCTIONS ");
    wattroff(win, A_BOLD);
    
    if (ui->mode == MODE_CONFIG) {
        mvwprintw(win, 1, 2, "1. Select a field");
        mvwprintw(win, 2, 2, "2. Adjust values");
        mvwprintw(win, 3, 2, "3. Apply config and start simulation");
    } else {
        mvwprintw(win, 1, 2, "1. Enter hex address (e.g., 0x1000)");
        mvwprintw(win, 2, 2, "2. Load or store, then execute");
        mvwprintw(win, 3, 2, "3. Clear stats, Q to quit");
    }
    
    wnoutrefresh(win);
}

static void draw_stats_panel(UIState *ui, CacheSystem *sys) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    int panel_width = max_x - 4;
    int panel_height = 8;
    
    WINDOW *win = ui->stats_win;
    
    werase(win);
    
    box(win, 0, 0);
    mvwprintw(win, 0, (panel_width - 15) / 2, " STATISTICS ");
    
    CacheStats *stats = &sys->l1_stats;
    uint64_t total = stats->hits + stats->misses;
    double hit_rate = total > 0 ? (double)stats->hits / total * 100.0 : 0.0;
    
    mvwprintw(win, 2, 2, "L1 Cache:");
    wattron(win, COLOR_PAIR(2));
    mvwprintw(win, 2, 20, "Hits:   %6llu", (unsigned long long)stats->hits);
    wattroff(win, COLOR_PAIR(2));
    wattron(win, COLOR_PAIR(3));
    mvwprintw(win, 2, 38, "Misses: %6llu", (unsigned long long)stats->misses);
    wattroff(win, COLOR_PAIR(3));
    mvwprintw(win, 2, 56, "Evictions: %4llu", (unsigned long long)stats->evictions);
    mvwprintw(win, 2, 76, "Hit Rate: %6.2f%%", hit_rate);
    
    mvwprintw(win, 3, 2, "Memory:");
    mvwprintw(win, 3, 20, "Reads:  %6llu", (unsigned long long)stats->memory_reads);
    mvwprintw(win, 3, 38, "Writes: %6llu", (unsigned long long)stats->memory_writes);
    
    if (sys->enable_l2) {
        CacheStats *l2_stats = &sys->l2_stats;
        uint64_t l2_total = l2_stats->hits + l2_stats->misses;
        double l2_hit_rate = l2_total > 0 ? (double)l2_stats->hits / l2_total * 100.0 : 0.0;
        
        mvwprintw(win, 5, 2, "L2 Cache:");
        wattron(win, COLOR_PAIR(2));
        mvwprintw(win, 5, 20, "Hits:   %6llu", (unsigned long long)l2_stats->hits);
        wattroff(win, COLOR_PAIR(2));
        wattron(win, COLOR_PAIR(3));
        mvwprintw(win, 5, 38, "Misses: %6llu", (unsigned long long)l2_stats->misses);
        wattroff(win, COLOR_PAIR(3));
        mvwprintw(win, 5, 56, "Evictions: %4llu", (unsigned long long)l2_stats->evictions);
        mvwprintw(win, 5, 76, "Hit Rate: %6.2f%%", l2_hit_rate);
    }
    
    wnoutrefresh(win);
}

static void draw_main_border(UIState *ui) {
    WINDOW *win = ui->main_win;
    
    werase(win);
    
    box(win, 0, 0);
    
    wattron(win, A_BOLD | COLOR_PAIR(1));
    mvwprintw(win, 0, (COLS - 25) / 2, " CACHE SIMULATOR ");
    wattroff(win, A_BOLD | COLOR_PAIR(1));
    
    const char *mode_str = ui->mode == MODE_CONFIG ? "[CONFIG]" : "[SIMULATE]";
    mvwprintw(win, 0, COLS - 15, "%s", mode_str);
    
    wnoutrefresh(win);
}

void ui_draw(UIState *ui, CacheSystem *sys) {
    draw_main_border(ui);
    draw_config_panel(ui, sys);
    draw_cache_view(ui, sys);
    draw_instruction_panel(ui);
    draw_stats_panel(ui, sys);
    draw_input_panel(ui, sys);
    
    doupdate();
}

static void apply_config(UIState *ui, CacheSystem *sys) {
    WritePolicy wp = (ui->write_policy_idx == 0) ? WRITE_THROUGH : WRITE_BACK;
    EvictionPolicy l1_policy = (EvictionPolicy)ui->l1_policy_idx;
    EvictionPolicy l2_policy = (EvictionPolicy)ui->l2_policy_idx;
    
    cache_system_free(sys);
    cache_system_init(sys, ui->l1_sets, ui->l1_assoc, ui->l1_block,
                      ui->l2_sets, ui->l2_assoc, ui->l2_block,
                      l1_policy, l2_policy, wp, ui->enable_l2);
    
    ui->mode = MODE_SIMULATE;
}

static void handle_config_input(UIState *ui, CacheSystem *sys, int ch) {
    switch (ch) {
        case KEY_UP:
            ui->selected_field--;
            if (ui->selected_field < 0) ui->selected_field = ui->enable_l2 ? 8 : 4;
            break;
        case KEY_DOWN:
            ui->selected_field++;
            if (ui->selected_field > (ui->enable_l2 ? 8 : 4)) ui->selected_field = 0;
            break;
        case '+':
        case KEY_RIGHT:
            switch (ui->selected_field) {
                case 0: ui->l1_sets = ui->l1_sets * 2; if (ui->l1_sets > 256) ui->l1_sets = 256; break;
                case 1: ui->l1_assoc = ui->l1_assoc * 2; if (ui->l1_assoc > 16) ui->l1_assoc = 16; break;
                case 2: ui->l1_block = ui->l1_block * 2; if (ui->l1_block > 128) ui->l1_block = 128; break;
                case 3: ui->l1_policy_idx = (ui->l1_policy_idx + 1) % NUM_POLICIES; break;
                case 4: ui->l2_sets = ui->l2_sets * 2; if (ui->l2_sets > 256) ui->l2_sets = 256; break;
                case 5: ui->l2_assoc = ui->l2_assoc * 2; if (ui->l2_assoc > 16) ui->l2_assoc = 16; break;
                case 6: ui->l2_block = ui->l2_block * 2; if (ui->l2_block > 128) ui->l2_block = 128; break;
                case 7: ui->l2_policy_idx = (ui->l2_policy_idx + 1) % NUM_POLICIES; break;
                case 8: ui->write_policy_idx = (ui->write_policy_idx + 1) % NUM_WRITE_POLICIES; break;
            }
            break;
        case '-':
        case KEY_LEFT:
            switch (ui->selected_field) {
                case 0: ui->l1_sets = ui->l1_sets / 2; if (ui->l1_sets < 1) ui->l1_sets = 1; break;
                case 1: ui->l1_assoc = ui->l1_assoc / 2; if (ui->l1_assoc < 1) ui->l1_assoc = 1; break;
                case 2: ui->l1_block = ui->l1_block / 2; if (ui->l1_block < 4) ui->l1_block = 4; break;
                case 3: ui->l1_policy_idx = (ui->l1_policy_idx - 1 + NUM_POLICIES) % NUM_POLICIES; break;
                case 4: ui->l2_sets = ui->l2_sets / 2; if (ui->l2_sets < 1) ui->l2_sets = 1; break;
                case 5: ui->l2_assoc = ui->l2_assoc / 2; if (ui->l2_assoc < 1) ui->l2_assoc = 1; break;
                case 6: ui->l2_block = ui->l2_block / 2; if (ui->l2_block < 4) ui->l2_block = 4; break;
                case 7: ui->l2_policy_idx = (ui->l2_policy_idx - 1 + NUM_POLICIES) % NUM_POLICIES; break;
                case 8: ui->write_policy_idx = (ui->write_policy_idx - 1 + NUM_WRITE_POLICIES) % NUM_WRITE_POLICIES; break;
            }
            break;
        case '2':
            ui->enable_l2 = !ui->enable_l2;
            if (ui->selected_field < 4 && !ui->enable_l2) ui->selected_field = 0;
            break;
        case KEY_F(5):
            apply_config(ui, sys);
            break;
    }
}

static void handle_address_input(UIState *ui, int ch) {
    const char *hex_chars = "0123456789abcdefABCDEF";
    
    if (ch >= '0' && ch <= '9') {
        if (ui->input_pos < 18) {
            ui->address_input[ui->input_pos++] = ch;
            ui->address_input[ui->input_pos] = '\0';
        }
    } else if ((ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F')) {
        if (ui->input_pos < 18) {
            ui->address_input[ui->input_pos++] = ch;
            ui->address_input[ui->input_pos] = '\0';
        }
    } else if (ch == KEY_BACKSPACE || ch == 127) {
        if (ui->input_pos > 0) {
            ui->address_input[--ui->input_pos] = '\0';
        }
    } else if (ch == '0' && ui->input_pos == 0) {
        ui->address_input[ui->input_pos++] = '0';
        ui->address_input[ui->input_pos] = '\0';
    }
}

static uint32_t parse_address(const char *str) {
    uint32_t addr = 0;
    const char *ptr = str;
    
    if (strncmp(ptr, "0x", 2) == 0 || strncmp(ptr, "0X", 2) == 0) {
        ptr += 2;
    }
    
    while (*ptr) {
        char c = *ptr;
        uint32_t nibble;
        if (c >= '0' && c <= '9') nibble = c - '0';
        else if (c >= 'a' && c <= 'f') nibble = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') nibble = c - 'A' + 10;
        else break;
        addr = (addr << 4) | nibble;
        ptr++;
    }
    
    return addr;
}

void ui_handle_input(UIState *ui, CacheSystem *sys, int ch) {
    switch (ch) {
        case 'q':
        case 'Q':
        case KEY_F(10):
            break;
            
        case 'c':
        case 'C':
            cache_system_clear(sys);
            strcpy(ui->address_input, "0x00000000");
            ui->input_pos = strlen(ui->address_input);
            break;
            
        case KEY_F(6):
            cache_system_clear(sys);
            break;
            
        case 'l':
        case 'L':
            if (ui->mode == MODE_SIMULATE) {
                uint32_t addr = parse_address(ui->address_input);
                cache_system_access(sys, addr, ACCESS_LOAD);
            }
            break;
            
        case 's':
        case 'S':
            if (ui->mode == MODE_SIMULATE) {
                uint32_t addr = parse_address(ui->address_input);
                cache_system_access(sys, addr, ACCESS_STORE);
            }
            break;
            
        case '\n':
        case '\r':
            if (ui->mode == MODE_CONFIG) {
                apply_config(ui, sys);
            } else if (ui->mode == MODE_SIMULATE) {
                uint32_t addr = parse_address(ui->address_input);
                cache_system_access(sys, addr, ACCESS_LOAD);
            }
            break;
            
        default:
            if (ui->mode == MODE_CONFIG) {
                handle_config_input(ui, sys, ch);
            } else if (ui->mode == MODE_SIMULATE) {
                handle_address_input(ui, ch);
            }
            break;
    }
}
