
#include "gc.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <setjmp.h>
#include <time.h>

// ---- Tunables --------------------------------------------------------------

#define DEFAULT_GC_THRESHOLD (512*1024)
#define GC_INITIAL_ROOTS 16
#define GC_INITIAL_ALLOC_SIZE 256


// ---- Array-based allocation tracking ---------------------------------------

// Compute direct mapped cache index for a pointer
static size_t dm_cache_idx(gc_state *gc, void *ptr) {
    // Mix the bits of the pointer for better distribution
    uintptr_t p = (uintptr_t)ptr;
    p ^= p >> 16;
    p *= 0x85ebca6b;
    p ^= p >> 13;
    p *= 0xc2b2ae35;
    p ^= p >> 16;
    return (size_t)p & gc->cache_mask;
}

// Round up to next power of 2
static size_t next_power_of_2(size_t n) {
    if (n == 0) return 1;
    
    // Find the position of the highest set bit
    size_t power = 1;
    while (power < n) {
        power <<= 1;
    }
    return power;
}

// Comparison function for sorting gc_entry by pointer
static int entry_compare(const void *a, const void *b) {
    const gc_entry *ea = (const gc_entry *)a;
    const gc_entry *eb = (const gc_entry *)b;
    if (ea->ptr < eb->ptr) return -1;
    if (ea->ptr > eb->ptr) return 1;
    return 0;
}

// Comparison function for bsearch - finds the allocation containing ptr
static int find_entry_compare(const void *key, const void *entry) {
    void *search_ptr = *(void * const *)key;
    const gc_entry *e = (const gc_entry *)entry;
    
    void *alloc_start = e->ptr;
    void *alloc_end = (char*)alloc_start + e->size;
    
    if (search_ptr < alloc_start) {
        return -1;  // search_ptr is before this entry
    } else if (search_ptr >= alloc_end) {
        return 1;   // search_ptr is after this entry
    } else {
        return 0;   // search_ptr is within this entry
    }
}

// Binary search to find entry by pointer (supports interior pointers)
static gc_entry* find_entry(gc_state *gc, void *ptr) {
    if (gc->alloc_count == 0) return NULL;
    
    // Fast path: check if pointer is within the range of all allocations
    void *min_ptr = gc->allocs[0].ptr;
    void *max_ptr = (char*)gc->allocs[gc->alloc_count - 1].ptr + gc->allocs[gc->alloc_count - 1].size;
    if (ptr < min_ptr || ptr >= max_ptr) {
        return NULL;  // Pointer is outside heap range
    }

    // Next try direct mapped cache for exact pointer match
    size_t index = dm_cache_idx(gc, ptr);
    gc_entry *cached = gc->cache[index];
    if (cached && cached->ptr == ptr) {
        return cached;
    }
    
    // Finally, bsearch that takes into account interior pointers.
    return (gc_entry*)bsearch(&ptr, gc->allocs, gc->alloc_count, 
                              sizeof(gc_entry), find_entry_compare);
}

// Grow the allocations array when needed
static void grow_alloc_array(gc_state *gc) {
    size_t new_capacity = gc->alloc_capacity * 2;
    gc_entry *new_allocs = (gc_entry*)realloc(gc->allocs, new_capacity * sizeof(gc_entry));
    if (!new_allocs) {
        fprintf(stderr, "grow_alloc_array: OOM\n");
        exit(1);
    }
    gc->allocs = new_allocs;
    gc->alloc_capacity = new_capacity;
}

// Add a new allocation entry (unsorted append)
static void add_entry(gc_state *gc, void *ptr, size_t size) {
    if (gc->alloc_count >= gc->alloc_capacity) {
        grow_alloc_array(gc);
    }
    gc->allocs[gc->alloc_count] = (gc_entry){ .ptr = ptr, .size = size, .marked = 0 };
    gc->allocated_bytes += size;
    gc->alloc_count++;
}

// ---- Mark helpers ----------------------------------------------------------

static bool mark_from_ptr(gc_state *gc, void *ptr);

static void scan_range_for_ptrs(gc_state *gc, void *start, void *end) {
    void **p = (void**)start, **q = (void**)end;
    size_t bytes = (char*)end - (char*)start;
    gc->bytes_scanned += bytes;
    while (p < q) {
        void *cand = *p++;
        if (cand) mark_from_ptr(gc, cand);
    }
}

static bool mark_from_ptr(gc_state *gc, void *ptr) {
    // Array must be sorted for binary search to work
    // During gc_collect, array is already sorted
    gc_entry *e = find_entry(gc, ptr);
    if (!e || e->marked) return e != NULL;
    e->marked = 1;
    scan_range_for_ptrs(gc, e->ptr, (char*)e->ptr + e->size);
    return true;
}

// ---- Stack scanning --------------------------------------------------------

static void scan_stack(gc_state *gc) {
    void *top;
    GC_GET_STACK_POINTER(&top);
    void *bot = gc->stack_bottom;
    if (top > bot) { void *t = top; top = bot; bot = t; }
    scan_range_for_ptrs(gc, top, bot);
}

// ---- Public API ------------------------------------------------------------

void gc_init(gc_state *gc, void *stack_bottom) {
    gc->alloc_capacity = GC_INITIAL_ALLOC_SIZE;
    gc->allocs = (gc_entry*)malloc(gc->alloc_capacity * sizeof(gc_entry));
    if (!gc->allocs) { fprintf(stderr, "gc_init: OOM\n"); exit(1); }
    gc->alloc_count = 0;
    gc->allocated_bytes = 0;
    gc->threshold = DEFAULT_GC_THRESHOLD;
    gc->stack_bottom = stack_bottom;

    // Initialize cache with minimal size
    gc->cache_size = 16;  // Minimum cache size
    gc->cache_mask = gc->cache_size - 1;
    gc->cache = (gc_entry**)calloc(gc->cache_size, sizeof(gc_entry*));
    if (!gc->cache) { fprintf(stderr, "gc_init: OOM (cache)\n"); exit(1); }

    gc->root_capacity = GC_INITIAL_ROOTS;
    gc->roots = (gc_root*)malloc(gc->root_capacity * sizeof(gc_root));
    if (!gc->roots) { fprintf(stderr, "gc_init: OOM (roots)\n"); exit(1); }
    gc->root_count = 0;
    
    gc->debug_stress = 0;  // Default: stress testing disabled
    gc->debug_print_stats = 0;  // Default: stats printing disabled
}

void gc_cleanup(gc_state *gc) {
    for (size_t i = 0; i < gc->alloc_count; i++)
        free(gc->allocs[i].ptr);
    free(gc->allocs); gc->allocs = NULL; gc->alloc_capacity = 0;
    gc->alloc_count = 0; gc->allocated_bytes = 0;
    free(gc->cache); gc->cache = NULL;
    free(gc->roots); gc->roots = NULL; gc->root_count = 0; gc->root_capacity = 0;
}

// Helper to get time in microseconds
static double get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000.0 + ts.tv_nsec / 1000.0;
}

void gc_collect(gc_state *gc) {
    // Record stats before collection
    size_t old_count = gc->alloc_count;
    size_t old_bytes = gc->allocated_bytes;
    double start_time = 0;
    
    if (gc->debug_print_stats) {
        start_time = get_time_us();
    }
    
    // Reset scan counter
    gc->bytes_scanned = 0;
    
    // Phase 1: Setup (sort allocations and rebuild cache)
    double setup_start = gc->debug_print_stats ? get_time_us() : 0;
    
    // Sort allocations for binary search
    qsort(gc->allocs, gc->alloc_count, sizeof(gc_entry), entry_compare);
    
    // Calculate desired cache size (minimum 16)
    size_t desired_size = gc->alloc_count > 16 ? next_power_of_2(gc->alloc_count) : 16;
    
    // Reallocate cache if needed (too small or more than 4x too large)
    if (gc->cache_size < desired_size || gc->cache_size > desired_size * 4) {
        gc_entry **new_cache = (gc_entry**)realloc(gc->cache, desired_size * sizeof(gc_entry*));
        if (!new_cache) { fprintf(stderr, "gc_collect: OOM (cache)\n"); exit(1); }
        gc->cache = new_cache;
        gc->cache_size = desired_size;
        gc->cache_mask = desired_size - 1;
    }
    
    // Clear and populate the direct mapped cache
    memset(gc->cache, 0, gc->cache_size * sizeof(gc_entry*));
    for (size_t i = 0; i < gc->alloc_count; i++) {
        size_t index = dm_cache_idx(gc, gc->allocs[i].ptr);
        gc->cache[index] = &gc->allocs[i];
    }
    
    double setup_time = gc->debug_print_stats ? get_time_us() - setup_start : 0;
    
    // clear marks
    for (size_t i = 0; i < gc->alloc_count; i++)
        gc->allocs[i].marked = 0;

    // Phase 2: Mark reachable objects
    double mark_start = gc->debug_print_stats ? get_time_us() : 0;
    
    // Save registers using setjmp and scan them
    jmp_buf regs;
    setjmp(regs);
    
    // Scan the jmp_buf for pointers
    // jmp_buf is an array type, so we scan it as a memory region
    scan_range_for_ptrs(gc, &regs, (char*)&regs + sizeof(regs));
    
    // mark: stack + roots (and contents)
    scan_stack(gc);
    for (size_t i = 0; i < gc->root_count; i++) {
        gc_root *r = &gc->roots[i];
        // Mark the pointer in case the root is a gc heap object.
        mark_from_ptr(gc, r->ptr);
        // Scan the range manually for the case it is not a gc heap object.
        scan_range_for_ptrs(gc, r->ptr, (char*)r->ptr + r->size);
    }
    
    double mark_time = gc->debug_print_stats ? get_time_us() - mark_start : 0;

    
    // Phase 3: Sweep unreachable objects
    double sweep_start = gc->debug_print_stats ? get_time_us() : 0;
    
    size_t write_pos = 0;
    size_t new_bytes = 0;
    
    for (size_t read_pos = 0; read_pos < gc->alloc_count; read_pos++) {
        gc_entry *e = &gc->allocs[read_pos];
        if (e->marked) {
            // Keep marked entry
            if (write_pos != read_pos) {
                gc->allocs[write_pos] = *e;
            }
            write_pos++;
            new_bytes += e->size;
        } else {
            // Free unmarked entry
            free(e->ptr);
        }
    }

    // Update counts
    gc->alloc_count = write_pos;
    gc->allocated_bytes = new_bytes;
    
    // Shrink the allocations array if it's more than 4x too large
    if (gc->alloc_capacity > gc->alloc_count * 4 && gc->alloc_capacity > GC_INITIAL_ALLOC_SIZE) {
        size_t new_capacity = gc->alloc_count * 2;
        if (new_capacity < GC_INITIAL_ALLOC_SIZE) {
            new_capacity = GC_INITIAL_ALLOC_SIZE;
        }
        gc_entry *new_allocs = (gc_entry*)realloc(gc->allocs, new_capacity * sizeof(gc_entry));
        if(!new_allocs) { fprintf(stderr, "gc_collect: OOM (shrink allocs)\n"); exit(1); }
        gc->allocs = new_allocs;
        gc->alloc_capacity = new_capacity;
    }

    double sweep_time = gc->debug_print_stats ? get_time_us() - sweep_start : 0;
    
    if (gc->debug_print_stats) {
        double total_time = get_time_us() - start_time;
        size_t freed_count = old_count - gc->alloc_count;
        size_t freed_bytes = old_bytes - gc->allocated_bytes;
        
        fprintf(stderr, "GC: %zu->%zu allocs, %zu->%zu bytes (freed %zu/%zu), scanned %zu bytes, %.0fus (setup:%.1f%% mark:%.1f%% sweep:%.1f%%)\n",
                old_count, gc->alloc_count,
                old_bytes, gc->allocated_bytes,
                freed_count, freed_bytes,
                gc->bytes_scanned,
                total_time,
                (setup_time/total_time)*100,
                (mark_time/total_time)*100,
                (sweep_time/total_time)*100);
    }

    if (gc->allocated_bytes > (gc->threshold * 3) / 4) gc->threshold *= 2;
}

void* gc_malloc(gc_state *gc, size_t size) {
    // Debug stress mode: force collection before every allocation
    if (gc->debug_stress || (gc->allocated_bytes + size > gc->threshold)) {
        gc_collect(gc);
    }

    void *p = malloc(size);
    if (!p) { gc_collect(gc); p = malloc(size); }
    if (!p) { fprintf(stderr, "gc_malloc: OOM\n"); exit(1); }

    memset(p, 0, size);
    add_entry(gc, p, size);
    
    // Note: Array becomes unsorted after add, but that's OK.
    // It will be sorted at next gc_collect() before any lookups.
    return p;
}

size_t gc_allocated_bytes(gc_state *gc) { return gc->allocated_bytes; }

void gc_add_root(gc_state *gc, void *ptr, size_t size) {
    if (gc->root_count >= gc->root_capacity) {
        size_t n = gc->root_capacity * 2;
        gc_root *nr = (gc_root*)realloc(gc->roots, n * sizeof(gc_root));
        if (!nr) { fprintf(stderr, "gc_add_root: OOM\n"); return; }
        gc->roots = nr; gc->root_capacity = n;
    }
    gc->roots[gc->root_count++] = (gc_root){ .ptr = ptr, .size = size };
}

void gc_remove_root(gc_state *gc, void *ptr) {
    for (size_t i = 0; i < gc->root_count; i++) {
        if (gc->roots[i].ptr == ptr) {
            gc->roots[i] = gc->roots[gc->root_count - 1];
            gc->root_count--;
            return;
        }
    }
}
