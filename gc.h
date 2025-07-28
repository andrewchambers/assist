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
 * - Tracks all allocations in a hash table
 * - Conservative stack scanning to find roots
 * - Support for explicit root registration
 * 
 * Limitations:
 * - Conservative: May keep dead memory alive if integers look like pointers
 * - No interior pointer support: Only pointers to the start of allocations are recognized
 * - Not thread-safe (single-threaded use only)
 * - May not work with some optimizations that hide pointers
 */

// Initial hash table size (must be power of 2)
#define GC_INITIAL_HASH_SIZE 256
#define GC_MAX_LOAD_FACTOR 0.75

// GC allocation entry
typedef struct gc_entry {
    void *ptr;              // User pointer (key)
    size_t size;            // Allocation size
    uint8_t marked;         // Mark bit
    uint8_t occupied;       // Slot is occupied
} gc_entry;

// GC root entry
typedef struct gc_root {
    void *ptr;              // Root pointer
    size_t size;            // Size of root area
} gc_root;

// Initial root array size
#define GC_INITIAL_ROOTS 16

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
#else
    // Fallback to portable method
    #define GC_GET_STACK_POINTER(ptr) do { \
        volatile int x; \
        *(ptr) = (void*)&x; \
    } while(0)
#endif

// GC state
typedef struct gc_state {
    gc_entry *hash_table;       // Hash table of allocations (array)
    size_t hash_size;           // Current hash table size
    size_t entry_count;         // Number of entries in hash table
    size_t allocated_bytes;     // Total allocated memory
    size_t threshold;           // Collection threshold
    void *stack_bottom;         // Bottom of stack for scanning
    
    // Root management
    gc_root *roots;             // Array of registered roots
    size_t root_count;          // Number of registered roots
    size_t root_capacity;       // Capacity of roots array
} gc_state;

// Initialize GC with stack bottom
void gc_init(gc_state *gc, void *stack_bottom);

// Clean up GC
void gc_cleanup(gc_state *gc);

// Allocate memory with GC tracking
void* gc_malloc(gc_state *gc, size_t size);

// Reallocate memory with GC tracking
void* gc_realloc(gc_state *gc, void *ptr, size_t size);

// Force a garbage collection
void gc_collect(gc_state *gc);

// Get total allocated bytes
size_t gc_allocated_bytes(gc_state *gc);

// Add a root to be scanned during GC
void gc_add_root(gc_state *gc, void *ptr, size_t size);

// Remove a root from GC scanning
void gc_remove_root(gc_state *gc, void *ptr);

#endif // GC_H