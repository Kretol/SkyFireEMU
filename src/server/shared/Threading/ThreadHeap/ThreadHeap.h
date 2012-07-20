/*
 * Copyright (C) 2011-2012 Project SkyFire <http://www.projectskyfire.org/>
 * Copyright (C) 2008-2012 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2012 MaNGOS <http://getmangos.com/>
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

/*
 * In order:
 * 1. [optional] call set_fallback_heap
 * 2. [optional] call initialize
 * 3. call allocate, de-allocate, re-allocate, etc as desired
 * 4. [optional] call de-initialize
 * 5. [optional] repeat from step 1
 */

extern "C"
{
    DLL_EXPORT void *allocate(size_t size);                     // malloc-equivalent
    DLL_EXPORT void deallocate(void *mem);                      // free-equivalent
    DLL_EXPORT void *reallocate(void *oldmem, size_t newsize);  // realloc-equivalent

    // malloc-equivalent, always aligned to a cache-line boundary
    DLL_EXPORT void *allocate_cacheline_aligned(size_t size);

    // memsize returns the usable size of an allocation, which is always at
    //   least as large as the requested size
    DLL_EXPORT size_t memsize(void *mem);



    /*
     * sets the fallback heap...
     *  allocate - the fallback allocate will be used if the requested allocation
     *     is too large for this heap or if it otherwise would not fit in this heap
     *     default is malloc
     *  deallocate - the fallback deallocate is used on if the address being deallocated
     *    does not lie in this heap
     *    default is free
     *  reallocate - the fallback reallocate may be used if the address being reallocated
     *    does not lie in this heap
     *    default is realloc
     *  memsize - the fallback memsize is used if the address that memsize is called upon
     *    does not lie in this heap
     *    default is NULL
     *  page_aligned - normally false, but if allocate and reallocate are guaranteed to
     *    always return page-aligned blocks of memory when size is >= the page size, then
     *    true.
     */

    DLL_EXPORT void set_fallback_heap(    // NOT thread-safe
        void* (*allocate)(size_t),
        void  (*deallocate)(void*),
        void  (*reallocate)(void*, size_t),
        size_t(*memsize)(void*),
        bool page_aligned = false
    );

    // initialize the heap for use, set its size to heapsize bytes
    DLL_EXPORT void initialize();  // NOT thread-safe

    // shutdown the heap, freeing all memory used by the heap
    DLL_EXPORT void deinitialize(); // NOT thread-safe

    // true if mem points at an address inside the heap area,
    //   NOT counting the fallback heap, false otherwise
//    DLL_EXPORT bool is_address_in_heap_memory(void *mem);

    // returns the total number of bytes reserved for allocations within the specified range,
    //   NOT counting the fallback heap
//    DLL_EXPORT size_t get_bytes_allocated(size_t minsize, size_t maxsize);
}
