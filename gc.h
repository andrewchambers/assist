#ifndef GC_H
#define GC_H

#include <stddef.h>
#include <stdint.h>

/*
 * Standalone Mark-and-Sweep Garbage Collector
 * 
 * This is a simple, portable garbage collector that tracks heap allocations
 * and automatically frees unreachable memory. It uses conservative stack
 * scanning to find root pointers.
 * 
 * Features:
 * - Automatic memory management via mark-and-sweep collection
 * - Conservative stack scanning to find roots
*  - Handles interior pointers
 * - Support for explicit root registration
 * 
 * Limitations:
 * - Conservative: May keep dead memory alive if integers look like pointers
 * - Not thread-safe (single-threaded use only)
 * - May not work with some optimizations that hide pointers
 */



// GC allocation entry
// The mark bit is stored in the low bit of ptr (assumes at least 2-byte alignment)
typedef struct gc_entry {
    uintptr_t ptr_and_mark; // User pointer with mark bit in LSB
    size_t size;            // Allocation size
} gc_entry;

// GC root entry
typedef struct gc_root {
    void *ptr;              // Root pointer
    size_t size;            // Size of root area
} gc_root;

// Platform-specific macro to get stack pointer
// Usage: void *sp; GC_GET_STACK_POINTER(&sp);
#if defined(__x86_64__) || defined(__amd64__)
    #define GC_GET_STACK_POINTER(ptr) \
        __asm__ volatile ("movq %%rsp, %0" : "=r"(*(ptr)))
#elif defined(__i386__)
    #define GC_GET_STACK_POINTER(ptr) \
        __asm__ volatile ("movl %%esp, %0" : "=r"(*(ptr)))
#elif defined(__aarch64__)
    #define GC_GET_STACK_POINTER(ptr) \
        __asm__ volatile ("mov %0, sp" : "=r"(*(ptr)))
#elif defined(__arm__)
    #define GC_GET_STACK_POINTER(ptr) \
        __asm__ volatile ("mov %0, sp" : "=r"(*(ptr)))
#elif defined(__riscv)
    #define GC_GET_STACK_POINTER(ptr) \
        __asm__ volatile ("mv %0, sp" : "=r"(*(ptr)))
#elif defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
    #define GC_GET_STACK_POINTER(ptr) \
        __asm__ volatile ("mr %0, 1" : "=r"(*(ptr)))
#endif

// GC state
typedef struct gc_state {
    gc_entry *allocs;           // Array of allocations (sorted by ptr)
    size_t alloc_count;         // Number of allocations
    size_t alloc_capacity;      // Capacity of allocations array
    size_t allocated_bytes;     // Total allocated memory
    size_t threshold;           // Collection threshold
    void *stack_bottom;         // Bottom of stack for scanning
    
    // Direct mapped cache for fast lookups
    gc_entry **cache;           // Direct mapped cache (pointers to entries)
    size_t cache_size;          // Size of cache (power of 2)
    size_t cache_mask;          // Mask for cache indexing (size - 1)
    
    // Root management
    gc_root *roots;             // Array of registered roots
    size_t root_count;          // Number of registered roots
    size_t root_capacity;       // Capacity of roots array
    
    // Debug flags
    int debug_stress;           // Force GC on every allocation
    int debug_print_stats;      // Print statistics during collection
    
    // Statistics for current collection
    size_t bytes_scanned;       // Bytes scanned during current collection
} gc_state;

// Initialize GC with stack bottom
void gc_init(gc_state *gc, void *stack_bottom);

// Clean up GC
void gc_cleanup(gc_state *gc);

// Allocate memory with GC tracking
void* gc_malloc(gc_state *gc, size_t size);

// Force a garbage collection
void gc_collect(gc_state *gc);

// Get total allocated bytes
size_t gc_allocated_bytes(gc_state *gc);

// Add a root to be scanned during GC
void gc_add_root(gc_state *gc, void *ptr, size_t size);

// Remove a root from GC scanning
void gc_remove_root(gc_state *gc, void *ptr);

#endif // GC_H