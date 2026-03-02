#include "cache.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

static int log2_int(int value) {
    int result = 0;
    while (value > 1) {
        value >>= 1;
        result++;
    }
    return result;
}

void cache_init(Cache *cache, int num_sets, int associativity, int block_size,
                EvictionPolicy policy, WritePolicy write_policy) {
    cache->num_sets = num_sets;
    cache->associativity = associativity;
    cache->block_size = block_size;
    cache->policy = policy;
    cache->write_policy = write_policy;
    cache->global_counter = 0;

    cache->block_offset_bits = log2_int(block_size);
    cache->set_index_bits = log2_int(num_sets);

    cache->lines = (CacheLine **)malloc(num_sets * sizeof(CacheLine *));
    for (int i = 0; i < num_sets; i++) {
        cache->lines[i] = (CacheLine *)malloc(associativity * sizeof(CacheLine));
        for (int j = 0; j < associativity; j++) {
            cache->lines[i][j].valid = 0;
            cache->lines[i][j].dirty = 0;
            cache->lines[i][j].tag = 0;
            cache->lines[i][j].lru_counter = 0;
            cache->lines[i][j].fifo_counter = 0;
            cache->lines[i][j].freq_counter = 0;
        }
    }
}

void cache_free(Cache *cache) {
    if (cache->lines) {
        for (int i = 0; i < cache->num_sets; i++) {
            free(cache->lines[i]);
        }
        free(cache->lines);
        cache->lines = NULL;
    }
}

void cache_clear(Cache *cache) {
    cache->global_counter = 0;
    for (int i = 0; i < cache->num_sets; i++) {
        for (int j = 0; j < cache->associativity; j++) {
            cache->lines[i][j].valid = 0;
            cache->lines[i][j].dirty = 0;
            cache->lines[i][j].tag = 0;
            cache->lines[i][j].lru_counter = 0;
            cache->lines[i][j].fifo_counter = 0;
            cache->lines[i][j].freq_counter = 0;
        }
    }
}

uint32_t cache_get_tag(Cache *cache, uint32_t address) {
    uint32_t tag = address >> (cache->set_index_bits + cache->block_offset_bits);
    return tag;
}

uint32_t cache_get_set_index(Cache *cache, uint32_t address) {
    uint32_t mask = (1 << cache->set_index_bits) - 1;
    return (address >> cache->block_offset_bits) & mask;
}

uint32_t cache_get_block_offset(Cache *cache, uint32_t address) {
    uint32_t mask = (1 << cache->block_offset_bits) - 1;
    return address & mask;
}

static int find_eviction_line(Cache *cache, int set_idx) {
    int evict_idx = 0;
    uint32_t min_value = cache->lines[set_idx][0].lru_counter;
    
    switch (cache->policy) {
        case POLICY_LRU:
            for (int i = 1; i < cache->associativity; i++) {
                if (cache->lines[set_idx][i].lru_counter < min_value) {
                    min_value = cache->lines[set_idx][i].lru_counter;
                    evict_idx = i;
                }
            }
            break;
            
        case POLICY_FIFO:
            min_value = cache->lines[set_idx][0].fifo_counter;
            for (int i = 1; i < cache->associativity; i++) {
                if (cache->lines[set_idx][i].fifo_counter < min_value) {
                    min_value = cache->lines[set_idx][i].fifo_counter;
                    evict_idx = i;
                }
            }
            break;
            
        case POLICY_LFU:
            min_value = cache->lines[set_idx][0].freq_counter;
            for (int i = 1; i < cache->associativity; i++) {
                if (cache->lines[set_idx][i].freq_counter < min_value) {
                    min_value = cache->lines[set_idx][i].freq_counter;
                    evict_idx = i;
                }
            }
            break;
            
        case POLICY_RANDOM:
            evict_idx = rand() % cache->associativity;
            break;
    }
    
    return evict_idx;
}

static int find_invalid_line(Cache *cache, int set_idx) {
    for (int i = 0; i < cache->associativity; i++) {
        if (!cache->lines[set_idx][i].valid) {
            return i;
        }
    }
    return -1;
}

AccessResult cache_access(Cache *cache, uint32_t address, AccessType type,
                          CacheStats *stats, bool *evicted) {
    uint32_t tag = cache_get_tag(cache, address);
    uint32_t set_idx = cache_get_set_index(cache, address);
    
    *evicted = false;
    
    if (type == ACCESS_LOAD) {
        stats->reads++;
    } else {
        stats->writes++;
    }
    
    for (int i = 0; i < cache->associativity; i++) {
        if (cache->lines[set_idx][i].valid && cache->lines[set_idx][i].tag == tag) {
            if (type == ACCESS_STORE) {
                if (cache->write_policy == WRITE_BACK) {
                    cache->lines[set_idx][i].dirty = 1;
                } else {
                    stats->memory_writes++;
                }
            }
            
            switch (cache->policy) {
                case POLICY_LRU:
                    cache->lines[set_idx][i].lru_counter = cache->global_counter++;
                    break;
                case POLICY_FIFO:
                    break;
                case POLICY_LFU:
                    cache->lines[set_idx][i].freq_counter++;
                    break;
                case POLICY_RANDOM:
                    break;
            }
            
            stats->hits++;
            return RESULT_HIT;
        }
    }
    
    stats->misses++;
    stats->memory_reads++;
    
    int line_idx = find_invalid_line(cache, set_idx);
    
    if (line_idx == -1) {
        line_idx = find_eviction_line(cache, set_idx);
        *evicted = true;
        stats->evictions++;
        
        if (cache->write_policy == WRITE_BACK && cache->lines[set_idx][line_idx].dirty) {
            stats->memory_writes++;
        }
    }
    
    cache->lines[set_idx][line_idx].valid = 1;
    cache->lines[set_idx][line_idx].tag = tag;
    cache->lines[set_idx][line_idx].dirty = 0;
    
    switch (cache->policy) {
        case POLICY_LRU:
            cache->lines[set_idx][line_idx].lru_counter = cache->global_counter++;
            break;
        case POLICY_FIFO:
            cache->lines[set_idx][line_idx].fifo_counter = cache->global_counter++;
            break;
        case POLICY_LFU:
            cache->lines[set_idx][line_idx].freq_counter = 1;
            break;
        case POLICY_RANDOM:
            break;
    }
    
    if (type == ACCESS_STORE) {
        if (cache->write_policy == WRITE_BACK) {
            cache->lines[set_idx][line_idx].dirty = 1;
        } else {
            stats->memory_writes++;
        }
    }
    
    return *evicted ? RESULT_EVICTION : RESULT_MISS;
}

void cache_system_init(CacheSystem *sys, int l1_sets, int l1_assoc, int l1_block,
                       int l2_sets, int l2_assoc, int l2_block,
                       EvictionPolicy l1_policy, EvictionPolicy l2_policy,
                       WritePolicy write_policy, bool enable_l2) {
    cache_init(&sys->l1_cache, l1_sets, l1_assoc, l1_block, l1_policy, write_policy);
    
    sys->enable_l2 = enable_l2;
    if (enable_l2) {
        cache_init(&sys->l2_cache, l2_sets, l2_assoc, l2_block, l2_policy, write_policy);
    }
    
    memset(&sys->l1_stats, 0, sizeof(CacheStats));
    memset(&sys->l2_stats, 0, sizeof(CacheStats));
    
    srand(time(NULL));
}

void cache_system_free(CacheSystem *sys) {
    cache_free(&sys->l1_cache);
    if (sys->enable_l2) {
        cache_free(&sys->l2_cache);
    }
}

void cache_system_clear(CacheSystem *sys) {
    cache_clear(&sys->l1_cache);
    if (sys->enable_l2) {
        cache_clear(&sys->l2_cache);
    }
    memset(&sys->l1_stats, 0, sizeof(CacheStats));
    memset(&sys->l2_stats, 0, sizeof(CacheStats));
}

void cache_system_access(CacheSystem *sys, uint32_t address, AccessType type) {
    bool evicted;
    AccessResult result = cache_access(&sys->l1_cache, address, type, &sys->l1_stats, &evicted);
    
    if (sys->enable_l2 && (result == RESULT_MISS || result == RESULT_EVICTION)) {
        bool l2_evicted;
        cache_access(&sys->l2_cache, address, type, &sys->l2_stats, &l2_evicted);
    }
}

const char *policy_to_string(EvictionPolicy policy) {
    switch (policy) {
        case POLICY_LRU: return "LRU";
        case POLICY_FIFO: return "FIFO";
        case POLICY_LFU: return "LFU";
        case POLICY_RANDOM: return "RANDOM";
        default: return "UNKNOWN";
    }
}

EvictionPolicy string_to_policy(const char *str) {
    if (strcmp(str, "LRU") == 0) return POLICY_LRU;
    if (strcmp(str, "FIFO") == 0) return POLICY_FIFO;
    if (strcmp(str, "LFU") == 0) return POLICY_LFU;
    if (strcmp(str, "RANDOM") == 0) return POLICY_RANDOM;
    return POLICY_LRU;
}
