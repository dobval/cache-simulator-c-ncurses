#ifndef TRACE_H
#define TRACE_H

#include "cache.h"
#include <stdbool.h>
#include <stdio.h>

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

bool trace_open(TraceReader *reader, const char *filename);
void trace_close(TraceReader *reader);
bool trace_read_entry(TraceReader *reader, TraceEntry *entry);

int trace_parse_address(const char *str);

#endif
