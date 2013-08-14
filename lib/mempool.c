
#include "libimagequant.h"
#include "mempool.h"
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#define ALIGN_MASK 15UL
#define MEMPOOL_RESERVED ((sizeof(struct mempool_)+ALIGN_MASK) & ~ALIGN_MASK)

struct mempool_ {
    unsigned int used, size;
    void* (*malloc)(size_t);
    void (*free)(void*);
    struct mempool_ *next;
};
LIQ_PRIVATE void* mempool_create(mempool *mptr, const unsigned int size, unsigned int max_size, void* (*malloc)(size_t), void (*free)(void*))
{
    if (*mptr && ((*mptr)->used+size) <= (*mptr)->size) {
        unsigned int prevused = (*mptr)->used;
        (*mptr)->used += (size+15UL) & ~0xFUL;
        return ((char*)(*mptr)) + prevused;
    }

    mempool old = *mptr;
    if (!max_size) max_size = (1<<17);
    max_size = size+ALIGN_MASK > max_size ? size+ALIGN_MASK : max_size;

    *mptr = (mempool)malloc(MEMPOOL_RESERVED + max_size);
    if (!*mptr) return NULL;
    **mptr = (struct mempool_){
        .malloc = malloc,
        .free = free,
        .size = MEMPOOL_RESERVED + max_size,
        .used = sizeof(struct mempool_),
        .next = old,
    };
    uintptr_t mptr_used_start = (uintptr_t)(*mptr + (*mptr)->used);
    (*mptr)->used += (ALIGN_MASK + 1 - (mptr_used_start & ALIGN_MASK)) & ALIGN_MASK; // reserve bytes required to make subsequent allocations aligned
    assert(!((uintptr_t)(*mptr + (*mptr)->used) & ALIGN_MASK));

    return mempool_alloc(mptr, size, size);
}

LIQ_PRIVATE void* mempool_alloc(mempool *mptr, unsigned int size, unsigned int max_size)
{
    if (((*mptr)->used+size) <= (*mptr)->size) {
        unsigned int prevused = (*mptr)->used;
        (*mptr)->used += (size + ALIGN_MASK) & ~ALIGN_MASK;
        return ((char*)(*mptr)) + prevused;
    }

    return mempool_create(mptr, size, max_size, (*mptr)->malloc, (*mptr)->free);
}

LIQ_PRIVATE void mempool_destroy(mempool m)
{
    while (m) {
        mempool next = m->next;
        m->free(m);
        m = next;
    }
}
