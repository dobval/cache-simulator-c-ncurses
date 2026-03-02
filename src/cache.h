#ifndef CACHE_H
#define CACHE_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_SETS 256
#define MAX_ASSOC 16
#define MAX_BLOCK_SIZE 128

typedef enum {
    POLICY_LRU,
    POLICY_FIFO,
    POLICY_LFU,
    POLICY_RANDOM
} EvictionPolicy;

typedef enum {
    WRITE_THROUGH,
    WRITE_BACK
} WritePolicy;

typedef struct {
    uint32_t tag;
    uint8_t valid;
    uint8_t dirty;
    uint32_t lru_counter;
    uint32_t fifo_counter;
    uint32_t freq_counter;
} CacheLine;

typedef struct {
    int num_sets;
    int associativity;
    int block_size;
    int block_offset_bits;
    int set_index_bits;
    EvictionPolicy policy;
    WritePolicy write_policy;
    CacheLine **lines;
    uint32_t global_counter;
} Cache;

typedef struct {
    uint64_t hits;
    uint64_t misses;
    uint64_t evictions;
    uint64_t reads;
    uint64_t writes;
    uint64_t memory_reads;
    uint64_t memory_writes;
} CacheStats;

typedef struct {
    Cache l1_cache;
    Cache l2_cache;
    bool enable_l2;
    CacheStats l1_stats;
    CacheStats l2_stats;
} CacheSystem;

void cache_init(Cache *cache, int num_sets, int associativity, int block_size,
                EvictionPolicy policy, WritePolicy write_policy);
void cache_free(Cache *cache);
void cache_clear(Cache *cache);

typedef enum {
    ACCESS_LOAD,
    ACCESS_STORE
} AccessType;

typedef enum {
    RESULT_HIT,
    RESULT_MISS,
    RESULT_EVICTION
} AccessResult;

AccessResult cache_access(Cache *cache, uint32_t address, AccessType type,
                          CacheStats *stats, bool *evicted);

uint32_t cache_get_tag(Cache *cache, uint32_t address);
uint32_t cache_get_set_index(Cache *cache, uint32_t address);
uint32_t cache_get_block_offset(Cache *cache, uint32_t address);

void cache_system_init(CacheSystem *sys, int l1_sets, int l1_assoc, int l1_block,
                       int l2_sets, int l2_assoc, int l2_block,
                       EvictionPolicy l1_policy, EvictionPolicy l2_policy,
                       WritePolicy write_policy, bool enable_l2);
void cache_system_free(CacheSystem *sys);
void cache_system_clear(CacheSystem *sys);
void cache_system_access(CacheSystem *sys, uint32_t address, AccessType type);

const char *policy_to_string(EvictionPolicy policy);
EvictionPolicy string_to_policy(const char *str);

#endif
