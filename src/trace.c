#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "trace.h"
#include <stdint.h>

const char *trace_type_name(TraceType type) {
    switch (type) {
        case TRACE_LOOP:         return "Loop";
        case TRACE_RANDOM:       return "Random";
        case TRACE_SEQUENTIAL:   return "Sequential";
        default:                 return "Unknown";
    }
}

const char *trace_type_desc(TraceType type) {
    switch (type) {
        case TRACE_LOOP:         return "Repeated same lines";
        case TRACE_RANDOM:       return "Random addresses";
        case TRACE_SEQUENTIAL:   return "Streaming access (0, 64, 128...)";
        default:                  return "";
    }
}

static uint32_t lcg_rand(uint32_t *seed) {
    *seed = (*seed * 1103515245 + 12345) & 0x7fffffff;
    return *seed;
}

void trace_gen_init(TraceGenerator *gen, TraceType type, uint32_t base_addr, uint32_t stride, uint32_t count) {
    gen->type = type;
    gen->base_address = base_addr;
    gen->stride = stride;
    gen->access_count = count;
    gen->working_set_size = 256;
    gen->current_idx = 0;
    gen->seed = 12345;
}

bool trace_gen_next(TraceGenerator *gen, TraceEntry *entry) {
    if (gen->current_idx >= gen->access_count) {
        return false;
    }

    uint32_t idx = gen->current_idx++;
    uint32_t addr = 0;

    switch (gen->type) {
        case TRACE_SEQUENTIAL:
            addr = gen->base_address + (idx * gen->stride);
            entry->type = (idx % 4 == 3) ? 'S' : 'L';
            break;

        case TRACE_RANDOM:
            addr = gen->base_address + (lcg_rand(&gen->seed) % (gen->working_set_size * 64));
            entry->type = (lcg_rand(&gen->seed) % 4 == 0) ? 'S' : 'L';
            break;

        case TRACE_LOOP: {
            uint32_t loop_size = 8;
            uint32_t offset = (idx % loop_size) * 64;
            addr = gen->base_address + offset;
            entry->type = (idx % 3 == 0) ? 'S' : 'L';
            break;
        }

        default:
            addr = gen->base_address;
            entry->type = 'L';
            break;
    }

    entry->address = addr;
    return true;
}

void trace_gen_reset(TraceGenerator *gen) {
    gen->current_idx = 0;
    gen->seed = 12345;
}
