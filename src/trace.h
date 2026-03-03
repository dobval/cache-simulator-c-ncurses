#ifndef TRACE_H
#define TRACE_H

#include "cache.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

typedef enum {
    TRACE_SEQUENTIAL,
    TRACE_RANDOM,
    TRACE_LOOP,
    TRACE_STRIDED,
    TRACE_NUM
} TraceType;

typedef struct {
    char type;
    uint32_t address;
} TraceEntry;

typedef struct {
    FILE *file;
    char *filename;
    int current_line;
    bool eof;
} TraceReader;

typedef struct {
    TraceType type;
    uint32_t base_address;
    uint32_t stride;
    uint32_t access_count;
    uint32_t working_set_size;
    uint32_t current_idx;
    uint32_t seed;
} TraceGenerator;

const char *trace_type_name(TraceType type);
const char *trace_type_desc(TraceType type);

void trace_gen_init(TraceGenerator *gen, TraceType type, uint32_t base_addr, uint32_t stride, uint32_t count);
bool trace_gen_next(TraceGenerator *gen, TraceEntry *entry);
void trace_gen_reset(TraceGenerator *gen);

bool trace_open(TraceReader *reader, const char *filename);
void trace_close(TraceReader *reader);
bool trace_read_entry(TraceReader *reader, TraceEntry *entry);

int trace_parse_address(const char *str);

#endif
