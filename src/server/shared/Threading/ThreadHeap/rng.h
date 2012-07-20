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

namespace RNGs
{
    class RNG_fast32_8
    {
        // low quality but very fast
        // for use inside the benchmarks, so that random size choices don't slow them down
        typedef Basics::Uint32 InternalWord;
        typedef Basics::Uint8 OutputWord;

        enum {LENGTH = 11};
        union
        {
            InternalWord cbuf[LENGTH];
            OutputWord results[LENGTH * sizeof(InternalWord)/sizeof(OutputWord)];
        };

        InternalWord output_words_left;
        OutputWord refill();
    public:
        inline OutputWord raw8()
        {
            if (output_words_left) return results[--output_words_left];
            else return refill();
        }

        Basics::Uint16 raw16() { Basics::Uint16 rv = raw8 (); rv<<= 8; rv += raw8 (); return rv; }
        Basics::Uint32 raw32() { Basics::Uint32 rv = raw16(); rv<<=16; rv += raw16(); return rv; }
        Basics::Uint64 raw64() { Basics::Uint64 rv = raw32(); rv<<=32; rv += raw32(); return rv; }

        void seed_64(Basics::Uint64 s);
        void seed_32(Basics::Uint32 s) { seed_64(s); }
    };

    class RNG_jsf32
    {
        //    Robert Jenkins small fast prng
        //    reasonably small, reasonably fast, reasonably high quality output
        //    There are some bad cycles, but the odds of hitting them are
        //        low, believed to be under 1 in 2^60
        Basics::Uint32 a, b, c, d;
    public:
        Basics::Uint32 raw32();
        Basics::Uint64 raw64() { Basics::Uint64 rv = raw32(); rv<<=32; rv += raw32(); return rv; }
        void seed_32(Basics::Uint32 s);
        void seed_64(Basics::Uint64 s);
    };
    extern RNG_jsf32 global_rng;
}
