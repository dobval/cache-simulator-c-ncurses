#include "cache.h"
#include "ui.h"
#include "trace.h"
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    int l1_sets;
    int l1_assoc;
    int l1_block;
    EvictionPolicy l1_policy;
    WritePolicy write_policy;
    
    int l2_sets;
    int l2_assoc;
    int l2_block;
    EvictionPolicy l2_policy;
    int enable_l2;
    
    TraceType trace_type;
    int trace_count;
    int trace_stride;
    
    int cli_mode;
} CLIArgs;

static void print_usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  --l1-sets N        L1 sets (default: 32)\n");
    printf("  --l1-assoc N       L1 associativity (default: 4)\n");
    printf("  --l1-block N       L1 block size (default: 64)\n");
    printf("  --l1-policy P      L1 policy: LRU, FIFO, LFU, RANDOM\n");
    printf("  --write-policy P   Write policy: WT, WB\n");
    printf("  --enable-l2        Enable L2 cache\n");
    printf("  --l2-sets N        L2 sets (default: 128)\n");
    printf("  --l2-assoc N       L2 associativity (default: 8)\n");
    printf("  --l2-block N       L2 block size (default: 64)\n");
    printf("  --l2-policy P      L2 policy: LRU, FIFO, LFU, RANDOM\n");
    printf("  --trace T          Trace: loop, random, sequential\n");
    printf("  --count N          Accesses (default: 1000)\n");
    printf("  --stride N         Stride (default: 4)\n");
    printf("\nNo args = interactive UI mode.\n");
}

static EvictionPolicy parse_policy(const char *s) {
    if (strcmp(s, "LRU") == 0) return POLICY_LRU;
    if (strcmp(s, "FIFO") == 0) return POLICY_FIFO;
    if (strcmp(s, "LFU") == 0) return POLICY_LFU;
    return POLICY_RANDOM;
}

static WritePolicy parse_wp(const char *s) {
    return (strcmp(s, "WT") == 0) ? WRITE_THROUGH : WRITE_BACK;
}

static TraceType parse_trace(const char *s) {
    if (strcmp(s, "loop") == 0) return TRACE_LOOP;
    if (strcmp(s, "random") == 0) return TRACE_RANDOM;
    return TRACE_SEQUENTIAL;
}

typedef struct { int *var; int min, max; } IntOpt;
typedef struct { int *var; } FlagOpt;
typedef struct { EvictionPolicy *var; } PolicyOpt;
typedef struct { WritePolicy *var; } WpOpt;
typedef struct { TraceType *var; } TraceOpt;

typedef enum { INT, FLAG, POLICY, WP, TRACE } OptType;

typedef struct {
    const char *name;
    OptType type;
    union { IntOpt i; FlagOpt f; PolicyOpt p; WpOpt w; TraceOpt t; } u;
} Option;

static CLIArgs parse_args(int argc, char *argv[]) {
    CLIArgs a = {
        .l1_sets = 32, .l1_assoc = 4, .l1_block = 64,
        .l1_policy = POLICY_LRU, .write_policy = WRITE_BACK,
        .l2_sets = 128, .l2_assoc = 8, .l2_block = 64,
        .l2_policy = POLICY_LRU, .enable_l2 = 0,
        .trace_type = TRACE_LOOP, .trace_count = 1000, .trace_stride = 4
    };

    Option opts[] = {
        {"--l1-sets",    INT,  {.i = {&a.l1_sets, 1, 256}}},
        {"--l1-assoc",   INT,  {.i = {&a.l1_assoc, 1, 16}}},
        {"--l1-block",   INT,  {.i = {&a.l1_block, 4, 128}}},
        {"--l1-policy",  POLICY, {.p = {&a.l1_policy}}},
        {"--write-policy", WP, {.w = {&a.write_policy}}},
        {"--enable-l2",  FLAG, {.f = {&a.enable_l2}}},
        {"--l2-sets",    INT,  {.i = {&a.l2_sets, 1, 256}}},
        {"--l2-assoc",   INT,  {.i = {&a.l2_assoc, 1, 16}}},
        {"--l2-block",   INT,  {.i = {&a.l2_block, 4, 128}}},
        {"--l2-policy",  POLICY, {.p = {&a.l2_policy}}},
        {"--trace",      TRACE, {.t = {&a.trace_type}}},
        {"--count",      INT,  {.i = {&a.trace_count, 100, 10000}}},
        {"--stride",     INT,  {.i = {&a.trace_stride, 4, 1024}}},
    };

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            exit(0);
        }

        int matched = 0;
        for (size_t j = 0; j < sizeof(opts)/sizeof(opts[0]); j++) {
            if (strcmp(argv[i], opts[j].name) == 0) {
                matched = 1;
                switch (opts[j].type) {
                    case INT: {
                        if (i + 1 >= argc) { fprintf(stderr, "Missing value\n"); exit(1); }
                        long n = strtol(argv[++i], NULL, 10);
                        if (n < opts[j].u.i.min || n > opts[j].u.i.max) exit(1);
                        *opts[j].u.i.var = (int)n;
                        break;
                    }
                    case FLAG: *opts[j].u.f.var = 1; break;
                    case POLICY:
                        if (i + 1 >= argc) { fprintf(stderr, "Missing value\n"); exit(1); }
                        *opts[j].u.p.var = parse_policy(argv[++i]);
                        break;
                    case WP:
                        if (i + 1 >= argc) { fprintf(stderr, "Missing value\n"); exit(1); }
                        *opts[j].u.w.var = parse_wp(argv[++i]);
                        break;
                    case TRACE:
                        if (i + 1 >= argc) { fprintf(stderr, "Missing value\n"); exit(1); }
                        *opts[j].u.t.var = parse_trace(argv[++i]);
                        break;
                }
                break;
            }
        }
        if (!matched) { fprintf(stderr, "Unknown: %s\n", argv[i]); exit(1); }
    }
    return a;
}

static void run_cli_simulation(CLIArgs *args) {
    CacheSystem sys;
    
    cache_system_init(&sys, 
        args->l1_sets, args->l1_assoc, args->l1_block,
        args->l2_sets, args->l2_assoc, args->l2_block,
        args->l1_policy, args->l2_policy, args->write_policy, args->enable_l2);
    
    TraceGenerator gen;
    trace_gen_init(&gen, args->trace_type, 0x1000, args->trace_stride, args->trace_count);
    
    TraceEntry entry;
    while (trace_gen_next(&gen, &entry)) {
        AccessType type = (entry.type == 'S') ? ACCESS_STORE : ACCESS_LOAD;
        cache_system_access(&sys, entry.address, type);
    }
    
    CacheStats *l1 = &sys.l1_stats;
    uint64_t l1_total = l1->hits + l1->misses;
    double l1_rate = l1_total > 0 ? (double)l1->hits / l1_total * 100.0 : 0.0;
    
    printf("=== Cache Simulation Results ===\n\n");
    printf("Configuration:\n");
    printf("  L1: %d sets, %d-way, %d bytes\n", args->l1_sets, args->l1_assoc, args->l1_block);
    printf("  L1 Policy: %s, Write: %s\n", 
        policy_to_string(args->l1_policy),
        args->write_policy == WRITE_THROUGH ? "Write-Through" : "Write-Back");
    if (args->enable_l2) {
        printf("  L2: %d sets, %d-way, %d bytes\n", args->l2_sets, args->l2_assoc, args->l2_block);
        printf("  L2 Policy: %s\n", policy_to_string(args->l2_policy));
    }
    printf("  Trace: %s, Count: %d, Stride: %d\n\n",
        trace_type_name(args->trace_type), args->trace_count, args->trace_stride);
    
    printf("L1 Cache:\n");
    printf("  Hits:       %llu\n", (unsigned long long)l1->hits);
    printf("  Misses:     %llu\n", (unsigned long long)l1->misses);
    printf("  Hit Rate:   %.2f%%\n", l1_rate);
    printf("  Evictions:  %llu\n", (unsigned long long)l1->evictions);
    printf("  Mem Reads:  %llu\n", (unsigned long long)l1->memory_reads);
    printf("  Mem Writes: %llu\n", (unsigned long long)l1->memory_writes);
    
    if (args->enable_l2) {
        CacheStats *l2 = &sys.l2_stats;
        uint64_t l2_total = l2->hits + l2->misses;
        double l2_rate = l2_total > 0 ? (double)l2->hits / l2_total * 100.0 : 0.0;
        
        printf("\nL2 Cache:\n");
        printf("  Hits:       %llu\n", (unsigned long long)l2->hits);
        printf("  Misses:     %llu\n", (unsigned long long)l2->misses);
        printf("  Hit Rate:   %.2f%%\n", l2_rate);
        printf("  Evictions:  %llu\n", (unsigned long long)l2->evictions);
    }
    
    cache_system_free(&sys);
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        CLIArgs args = parse_args(argc, argv);
        run_cli_simulation(&args);
        return 0;
    }
    
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
