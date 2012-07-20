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

#define THREADFUNC_CALLING_CONVENTION __stdcall
#define THREADFUNC_RETURN_TYPE unsigned long
#define DLL_EXPORT __declspec(dllexport)
#define PLATFORM_IS_LITTLE_ENDIAN
//#define PLATFORM_IS_BIG_ENDIAN

namespace Basics
{
    typedef signed char  Sint8 ;
    typedef signed short Sint16;
    typedef signed int   Sint32;
    typedef signed long long int Sint64;
    typedef Sint8  Uint8 ;
    typedef Sint16 Uint16;
    typedef Sint32 Uint32;
    typedef Sint64 Uint64;
    void message(const char *msg, ...);
    void error(const char *msg = 0, ...);
    #define THREADHEAP_POINTER_SIZE 4 // must be a macro
}
namespace BitwiseMath
{
    // a good compiler will optimize these down to a single opcode on most ISAs, assuming 'amount' is a constant
    using namespace Basics;
    static inline Uint64 bit_rotate(Uint64 value, int amount) { return (value << amount) | (value >> (64-amount)); }
    static inline Uint32 bit_rotate(Uint32 value, int amount) { return (value << amount) | (value >> (32-amount)); }
    static inline Uint16 bit_rotate(Uint16 value, int amount) { return (value << amount) | (value >> (16-amount)); }
    static inline Uint8  bit_rotate(Uint8  value, int amount) { return (value << amount) | (value >> (8 -amount)); }
//    unsigned char bit_scan_forward(Uint8 );
//    unsigned char bit_scan_forward(Uint16);
//    unsigned char bit_scan_forward(Uint32);
//    unsigned char bit_scan_forward(Uint64);
}

namespace GetTime
{
    long ms();          // time since initialization in milliseconds
    long long ticks();  // time since initialization in ticks
    long long seed();   // get arbitrary value for use as RNG seed

    double ticks_per_second();  // ticks per second (estimated)
    void initialize();          // call before using the other GetTime functions
}

namespace Threading
{
    void create_thread(THREADFUNC_RETURN_TYPE (THREADFUNC_CALLING_CONVENTION *threadfunc)(void*), void *param);
    void sleep(int milliseconds);

    class Lock
    {
        // implementation details hidden (by ugly methods)
        //    in order to avoid platform-specific include files here
        enum {MAX_IMPL_SIZE = 64}; // make sure this is large enough to hold whatever the real implementation is
        union
        {
            char impl_data[MAX_IMPL_SIZE];
            long long aligned_data;  // to force a reasonable alignment
        };
    public:
        Lock();
        ~Lock();

        void enter();
        void leave();
        bool try_enter(); // true on success
    };
    extern Lock general_lock;
}
