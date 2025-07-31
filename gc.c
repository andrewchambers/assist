
#include "gc.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// ---- Tunables --------------------------------------------------------------

#define DEFAULT_GC_THRESHOLD (1024 * 1024)

// ---- Array-based allocation tracking ---------------------------------------

// Comparison function for sorting gc_entry by pointer
static int entry_compare(const void *a, const void *b) {
    const gc_entry *ea = (const gc_entry *)a;
    const gc_entry *eb = (const gc_entry *)b;
    if (ea->ptr < eb->ptr) return -1;
    if (ea->ptr > eb->ptr) return 1;
    return 0;
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
    
    // Binary search to find the allocation containing this pointer
    size_t left = 0;
    size_t right = gc->alloc_count;
    
    while (left < right) {
        size_t mid = left + (right - left) / 2;
        void *alloc_start = gc->allocs[mid].ptr;
        void *alloc_end = (char*)alloc_start + gc->allocs[mid].size;
        
        if (ptr >= alloc_start && ptr < alloc_end) {
            // Found it - ptr is inside this allocation
            return &gc->allocs[mid];
        } else if (ptr < alloc_start) {
            right = mid;
        } else {
            // ptr >= alloc_end
            left = mid + 1;
        }
    }
    
    // Special case: check if ptr might be in the last allocation
    // (needed when ptr == end of last allocation)
    if (left > 0) {
        size_t last = left - 1;
        void *alloc_start = gc->allocs[last].ptr;
        void *alloc_end = (char*)alloc_start + gc->allocs[last].size;
        if (ptr >= alloc_start && ptr < alloc_end) {
            return &gc->allocs[last];
        }
    }
    
    return NULL;
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

    gc->root_capacity = GC_INITIAL_ROOTS;
    gc->roots = (gc_root*)malloc(gc->root_capacity * sizeof(gc_root));
    if (!gc->roots) { fprintf(stderr, "gc_init: OOM (roots)\n"); exit(1); }
    gc->root_count = 0;
}

void gc_cleanup(gc_state *gc) {
    for (size_t i = 0; i < gc->alloc_count; i++)
        free(gc->allocs[i].ptr);
    free(gc->allocs); gc->allocs = NULL; gc->alloc_capacity = 0;
    gc->alloc_count = 0; gc->allocated_bytes = 0;
    free(gc->roots); gc->roots = NULL; gc->root_count = 0; gc->root_capacity = 0;
}

void gc_collect(gc_state *gc) {
    // First, sort the allocations array for binary search
    qsort(gc->allocs, gc->alloc_count, sizeof(gc_entry), entry_compare);
    
    // clear marks
    for (size_t i = 0; i < gc->alloc_count; i++)
        gc->allocs[i].marked = 0;

    // mark: stack + roots (and contents)
    scan_stack(gc);
    for (size_t i = 0; i < gc->root_count; i++) {
        gc_root *r = &gc->roots[i];
        // Mark the pointer in case the root is a gc heap object.
        mark_from_ptr(gc, r->ptr);
        // Scan the range manually for the case it is not a gc heap object.
        scan_range_for_ptrs(gc, r->ptr, (char*)r->ptr + r->size);
    }

    // sweep: compact array by removing unmarked entries
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

    if (gc->allocated_bytes > (gc->threshold * 3) / 4) gc->threshold *= 2;
}

void* gc_malloc(gc_state *gc, size_t size) {
    if (gc->allocated_bytes + size > gc->threshold) gc_collect(gc);

    void *p = malloc(size);
    if (!p) { gc_collect(gc); p = malloc(size); }
    if (!p) { fprintf(stderr, "gc_malloc: OOM\n"); exit(1); }

    memset(p, 0, size);
    add_entry(gc, p, size);
    
    // Note: Array becomes unsorted after add, but that's OK.
    // It will be sorted at next gc_collect() before any lookups.
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
    
    // We need the array to be sorted for lookup - if it's not sorted, sort it now
    // This can happen if allocations were made since last gc_collect
    // For now, always sort to be safe (can optimize later with a sorted flag)
    if (gc->alloc_count > 0) {
        qsort(gc->allocs, gc->alloc_count, sizeof(gc_entry), entry_compare);
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
