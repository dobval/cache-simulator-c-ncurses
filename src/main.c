#include "cache.h"
#include "ui.h"
#include <ncurses.h>

int main(void) {
    CacheSystem cache_sys;
    
    cache_system_init(&cache_sys, 32, 4, 64, 0, 0, 0,
                      POLICY_LRU, POLICY_LRU, WRITE_BACK, false);
    
    UIState ui;
    ui_init(&ui);
    ui.needs_redraw = true;
    
    ui_draw(&ui, &cache_sys);
    
    int running = 1;
    while (running) {
        int ch = getch();
        
        if (ch != ERR) {
            ui_handle_input(&ui, &cache_sys, ch);
            
            if (ch == 'q' || ch == 'Q') {
                running = 0;
            }
            
            ui.needs_redraw = true;
        }
        
        ui_draw(&ui, &cache_sys);
    }
    
    ui_free(&ui);
    cache_system_free(&cache_sys);
    
    return 0;
}
