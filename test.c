/**************************************************************************
*
* Copyright (C) 2017 Edward Hague
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
*********************************************************************/

// This is a lightweight test. Obviously I have tested more than this, but I have stripped
// this code down to the bare necessities. It is up to you to add features.

// Compiles using MSVC !! 32 bit !! quite happily. Linux untested at this point.

#include <stdio.h>
#include "emm.h"

#if 0
// change this to #if 1 to enable OS malloc/free for a comparison. I have measure OS malloc/free to be 4x slower on Windows10
#include <memory.h>
#define emm_malloc malloc
#define emm_free free
#endif

void main(void)
{
    emm_init(); 

    for (int i = 0; i < 10000000; i++)
    {
        uint8_t *memptr0 = emm_malloc(178);
        uint8_t *memptr1 = emm_malloc(178);
        uint8_t *memptr2 = emm_malloc(1);
        uint8_t *memptr3 = emm_malloc(17);
        uint8_t *memptr4 = emm_malloc(18);
        uint8_t *memptr5 = emm_malloc(18);
        uint8_t *memptr6 = emm_malloc(17);
        uint8_t *memptr7 = emm_malloc(178);
        uint8_t *memptr8 = emm_malloc(18);
        uint8_t *memptr9 = emm_malloc(17);

        emm_free(memptr1);
        emm_free(memptr3);
        emm_free(memptr5);
        emm_free(memptr7);
        emm_free(memptr9);

        memptr1 = emm_malloc(178);
        memptr3 = emm_malloc(178);
        memptr5 = emm_malloc(1);
        memptr7 = emm_malloc(17);
        memptr9 = emm_malloc(18);

        emm_free(memptr0);
        emm_free(memptr1);
        emm_free(memptr2);
        emm_free(memptr3);
        emm_free(memptr4);
        emm_free(memptr5);
        emm_free(memptr6);
        emm_free(memptr7);
        emm_free(memptr8);
        emm_free(memptr9);
    }
}
