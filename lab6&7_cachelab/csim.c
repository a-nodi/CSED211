#include "cachelab.h"

// Define Cache components
typedef struct {
    int valid; // vaild bit
    unsigned long long tag; // tag
    unsigned long long lru;
} Line;

typedef struct {
    Line* line; // line
} Set;

typedef struct {
    Set* set; // set
    // Meta data
    int s;
    int E;
    int b;
} Cache;

int main()
{
    while ((option = getopt(argc, argv, "s:E:b:t:hv")) != -1){
        if (option == 'v')
            is_display_trace = 1;
        else if (option == 's')
            s = atoi(optarg);
        else if (option == 'E')
            E = atoi(optarg);
        else if (option == 'b')
            b = atoi(optarg);
        else if (option == 't')
            t = optarg;
        else {
            print_help();
            return 1;
        }
    }

    /*
        I accidently deleted the original file
        I restored it as much as I can.
        :(
    */

}

Cache* construct_cache(Cache* cache, int s, int E, int b){
    // Allocate cache
    cache = malloc(sizeof(Cache));
    // Record metadata
    cache->s = s;
    cache->E = E;
    cache->b = b;
    // Allocate sets
    cache->set = calloc(1 << s, sizeof(Set));
    // Allocate lines
    for (int i = 0; i < (1 << s); i++)
        cache->set[i].line = calloc(E, sizeof(Line));
    return cache;
}

void simulate_trace(FILE* fp, Cache* cache, int is_display_trace, int* hit_count, int* miss_count, int* eviction_count){
    // Trace variable
    char operation;
    unsigned long long address;
    unsigned int size;
    int result;
    unsigned int global_lru = 0;
    unsigned long long tag, set_index, block_offset;
    unsigned long long mask_of_set_index = 0, mask_of_block_offset = 0;
    
    // Create mask of set index
    mask_of_set_index = (~0) << (64 - cache->s - cache->b);
    mask_of_set_index = mask_of_set_index >> (64 - cache->s);
    mask_of_set_index = mask_of_set_index << (cache->b);
    
    // Create mask of block offset
    mask_of_block_offset = (~0) << (64 - cache->b);
    mask_of_block_offset = mask_of_block_offset >> (64 - cache->b);
    
    // Read trace file
    while (fscanf(fp, " %c %llx, %u" , &operation, &address, &size) != EOF) {
        // Parse address
        tag = (address) >> (cache->s + cache->b);
        set_index = (address & mask_of_set_index) >> (cache->b);
        block_offset = address & mask_of_block_offset;

        if (operation == 'I')
            continue;

        else if (operation == 'L') {
            result = access(cache, tag, set_index, block_offset, &global_lru);
            update_count(result, hit_count, miss_count, eviction_count);
            
            if (is_display_trace){
                printf("%c %llx, %u ", operation, address, size);
                display_trace(result);
                printf("\n");
            }
        }

        else if (operation == 'M') {
            result = access(cache, tag, set_index, block_offset, &global_lru);
            update_count(result, hit_count, miss_count, eviction_count);
            if (is_display_trace) {
                printf("%c %llx, %u ", operation, address, size);
                display_trace(result);
            }
            result = access(cache, tag, set_index, block_offset, &global_lru);
            update_count(result, hit_count, miss_count, eviction_count);
            if (is_display_trace) {
                display_trace(result);
                printf("\n");
            }
        }

        else if (operation == 'S') {
            result = access(cache, tag, set_index, block_offset, &global_lru);
            update_count(result, hit_count, miss_count, eviction_count);
                if (is_display_trace) {
                printf("%c %llx, %u ", operation, address, size);
                display_trace(result);
                printf("\n");
            }
        }
    }
    return;
}

int access(Cache* cache, unsigned long long tag, unsigned long long set_index, unsigned long long block_offset, unsigned int* global_lru) {
    unsigned int min_lru = 4294967295;
    int target_index = 0;
    for (int i = 0; i < cache->E; i++){
        if (cache->set[set_index].line[i].valid == 1){
            if (cache->set[set_index].line[i].tag == tag){
                cache->set[set_index].line[i].lru = *global_lru;
                (*global_lru)++;
                return 0;
            }
        }
    }
    
    // If the function did not return, it is cold miss.
    for (int i = 0; i < cache->E; i++){
        if (cache->set[set_index].line[i].valid == 0){
            cache->set[set_index].line[i].valid = 1;
            cache->set[set_index].line[i].tag = tag;
            cache->set[set_index].line[i].lru = *global_lru;
            (*global_lru)++;
            return 1;
        }
    }
    
    // If the function did not return, it is eviction miss
    for (int i = 0; i < cache->E; i++){
        if (cache->set[set_index].line[i].lru < min_lru){
            min_lru = cache->set[set_index].line[i].lru;
            target_index = i;
        }
    }
    
    cache->set[set_index].line[target_index].tag = tag;
    cache->set[set_index].line[target_index].lru = *global_lru;
    (*global_lru)++;
    
    return 2;
}

void update_count(int result, int* hit_count, int* miss_count, int* eviction_count){
    if (result == 0)
        (*hit_count)++;
    
    else if (result == 1){
        (*miss_count)++;
    }
    
    else if (result == 2){
        (*miss_count)++;
        (*eviction_count)++;
    }

    return;
}

void free_cache(Cache* cache){
    // Free lines
    for (int i = 0; i < cache->s; i++)
        free(cache->set[i].line);
    
    // Free sets
    free(cache->set);
    
    // Free cache
    free(cache);
    
    return;
}