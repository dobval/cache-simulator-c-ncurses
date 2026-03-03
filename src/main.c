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
    printf("       %s --sets N --assoc N --block N --trace T --count N\n\n", prog);
    printf("Options:\n");
    printf("  --help              Show this help message\n");
    printf("  --l1-sets N         L1 cache sets (default: 32)\n");
    printf("  --l1-assoc N        L1 cache associativity (default: 4)\n");
    printf("  --l1-block N        L1 cache block size (default: 64)\n");
    printf("  --l1-policy P       L1 eviction policy: LRU, FIFO, LFU, RANDOM (default: LRU)\n");
    printf("  --write-policy P    Write policy: WT, WB (default: WB)\n");
    printf("  --enable-l2         Enable L2 cache\n");
    printf("  --l2-sets N         L2 cache sets (default: 128)\n");
    printf("  --l2-assoc N        L2 cache associativity (default: 8)\n");
    printf("  --l2-block N        L2 cache block size (default: 64)\n");
    printf("  --l2-policy P       L2 eviction policy: LRU, FIFO, LFU, RANDOM (default: LRU)\n");
    printf("  --trace T           Trace type: loop, random, sequential (default: loop)\n");
    printf("  --count N           Number of memory accesses (default: 1000)\n");
    printf("  --stride N          Stride for sequential trace (default: 4)\n");
    printf("\nWithout options, runs in interactive UI mode.\n");
}

static EvictionPolicy parse_policy(const char *str) {
    if (strcmp(str, "LRU") == 0) return POLICY_LRU;
    if (strcmp(str, "FIFO") == 0) return POLICY_FIFO;
    if (strcmp(str, "LFU") == 0) return POLICY_LFU;
    if (strcmp(str, "RANDOM") == 0) return POLICY_RANDOM;
    return POLICY_LRU;
}

static WritePolicy parse_write_policy(const char *str) {
    if (strcmp(str, "WT") == 0) return WRITE_THROUGH;
    if (strcmp(str, "WB") == 0) return WRITE_BACK;
    return WRITE_BACK;
}

static TraceType parse_trace(const char *str) {
    if (strcmp(str, "loop") == 0) return TRACE_LOOP;
    if (strcmp(str, "random") == 0) return TRACE_RANDOM;
    if (strcmp(str, "sequential") == 0) return TRACE_SEQUENTIAL;
    return TRACE_LOOP;
}

static int parse_int(const char *arg, const char *val, int *out) {
    char *endptr;
    long num = strtol(val, &endptr, 10);
    if (*endptr != '\0' || num < 1 || num > 65536) {
        fprintf(stderr, "Invalid value for %s: %s\n", arg, val);
        return 0;
    }
    *out = (int)num;
    return 1;
}

static CLIArgs parse_args(int argc, char *argv[]) {
    CLIArgs args = {
        .l1_sets = 32,
        .l1_assoc = 4,
        .l1_block = 64,
        .l1_policy = POLICY_LRU,
        .write_policy = WRITE_BACK,
        .l2_sets = 128,
        .l2_assoc = 8,
        .l2_block = 64,
        .l2_policy = POLICY_LRU,
        .enable_l2 = 0,
        .trace_type = TRACE_LOOP,
        .trace_count = 1000,
        .trace_stride = 4,
        .cli_mode = 0
    };
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            exit(0);
        }
        
        char *next_arg = (i + 1 < argc) ? argv[i + 1] : NULL;
        
        if (strcmp(argv[i], "--l1-sets") == 0) {
            if (!next_arg || !parse_int(argv[i], next_arg, &args.l1_sets)) exit(1);
            i++;
        }
        else if (strcmp(argv[i], "--l1-assoc") == 0) {
            if (!next_arg || !parse_int(argv[i], next_arg, &args.l1_assoc)) exit(1);
            i++;
        }
        else if (strcmp(argv[i], "--l1-block") == 0) {
            if (!next_arg || !parse_int(argv[i], next_arg, &args.l1_block)) exit(1);
            i++;
        }
        else if (strcmp(argv[i], "--l1-policy") == 0) {
            if (!next_arg) { fprintf(stderr, "Missing value for %s\n", argv[i]); exit(1); }
            args.l1_policy = parse_policy(next_arg);
            i++;
        }
        else if (strcmp(argv[i], "--write-policy") == 0) {
            if (!next_arg) { fprintf(stderr, "Missing value for %s\n", argv[i]); exit(1); }
            args.write_policy = parse_write_policy(next_arg);
            i++;
        }
        else if (strcmp(argv[i], "--enable-l2") == 0) {
            args.enable_l2 = 1;
        }
        else if (strcmp(argv[i], "--l2-sets") == 0) {
            if (!next_arg || !parse_int(argv[i], next_arg, &args.l2_sets)) exit(1);
            i++;
        }
        else if (strcmp(argv[i], "--l2-assoc") == 0) {
            if (!next_arg || !parse_int(argv[i], next_arg, &args.l2_assoc)) exit(1);
            i++;
        }
        else if (strcmp(argv[i], "--l2-block") == 0) {
            if (!next_arg || !parse_int(argv[i], next_arg, &args.l2_block)) exit(1);
            i++;
        }
        else if (strcmp(argv[i], "--l2-policy") == 0) {
            if (!next_arg) { fprintf(stderr, "Missing value for %s\n", argv[i]); exit(1); }
            args.l2_policy = parse_policy(next_arg);
            i++;
        }
        else if (strcmp(argv[i], "--trace") == 0) {
            if (!next_arg) { fprintf(stderr, "Missing value for %s\n", argv[i]); exit(1); }
            args.trace_type = parse_trace(next_arg);
            i++;
        }
        else if (strcmp(argv[i], "--count") == 0) {
            if (!next_arg || !parse_int(argv[i], next_arg, &args.trace_count)) exit(1);
            i++;
        }
        else if (strcmp(argv[i], "--stride") == 0) {
            if (!next_arg || !parse_int(argv[i], next_arg, &args.trace_stride)) exit(1);
            i++;
        }
        else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            exit(1);
        }
    }
    
    args.cli_mode = 1;
    return args;
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
