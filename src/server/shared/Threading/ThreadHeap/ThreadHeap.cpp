/*
 * Copyright (C) 2011-2012 Project SkyFire <http://www.projectskyfire.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "platform_common.h"
#include "platform_heap.h"
#include "ThreadHeap.h"

#include <cstdlib>
#include <new>
#include <vector>

#define GENERIC_ALLOCATE(container) \
    if ((container).num_nodes) {\
        (container).num_nodes--;\
        Node *old_head = (container).free_nodes;\
        (container).free_nodes = old_head->next;\
        return old_head;\
    }
#define GENERIC_DEALLOCATE(container, node, si) \
    int marker_index = GET_BUNCH_MARKER_INDEX((container).num_nodes, (si));\
    (container).num_nodes++;\
    (node)->next = (container).free_nodes;\
    (container).free_nodes = node;\
    if (marker_index != 2) (container).bunch_markers[marker_index] = (node);


//#define CHECK_ALIGNMENT
//#define FASTBLOCKS

using namespace Basics;
using namespace AddressSpaceManagement;

void*(*wrapped_allocate)(size_t) = malloc;
void(*wrapped_deallocate)(void*) = free;

void *fallback_wrapper_allocate(size_t size)
{
    void *base = wrapped_allocate(size + (PAGE_SIZE + sizeof(void*)*2-1));
    if (!base) error("out of memory");

    AddressInteger addr = (AddressInteger)base;
    AddressInteger rv = (addr + (PAGE_SIZE + sizeof(void*)*2 - 1)) & (AddressInteger(0) - PAGE_SIZE);

    ((void**)rv)[-1] = base;
    ((void**)rv)[-2] = (void*)size;
    return (void*)rv;
//    return VirtualAlloc( NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

void fallback_wrapper_deallocate(void *mem)
{
    if (!mem)return;
    void *mem2 = ((void**)mem)[-1];
    wrapped_deallocate( mem2 );
 //    VirtualFree( mem, 0, MEM_RELEASE );
}
size_t fallback_wrapper_memsize(void *mem)
{
    return (size_t) (((void**)mem)[-2]);

/*  MEMORY_BASIC_INFORMATION info;
    if (VirtualQuery(mem, &info, sizeof(info)))
    {
        return info.RegionSize;
    }
    error("ThreadHeap4 - _fallback_memsize - VirtualQuery failed");
    return 0; //
*/
}
void*(*fallback_allocate)(size_t) = fallback_wrapper_allocate;
void(*fallback_deallocate)(void*) = fallback_wrapper_deallocate;
void*(*fallback_reallocate)(void*, size_t) = NULL;
size_t (*fallback_memsize)(void*) = fallback_wrapper_memsize;

DLL_EXPORT void set_fallback_heap(
    void* (*allocate)(size_t),
    void  (*deallocate)(void*),
    void* (*reallocate)(void*, size_t),
    size_t(*memsize)(void*),
    bool page_aligned)
{
    if (!memsize || !page_aligned)
    {
        fallback_allocate   = fallback_wrapper_allocate;
        fallback_deallocate = fallback_wrapper_deallocate;
        fallback_reallocate = NULL;
        fallback_memsize    = fallback_wrapper_memsize;
        wrapped_allocate    = allocate;
        wrapped_deallocate  = deallocate;
    }
    else
    {
        fallback_allocate   = allocate;
        fallback_deallocate = deallocate;
        fallback_reallocate = reallocate;
        fallback_memsize    = memsize;
    }
}


bool initialized = false;

/*
 * 4th per-thread heap, beginning to generalize
 * order name            Per-Thread  Per-Bin    Per-Block    other
 *   1. list             yes         yes     no          with bunch markers
 *   2. shared-list      no          yes     no          CS locking
 *   3. high-lists       no          yes     yes         user-settable, defaults to system malloc/free
 *   4. blocks           no          no      no          interlocked, never freed to
 *   5. system           no          no      no          ???
 * in roughly the order that allocate() tries them in... if there is something
 *   available from #1 it doesn't check #2 etc
 * deallocate() tries them in roughly the same order
 */
    // setting - size unit (minimum granularity of bin sizes)
    // should probably be 2 or 3
enum {SIZE_UNIT_SHIFT = 3};
    // setting - bin sizes
    // must be ordered, smallest must be >= sizeof(void*)*2, must be multiples of size_unit
#if THREADHEAP_POINTER_SIZE == 4
const size_t sizes[] = {8, 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768, 1024, 1536, 2048, 3072, 4096, 6144};
enum { MAX_SIZE = 6144 };
#elif THREADHEAP_POINTER_SIZE == 8
const size_t sizes[] = {16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768, 1024, 1536, 2048, 3072, 4096, 6144};
enum { MAX_SIZE = 6144 };
#else
#error THREADHEAP_POINTER_SIZE is neither 4 nor 8 bytes
#endif
//const int sizes[] = {8, 16, 32, 64, 128, 256, 512, 1024, 2048};enum {max_size = 2048};
//const int sizes[] = {8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 448, 512, 640, 768, 896, 1024, 1280, 1536, 1792, 2048};enum {max_size = 2048};
//const int sizes[] = {8,16,24,32,40,48,56,64,80,96,112,128,160,192,224,256,320,384,448,512,640,768,896,1024,1280,1536,1792,2048,2560,3072,3584};enum {max_size = 3584};

// setting - block size
// min log2(max_size)+1
enum {BLOCK_SIZE_SHIFT = 17};
enum {BLOCK_SIZE = 1 << BLOCK_SIZE_SHIFT};
enum {PAGES_PER_BLOCK = BLOCK_SIZE / PAGE_SIZE};

// setting - maximum nodes per bunch
// min 1, should be a power of 2
enum {MAX_BUNCH_COUNT = 128};

// setting - maximum bytes per bunch
// should be <= block_size, should be >= max_size
enum {MAX_BUNCH_SIZE = 1 << 13};

// basics:
enum {NUM_SIZES = sizeof(sizes) / sizeof(int)};

//enum {MAX_SIZE = sizes[NUM_SIZES-1]};
enum {SIZE_UNIT = 1 << SIZE_UNIT_SHIFT};

int size_lookup[MAX_SIZE >> SIZE_UNIT_SHIFT];

struct MegaBlockData;
struct BlockHeader;
struct Block;
struct Node
{
    Node *next; // any allocation unit
};

struct Bunch : public Node
{
    Bunch *next_bunch; // a set of allocation units
};

/*
struct BigBunch : public Bunch
{
    BigBunch *next_big_bunch; //
};
*/

inline int size_to_si(size_t size) { return size_lookup[(size + (SIZE_UNIT - 1)) >> SIZE_UNIT_SHIFT]; }
// prefetch - experimental, doesn't seem to help?
inline void prefetch_block(int block) {}
// inline void prefetch_node (void *node) { __asm PREFETCH node }
inline void prefetch_node (void *node) {}

// int bunch_size[NUM_SIZES];
//char bunch_count_index[NUM_SIZES][MAX_BUNCH_COUNT*2]; // this ought to be MAX_BUNCH_COUNT*2+1, rounded up to a power of 2, but that seems more complicated than I want
int bunch_count_shift[NUM_SIZES];
// int bunch_count[NUM_SIZES];
#define GET_BUNCH_MARKER_INDEX(n, si) ((n) >> bunch_count_shift[si])
//#define GET_BUNCH_MARKER_INDEX(n, si) (bunch_count_index[si][(n)])
#define GET_BUNCH_COUNT(si) (1 << bunch_count_shift[si])
//#define GET_BUNCH_SIZE(si) (bunch_count[si])


void add_megablock();

//void remove_megablock(MegaBlockData *data);
void allocate_block(int si);
//void deallocate_block(Block *block);
Node *allocate_node(int si);
void deallocate_node(int si, Node *);
Bunch *allocate_bunch(int si);
void deallocate_bunch(int si, Bunch *);
//Bunch *allocate_big_bunch(int si);
//void deallocate_big_bunch(int si, BigBunch *);
void *allocate_noPT(size_t size);
void deallocate_noPT(void *mem);

// 5. system level:
enum {MAX_BLOCKS_PER_MEGABLOCK = 512};
enum {MIN_BLOCKS_PER_MEGABLOCK = 32};
enum {STEP_BLOCKS_PER_MEGABLOCK = 32};

struct MegaBlockData // should fit in (BLOCK_SIZE - PAGE_SIZE) / 2
{
    static MegaBlockData *lowest;
    static size_t num_megablocks;
//    static size_t total_blocks;
//    Threading::Lock lock;

    void *actual_start;
    Block *aligned_start;
    MegaBlockData *next, *prev;
    unsigned long num_blocks;
    volatile unsigned long unused_blocks;

    struct BlockData
    {
        char size_index;
    };

    BlockData block[MAX_BLOCKS_PER_MEGABLOCK];
    MegaBlockData(size_t num_blocks_, void *actual_start_, void *aligned_start_) : num_blocks(num_blocks_), actual_start(actual_start_), aligned_start((Block*)aligned_start_), unused_blocks(num_blocks)
    {
        for (unsigned int i = 0; i < num_blocks; i++)
            block[i].size_index = -1;

        MegaBlockData *before = NULL;
        MegaBlockData *after = MegaBlockData::lowest;

        while (after && aligned_start > after->aligned_start)
        {
            before = after;
            after  = after->next;
        }
        next = after;
        prev = before;

        if (!before) MegaBlockData::lowest = this;
        else before->next = this;
        if (after) after->prev = this;

        MegaBlockData::num_megablocks++;
//        MegaBlockData::total_blocks += num_blocks;
    }
};

MegaBlockData *MegaBlockData::lowest = NULL;
size_t MegaBlockData::num_megablocks = 0;
//size_t MegaBlockData::total_blocks = 0;

struct BlockHeader
{
    char size_index;
    char padding[7];
    MegaBlockData *data;

    // for high-address-deallocation-lists
    Threading::Lock lock;
    Node *free_nodes;
    Node *bunch_markers[2];
    Bunch *free_bunches;
    int num_nodes;
    int num_bunches;
};
struct Block : public BlockHeader
{
    char _data[BLOCK_SIZE - sizeof(BlockHeader)];
};
Threading::Lock megablock_list_lock;

void add_megablock()
{
//    megablock_list_lock.enter();
    size_t num_blocks = MIN_BLOCKS_PER_MEGABLOCK + MegaBlockData::num_megablocks * STEP_BLOCKS_PER_MEGABLOCK;

    if (num_blocks > MAX_BLOCKS_PER_MEGABLOCK) num_blocks = MAX_BLOCKS_PER_MEGABLOCK;

    void *new_megablock = allocate_page_block( PAGES_PER_BLOCK * num_blocks + (PAGES_PER_BLOCK-1) );

    if (!new_megablock)
    {
        while (num_blocks != MIN_BLOCKS_PER_MEGABLOCK)
        {
            message("ThreadHeap4 - add_megablock - unable to allocate new megablock with %d blocks (%d pages)", num_blocks, PAGES_PER_BLOCK * num_blocks);
            num_blocks = (MIN_BLOCKS_PER_MEGABLOCK + num_blocks) >> 1;
            new_megablock = allocate_page_block(PAGES_PER_BLOCK * num_blocks);

            if (new_megablock)
            break;
        }
        
        if (!new_megablock)
        {
            error("ThreadHeap4 - add_megablock - unable to allocate new megablock of any acceptable size");
        }
    }

    typedef AddressInteger Address;
    Address addr = (Address)new_megablock;

    size_t alignment = addr & (BLOCK_SIZE - 1);
    size_t extra_at_start = alignment ? BLOCK_SIZE - alignment : 0;
    size_t extra_at_end = alignment ? alignment - PAGE_SIZE : BLOCK_SIZE - PAGE_SIZE;
    BlockHeader *main_megablock = (BlockHeader *) (addr + extra_at_start);
    void *data;

    if (extra_at_start >= extra_at_end) data = (void*)new_megablock;
    else
        data = (void*)(addr + extra_at_start + (num_blocks << BLOCK_SIZE_SHIFT));

    new (data) MegaBlockData(num_blocks, new_megablock, main_megablock);

//    megablock_list_lock.leave();
}

void allocate_block(int si)
{
    megablock_list_lock.enter();
    MegaBlockData *mb = MegaBlockData::lowest;
    while (mb)
    {
        if (mb->unused_blocks > 0)
        break;

        mb = mb->next;
    }
    if (!mb)
    {
//        megablock_list_lock.enter();
        add_megablock();
//        megablock_list_lock.leave();
        megablock_list_lock.leave();
        allocate_block(si);
        return;
    }

    unsigned int bi;
    for (bi = 0; bi < mb->num_blocks; bi++)
    {
        if (mb->block[bi].size_index == -1)
        break;
    }

    mb->block[bi].size_index = si;
    mb->unused_blocks--;
    Block *block = &mb->aligned_start[bi];
    block->data = mb;
    block->size_index = si;

    int size = sizes[si];
    int offset = sizeof(BlockHeader);

    if (!(size &  7)) offset = (offset +  7) & (0xFFffFFff -  7);
    if (!(size & 15)) offset = (offset + 15) & (0xFFffFFff - 15);
    if (!(size & 63)) offset = (offset + 63) & (0xFFffFFff - 63);
    int end = sizeof(Block) - size;

    for (; offset <= end; offset += size)
    {
        Node *node = (Node*)(((AddressInteger)block)+offset);
        if (offset & (PAGE_SIZE-1)) deallocate_node(si, node);
        //if (offset & (BLOCK_SIZE-1)) deallocate_node(node, si);
    }

    megablock_list_lock.leave();
}

// favor low addresses
void *const MAX_ADDRESS = (void*)(AddressInteger(0) - 1);
void *threshold_address[NUM_SIZES];
inline bool is_address_high(int si, void *ptr)
{
    return ptr < threshold_address[si];
}

void deallocate_to_high_path(Node *node)
{
    Block *block = (Block*)(((AddressInteger)node) & (AddressInteger(0) - BLOCK_SIZE));
    block->lock.enter();

    int si = block->size_index;
    GENERIC_DEALLOCATE(*block, node, si)
    else
    {
        block->num_nodes -= GET_BUNCH_COUNT(si);
        block->num_bunches++;
        Bunch *bunch = (Bunch*)node;
        bunch->next_bunch = block->free_bunches;
        block->free_bunches = bunch;
    }
    block->lock.leave();
}

void update_threshold_for_bunches(int si, int bunches) {}
void update_threshold_for_block(int si, Block *block, bool add) {}
void flush_high_path(Block *block) {}

/*
struct BunchBin
{
    Threading::Lock lock;
    int num_bunches;
    Bunch *free_bunches;
};
*/

struct Bin
{
    Threading::Lock free_node_lock;
    Node *bunch_markers[2];
    Node *free_nodes;

    int num_nodes;
    char padding1[48];

    Threading::Lock free_bunch_lock;
    Bunch *free_bunches;

    int num_bunches;
    char padding2[56];

    Bin()
    {
        bunch_markers[0]    = NULL;
        bunch_markers[1]    = NULL;
        free_nodes          = NULL;
        num_nodes           = 0;
        free_bunches        = NULL;
        num_bunches         = 0;
    }
};

Bin bins[NUM_SIZES];
Node *allocate_node(int si)
{
    Bin &bin = bins[si];
/*
    if (bin.num_nodes)
    {
        bin.num_nodes--;
        Node *old_head = bin.free_nodes;
        bin.free_nodes = old_head->next;
        return old_head;
    }
*/
    GENERIC_ALLOCATE(bin)
    if (bin.num_bunches)
    {
        bin.num_bunches--;
        Bunch *old_head     = bin.free_bunches;
        bin.free_bunches    = old_head->next_bunch;
        bin.free_nodes      = old_head->next;
        //bin.bunch_markers[0] = old_head->next;
        bin.num_nodes = GET_BUNCH_COUNT(si) - 1;
        return old_head;
    }
    allocate_block(si);
    return allocate_node(si);
}

void deallocate_node(int si, Node *node)
{
    Bin &bin = bins[si];
/*
    int marker_index = GET_BUNCH_MARKER_INDEX(bin.num_nodes, si);
    bin.num_nodes++;
    node->next = bin.free_nodes;
    bin.free_nodes = node;
    if (marker_index != 2) bin.bunch_markers[marker_index] = node;
*/
    GENERIC_DEALLOCATE(bin, node, si)
    else
    {
        bin.num_nodes -= GET_BUNCH_COUNT(si);
        Bunch *bunch = (Bunch*)bin.bunch_markers[0];
        bin.bunch_markers[0] = bin.bunch_markers[1];
        bin.bunch_markers[1] = node;
        bin.free_bunch_lock.enter();
        bunch->next_bunch = bin.free_bunches;
        bin.free_bunches  = bunch;
        bin.num_bunches ++;
        bin.free_bunch_lock.leave();
    }
}

Bunch *allocate_bunch(int si)
{
    Bin &bin = bins[si];
    if (!bin.num_bunches)
    {
        allocate_block(si);
        return allocate_bunch(si);
    }

    Bunch *old_head = bin.free_bunches;
    bin.free_bunches = old_head->next_bunch;
    bin.num_bunches--;
    return old_head;
}

void deallocate_bunch(int si, Bunch *bunch)
{
    Bin &bin = bins[si];
    bunch->next_bunch = bin.free_bunches;
    bin.free_bunches  = bunch;
    bin.num_bunches++;
}

struct PerThread
{
    struct PerThreadPerBin
    {
        int num_nodes;
        Node *free_nodes;
        Node *bunch_markers[2];

        PerThreadPerBin()
        {
            num_nodes         = 0;
//          free_nodes        = NULL;
//          bunch_markers[0]  = NULL;
//          bunch_markers[1]  = NULL;
        }
    };

    struct PerThreadPerBinPadded : PerThreadPerBin
    {
        char padding [(CACHE_LINE_SIZE - sizeof(PerThreadPerBin)) & (CACHE_LINE_SIZE-1)];
    };

    PerThreadPerBinPadded ptpb[NUM_SIZES];
    ~PerThread()
    {
        for (int si = 0; si < NUM_SIZES; si++)
        {
            PerThreadPerBin &tb = ptpb[si];
            int bc = GET_BUNCH_COUNT(si);
            while (tb.num_nodes >= bc)
            {
                tb.num_nodes -= bc;
                bins[si].free_bunch_lock.enter();
                deallocate_bunch(si, (Bunch*)tb.bunch_markers[0]);
                bins[si].free_bunch_lock.leave();
                tb.bunch_markers[0] = tb.bunch_markers[1];
            }

            if (tb.num_nodes)
            {
                bins[si].free_node_lock.enter();
                for (int i = 0; i < tb.num_nodes; i++)
                {
                    Node *node = tb.free_nodes;
                    tb.free_nodes = node->next;
                    deallocate_node(si, node);
                }
                bins[si].free_node_lock.leave();
            }
        }
    }
    Node *allocate(int si)
    {
        PerThreadPerBin &tb = ptpb[si];
/*
        if (tb.num_nodes)
        {
            tb.num_nodes--;
            Node *old_head = tb.free_nodes;
            tb.free_nodes  = old_head->next;
            return old_head;
        } //
*/
        GENERIC_ALLOCATE(tb)
        else
        {
            bins[si].free_bunch_lock.enter();
            Node *rv = allocate_bunch(si);
            bins[si].free_bunch_lock.leave();

            if (!rv) error();
            //return fallback_allocate(sizes[si]);
            tb.free_nodes = rv->next;
            tb.num_nodes  = GET_BUNCH_COUNT(si) - 1;
            return rv;
        }
    }
    void deallocate(int si, Node *node)
    {
        PerThreadPerBin &tb = ptpb[si];
/*
        int marker_index = GET_BUNCH_MARKER_INDEX(tb.num_nodes, si);
        tb.num_nodes++;
        node->next    = tb.free_nodes;
        tb.free_nodes = node;
        if (marker_index != 2) tb.bunch_markers[marker_index] = node; //
*/
        GENERIC_DEALLOCATE(tb, node, si)
        else
        {
            tb.num_nodes -= GET_BUNCH_COUNT(si);
            Bunch *bunch = (Bunch*)tb.bunch_markers[0];
            tb.bunch_markers[0] = tb.bunch_markers[1];
            tb.bunch_markers[1] = node;
            bins[si].free_bunch_lock.enter();
            deallocate_bunch(si, bunch);
            bins[si].free_bunch_lock.leave();
        }
    }
};
PerThread *get_PT()
{
    PerThread *rv = (PerThread*) Threading::get_TLS_pointer();
    if (rv)
    return rv;

//    message("ThreadHeap4 - no PerThread for this thread, creating");
    if (!initialized) initialize();

    rv = new (allocate_noPT(sizeof(PerThread))) PerThread();
//    threads_list_lock.enter();
//    PerThread *old_head = PerThread::head;
//    PerThread::head = rv;

//  rv->prev = NULL;
//    rv->next = old_head;

//    if (old_head) old_head->prev = rv;
//    threads_list_lock.leave();
    Threading::set_TLS_pointer(rv);
//    ticks = GetTime::ticks() - ticks;

//    if (ticks > 29999) std::printf("\nTH4 thread creation - %6.0f ns\n", ticks * 1000000000.0 / GetTime::ticks_per_second());
    return rv;
}

void thread_destruction_callback(void *tls_pointer)
{
    PerThread *pt = (PerThread*) tls_pointer;
    if (!pt)
    return;
//    threads_list_lock.enter();

//    if (pt->next) pt->next->prev = pt->prev;
//    if (pt->prev) pt->prev->next = pt->next;
//    else PerThread::head = pt->next;

//    __int64 ticks = GetTime::ticks();
    pt->~PerThread();
//    ::free(pt);
    deallocate_noPT(pt);
//    ticks = GetTime::ticks() - ticks;
//    if (ticks > 29999) std::printf("\nTH4 thread destruction - %16.0f ns\n", ticks * 1000000000.0 / GetTime::ticks_per_second());
//    fallback_deallocate(pt);
//    threads_list_lock.leave();
}

void *allocate_cacheline_aligned(size_t size)
{
    return allocate((size + (CACHE_LINE_SIZE - 1)) & (size_t(0) - CACHE_LINE_SIZE));
}

void *allocate_noPT(size_t size)
{
    if (size > MAX_SIZE) return fallback_allocate((size + (PAGE_SIZE - 1)) & (AddressInteger(0) - (PAGE_SIZE)));
    int si = size_to_si(size);
    bins[si].free_node_lock.enter();
    void *rv = allocate_node(si);
    bins[si].free_node_lock.leave();
    return rv;
}

void deallocate_noPT(void *mem)
{
    AddressInteger addr = (AddressInteger)mem;
    if (addr & (PAGE_SIZE - 1)) // small block
    {
        addr &= ((AddressInteger)0) - BLOCK_SIZE;
        Block *block = (Block*) addr;
        int si = block->size_index;
        bins[si].free_node_lock.enter();
        deallocate_node(si, (Node*)mem);
        bins[si].free_node_lock.leave();
    }
    else // large block?
    {
        if (!addr)
        return;

        fallback_deallocate(mem);
    }
}

void *allocate(size_t size)
{
    if (size > MAX_SIZE) return fallback_allocate((size + (PAGE_SIZE - 1)) & (AddressInteger(0) - PAGE_SIZE));
    int si = size_to_si(size);
    PerThread *pt = get_PT();
    return pt->allocate(si);
}

void deallocate(void *mem)
{
    AddressInteger addr = (AddressInteger)mem;
    if (addr & (PAGE_SIZE - 1)) // small block
    {
        PerThread *pt = get_PT();
        Block *block = (Block*)(addr & (AddressInteger(0) - BLOCK_SIZE));
        pt->deallocate(block->size_index, (Node*)mem);
    }
    else // large block?
    {
        if (!addr)
        return;

        fallback_deallocate(mem);
    }
}

size_t memsize(void *mem)
{
    AddressInteger addr = (AddressInteger)mem;
    if (addr & (PAGE_SIZE - 1)) // small block
    {
        addr &= ((AddressInteger)0) - (BLOCK_SIZE);
        Block *block = (Block*) addr;
        int si = block->size_index;
        return sizes[si];
    }
    else // large block?
    {
        if (fallback_memsize) return fallback_memsize(mem);
        error("ThreadHeap4 - memsize - fallback_memsize not available");
        return 0;
    }
}

void *reallocate(void *oldmem, size_t newsize)
{
    if (!oldmem) return allocate(newsize);
    if (!newsize)
    {
        deallocate(oldmem);
        return NULL;
    }

    AddressInteger addr = (AddressInteger)oldmem;
    size_t oldsize      = memsize(oldmem);
    void *newmem        = allocate(newsize);

    memcpy(newmem, oldmem, oldsize > newsize ? newsize : oldsize);
    deallocate(oldmem);
    return newmem;
}

void initialize()
{
    Threading::general_lock.enter();
    if (initialized)
    {
        Threading::general_lock.leave();
        return;
    }

    Threading::set_TLS_destruction_callback(thread_destruction_callback);
    for (int i = 0; i < NUM_SIZES; i++)
    if (sizes[i] % SIZE_UNIT)
    {
        error("ThreadHeap4::initialize() - invalid size %d, terminating", sizes[i]);
    }

    for (int i = 0; i <= (MAX_SIZE / SIZE_UNIT); i++)
    {
        size_t s = i * SIZE_UNIT;
        int x = -1;

        for (int j = 0; j < NUM_SIZES; j++) if (sizes[j] >= s) { x = j; break; }
        size_lookup[i] = x;
    }

    for (int i = CACHE_LINE_SIZE; i < MAX_SIZE; i += CACHE_LINE_SIZE)
    {
        int si = size_to_si(i);
        if (sizes[si] % CACHE_LINE_SIZE)
        {
            error("ThreadHeap4::initialize() - size index %d invalid (missing CACHE_LINE_SIZE multiple)", si);
        }
    }

    for (int si = 0; si < NUM_SIZES; si++)
    {
        int k = MAX_BUNCH_SIZE / sizes[si];
        if (k > MAX_BUNCH_COUNT) k = MAX_BUNCH_COUNT;
        int b;
        for (b = 0; b < 25; b++)
        {
            if ((2 << b) > k)
            {
                k = 1 << b;
                break;
            }
        }

//      if (k < 1) k = 1;
        if (b >= 16) error("ThreadHeap4::initialize - bunch_count calculations failed");
        bunch_count_shift[si] = b;
//        bunch_count[si] = 1 << b;
//        bunch_size[si] = k * sizes[si];
    }

    SharedLibrary::set_process_detach_callback(deinitialize);
    initialized = true;
    Threading::general_lock.leave();
}

void deinitialize()
{
    Threading::general_lock.enter();
    MegaBlockData *data   = MegaBlockData::lowest;
    MegaBlockData::lowest = NULL;
    while (data)
    {
        MegaBlockData *next = data->next;
        AddressSpaceManagement::deallocate_page_block(data->actual_start, PAGES_PER_BLOCK * data->num_blocks + (PAGES_PER_BLOCK-1));
        data = next;
    }
    initialized = false;
    Threading::general_lock.leave();
}
