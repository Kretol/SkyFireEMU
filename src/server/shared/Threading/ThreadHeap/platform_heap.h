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

namespace SharedLibrary
{
    void set_process_detach_callback(void (*callback)());
}
namespace Threading
{
    void set_TLS_destruction_callback(void(*callback)(void*));
    void *get_TLS_pointer();
    void set_TLS_pointer(void *);
    void reset_TLS();  // forget all TLS pointers, release TLS handle

    namespace AtomicOperations
    {
        unsigned long increment(volatile unsigned long *target);
        unsigned long compare_exchange(volatile unsigned long *target, unsigned long new_value, unsigned long old_value);
        void *compare_exchange_pointer(void *volatile *target, void*new_value, void*old_value);
    }
    void memory_barrier();
}
namespace AddressSpaceManagement
{
    enum {ADDRESS_BITS = 32};
    enum {PAGE_SIZE_LOG2 = 12, PAGE_SIZE = 1 << PAGE_SIZE_LOG2};
    enum {CACHE_LINE_SIZE = 64};
    typedef Basics::Uint32 AddressInteger;

    // reserves address space and (if enabled is true) RAM
    // on win32 this is VirtualAlloc w/ MEM_RESERVE and (if enabled if true) MEM_COMMIT
    // on *nix it should probably be mmap w/ MAP_ANONYMOUS and (if enabled) MAP_NORESERVE, PROT_NONE
    void *allocate_page_block(size_t num_pages, /*size_t alignment = 1,*/ bool enabled = true);

    // frees address space (and likely RAM); note that start & num_pages must match
    //    exactly the return value of a allocate_page_block call and its 1st parameter
    // on win32 this is VirtualFree w/ MEM_RELEASE
    // on *nix it should probably be munmap
    void deallocate_page_block(void *start, size_t num_pages);

    // causes some pages to no longer have any storage backing them, but still be reserved
    // pages should lie within a region returned by allocate_page_block
    // should only be called on enabled pages
    // on win32 this is VirtualFree w/ MEM_DECOMMIT
    // on *nix it should probably be mmap w/ PROT_NONE & MAP_NORESERVE
    void disable_pages(void *start, size_t num_pages);

    // causes some pages to become usable
    // pages should lie within a region returned by allocate_page_block
    // should only be called on disabled pages
    // on win32 this is VirtualAlloc w/ MEM_COMMIT
    // on *nix it should probably be mmap w/ PROT_READ|PROT_WRITE
    bool enable_pages(void *start, size_t num_pages);   // true on success

    // notifies the OS that it can get rid of backing storage for these pages until the next write
    // on win32 this is VirtualAlloc w/ MEM_RESET
    // on *nix I think there's no way to implement, but it can be safely ignored
    void reset_pages(void *start, size_t num_pages);
}
