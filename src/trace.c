#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "trace.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

const char *trace_type_name(TraceType type) {
    switch (type) {
        case TRACE_SEQUENTIAL:   return "Sequential";
        case TRACE_RANDOM:       return "Random";
        case TRACE_LOOP:         return "Loop";
        case TRACE_STRIDED:      return "Strided";
        default:                 return "Unknown";
    }
}

const char *trace_type_desc(TraceType type) {
    switch (type) {
        case TRACE_SEQUENTIAL:   return "Streaming access (0, 64, 128...)";
        case TRACE_RANDOM:       return "Random addresses";
        case TRACE_LOOP:        return "Repeated same lines";
        case TRACE_STRIDED:      return "Fixed stride pattern";
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

        case TRACE_STRIDED:
            addr = gen->base_address + (idx * gen->stride);
            entry->type = (idx % 5 == 0) ? 'S' : 'L';
            break;

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

bool trace_open(TraceReader *reader, const char *filename) {
    reader->file = fopen(filename, "r");
    if (!reader->file) {
        return false;
    }
    reader->filename = strdup(filename);
    reader->current_line = 0;
    reader->eof = false;
    return true;
}

void trace_close(TraceReader *reader) {
    if (reader->file) {
        fclose(reader->file);
        reader->file = NULL;
    }
    if (reader->filename) {
        free(reader->filename);
        reader->filename = NULL;
    }
}

int trace_parse_address(const char *str) {
    int addr = 0;
    const char *ptr = str;
    
    if (strncmp(ptr, "0x", 2) == 0 || strncmp(ptr, "0X", 2) == 0) {
        ptr += 2;
    }
    
    while (*ptr) {
        char c = *ptr;
        int nibble;
        if (c >= '0' && c <= '9') nibble = c - '0';
        else if (c >= 'a' && c <= 'f') nibble = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') nibble = c - 'A' + 10;
        else break;
        addr = (addr << 4) | nibble;
        ptr++;
    }
    
    return addr;
}

bool trace_read_entry(TraceReader *reader, TraceEntry *entry) {
    char line[256];
    
    while (fgets(line, sizeof(line), reader->file)) {
        reader->current_line++;
        
        if (line[0] == '#' || line[0] == '\n' || line[0] == ' ') {
            continue;
        }
        
        char type;
        unsigned int addr;
        
        if (sscanf(line, " %c %x", &type, &addr) == 2) {
            if (type == 'L' || type == 'l' || type == 'S' || type == 's') {
                entry->type = (type == 'L' || type == 'l') ? 'L' : 'S';
                entry->address = addr;
                return true;
            }
        }
        
        if (sscanf(line, "%x", &addr) == 1) {
            entry->type = 'L';
            entry->address = addr;
            return true;
        }
    }
    
    reader->eof = true;
    return false;
}
