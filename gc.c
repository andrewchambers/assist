
#include "gc.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// ---- Tunables --------------------------------------------------------------

#define DEFAULT_GC_THRESHOLD (1024 * 1024)

// ---- Hash table for allocations -------------------------------------------

static inline size_t hash_ptr(void *p, size_t n) {
    uintptr_t x = (uintptr_t)p;
    x ^= x >> 33; x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return (size_t)x & (n - 1);
}

static gc_entry* find_entry(gc_state *gc, void *ptr) {
    size_t i = hash_ptr(ptr, gc->hash_size), start = i;
    while (gc->hash_table[i].occupied) {
        if (gc->hash_table[i].ptr == ptr) return &gc->hash_table[i];
        i = (i + 1) & (gc->hash_size - 1);
        if (i == start) break;
    }
    return NULL;
}

static void resize_table(gc_state *gc) {
    size_t oldn = gc->hash_size, newn = oldn * 2;
    gc_entry *oldt = gc->hash_table, *newt = (gc_entry*)calloc(newn, sizeof(gc_entry));
    if (!newt) {
        fprintf(stderr, "resize_table: OOM\n");
        exit(1);
    }

    for (size_t i = 0; i < oldn; i++) if (oldt[i].occupied) {
        size_t j = hash_ptr(oldt[i].ptr, newn);
        while (newt[j].occupied) j = (j + 1) & (newn - 1);
        newt[j] = oldt[i];
    }
    gc->hash_table = newt;
    gc->hash_size  = newn;
    free(oldt);
}

static void add_entry(gc_state *gc, void *ptr, size_t size) {
    if (gc->entry_count >= gc->hash_size * GC_MAX_LOAD_FACTOR) resize_table(gc);
    size_t i = hash_ptr(ptr, gc->hash_size);
    while (gc->hash_table[i].occupied) i = (i + 1) & (gc->hash_size - 1);
    gc->hash_table[i] = (gc_entry){ .ptr = ptr, .size = size, .marked = 0, .occupied = 1 };
    gc->allocated_bytes += size;
    gc->entry_count++;
}

// ---- Mark helpers ----------------------------------------------------------

static bool mark_from_ptr(gc_state *gc, void *ptr);

static void scan_range_for_ptrs(gc_state *gc, void *start, void *end) {
    void **p = (void**)start, **q = (void**)end;
    while (p < q) {
        void *cand = *p++;
        if (cand) mark_from_ptr(gc, cand);
    }
}

static bool mark_from_ptr(gc_state *gc, void *ptr) {
    gc_entry *e = find_entry(gc, ptr);
    if (!e || e->marked) return e != NULL;
    e->marked = 1;
    scan_range_for_ptrs(gc, e->ptr, (char*)e->ptr + e->size);
    return true;
}

// ---- Stack scanning --------------------------------------------------------

static void* get_stack_pointer(void) {
    void *sp;
    volatile int x; sp = (void*)&x;
    return sp;
}

static void scan_stack(gc_state *gc) {
    void *top = get_stack_pointer(), *bot = gc->stack_bottom;
    if (top > bot) { void *t = top; top = bot; bot = t; }
    scan_range_for_ptrs(gc, top, bot);
}

// ---- Public API ------------------------------------------------------------

void gc_init(gc_state *gc, void *stack_bottom) {
    gc->hash_size = GC_INITIAL_HASH_SIZE;
    gc->hash_table = (gc_entry*)calloc(gc->hash_size, sizeof(gc_entry));
    if (!gc->hash_table) { fprintf(stderr, "gc_init: OOM\n"); exit(1); }
    gc->entry_count = 0;
    gc->allocated_bytes = 0;
    gc->threshold = DEFAULT_GC_THRESHOLD;
    gc->stack_bottom = stack_bottom;

    gc->root_capacity = GC_INITIAL_ROOTS;
    gc->roots = (gc_root*)malloc(gc->root_capacity * sizeof(gc_root));
    if (!gc->roots) { fprintf(stderr, "gc_init: OOM (roots)\n"); exit(1); }
    gc->root_count = 0;
}

void gc_cleanup(gc_state *gc) {
    for (size_t i = 0; i < gc->hash_size; i++)
        if (gc->hash_table[i].occupied) free(gc->hash_table[i].ptr);
    free(gc->hash_table); gc->hash_table = NULL; gc->hash_size = 0;
    gc->entry_count = 0; gc->allocated_bytes = 0;
    free(gc->roots); gc->roots = NULL; gc->root_count = 0; gc->root_capacity = 0;
}

void gc_collect(gc_state *gc) {
    // clear marks
    for (size_t i = 0; i < gc->hash_size; i++)
        if (gc->hash_table[i].occupied) gc->hash_table[i].marked = 0;

    // mark: stack + roots (and contents)
    scan_stack(gc);
    for (size_t i = 0; i < gc->root_count; i++) {
        gc_root *r = &gc->roots[i];
        // Mark the pointer in case the root is a gc heap object.
        mark_from_ptr(gc, r->ptr);
        // Scan the range manually for the case it is not a gc heap object.
        scan_range_for_ptrs(gc, r->ptr, (char*)r->ptr + r->size);
    }

    // sweep and rebuild hash table
    size_t old_size = gc->hash_size;
    gc_entry *old_table = gc->hash_table;
    
    // Allocate new hash table
    gc->hash_table = (gc_entry*)calloc(old_size, sizeof(gc_entry));
    if (!gc->hash_table) {
        fprintf(stderr, "gc_collect: OOM during rebuild\n");
        exit(1);
    }
    
    // Reset counts - we'll rebuild them as we copy entries
    size_t new_count = 0;
    size_t new_bytes = 0;
    
    // Copy marked entries to new table and free unmarked ones
    for (size_t i = 0; i < old_size; i++) {
        gc_entry *e = &old_table[i];
        if (e->occupied) {
            if (e->marked) {
                // Re-insert marked entry into new table
                size_t j = hash_ptr(e->ptr, gc->hash_size);
                while (gc->hash_table[j].occupied) {
                    j = (j + 1) & (gc->hash_size - 1);
                }
                gc->hash_table[j] = *e;
                new_count++;
                new_bytes += e->size;
            } else {
                // Free unmarked entry
                free(e->ptr);
            }
        }
    }
    
    // Update counts
    gc->entry_count = new_count;
    gc->allocated_bytes = new_bytes;
    
    // Free old table
    free(old_table);

    if (gc->allocated_bytes > (gc->threshold * 3) / 4) gc->threshold *= 2;
}

void* gc_malloc(gc_state *gc, size_t size) {
    if (gc->allocated_bytes + size > gc->threshold) gc_collect(gc);

    void *p = malloc(size);
    if (!p) { gc_collect(gc); p = malloc(size); }
    if (!p) { fprintf(stderr, "gc_malloc: OOM\n"); exit(1); }

    memset(p, 0, size);
    add_entry(gc, p, size);
    return p;
}

void* gc_realloc(gc_state *gc, void *ptr, size_t size) {
    if (!ptr) {
        // If ptr is NULL, behave like malloc
        return gc_malloc(gc, size);
    }
    
    if (size == 0) {
        // If size is 0, just return NULL (let GC handle the cleanup)
        return NULL;
    }
    
    // Find the existing entry
    gc_entry *entry = find_entry(gc, ptr);
    if (!entry) {
        return NULL;
    }
    
    size_t old_size = entry->size;
    
    // Allocate new memory
    void *new_ptr = gc_malloc(gc, size);
    
    // Copy data
    size_t copy_size = (old_size < size) ? old_size : size;
    memcpy(new_ptr, ptr, copy_size);
    
    // The old ptr will be collected by GC eventually
    return new_ptr;
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
