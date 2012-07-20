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
#include "rng.h"
#include <cstdio>

using namespace BitwiseMath;
namespace RNGs
{
    RNG_fast32_8::OutputWord RNG_fast32_8::refill()
    {
        enum {LAG = 5, C_LAG = LENGTH - LAG, SHIFT = 9};
        long i;

        for (i = 0; i < LAG; i++)
        {
            InternalWord lagged = cbuf[i+C_LAG];
            cbuf[i] += bit_rotate(lagged, SHIFT);
        }
        for (; i < LENGTH; i++)
        {
            InternalWord lagged = cbuf[i-  LAG];
            cbuf[i] += bit_rotate(lagged, SHIFT);
        }
        output_words_left = LENGTH * sizeof(InternalWord) / sizeof(OutputWord);
        return results[--output_words_left];
    }

    void RNG_fast32_8::seed_64(Uint64 s)
    {
        RNG_jsf32 seeder;
        seeder.seed_64(s);
        for (int i = 0; i < LENGTH; i++) cbuf[i] = InternalWord(seeder.raw64());
        refill();
    }

    Uint32 RNG_jsf32::raw32()
    {
        Uint32 e = a - bit_rotate(b, 27);
        a = b ^ bit_rotate(c, 17);
        b = c + d;
        c = d + e;
        d = e + a;
        return d;
    }

    void RNG_jsf32::seed_32(Basics::Uint32 s)
    {
        a = s;
        b = c = d = 1;
        for (int i = 0; i < 10; i++) raw32();
    }

    void RNG_jsf32::seed_64(Basics::Uint64 s)
    {
        a = Uint32(s);
        b = Uint32(s >> 32);
        c = d = 1;
        for (int i = 0; i < 12; i++) raw32();
    }
    RNG_jsf32 global_rng;
}
