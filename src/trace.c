#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "trace.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
