// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2020-2022 Martin Whitaker.
//
// Partly derived from an extract of memtest86+ test.c:
//
// MemTest86+ V5 Specific code (GPL V2.0)
// By Samuel DEMEULEMEESTER, sdemeule@memtest.org
// http://www.canardpc.com - http://www.memtest.org
// Thanks to Passmark for calculate_chunk() and various comments !
// ----------------------------------------------------
// test.c - MemTest-86  Version 3.4
//
// Released under version 2 of the Gnu Public License.
// By Chris Brady

#include <stdint.h>
#include "string.h"

#include "cache.h"
#include "smp.h"

#include "barrier.h"

#include "config.h"
#include "display.h"

#include "test_helper.h"

//------------------------------------------------------------------------------
// Public Functions
//------------------------------------------------------------------------------

void calculate_chunk(testword_t **start, testword_t **end, int my_cpu, int segment, size_t chunk_align)
{
    if (my_cpu < 0) {
        my_cpu = 0;
    }

    // If we are only running 1 CPU then test the whole segment.
    if (num_active_cpus == 1) {
        *start = vm_map[segment].start;
        *end   = vm_map[segment].end;
    } else {
        uintptr_t segment_size = (vm_map[segment].end - vm_map[segment].start + 1) * sizeof(testword_t);
        uintptr_t chunk_size   = round_down(segment_size / num_active_cpus, chunk_align);

        // Calculate chunk boundaries.
        *start = (testword_t *)((uintptr_t)vm_map[segment].start + chunk_size * chunk_index[my_cpu]);
        *end   = (testword_t *)((uintptr_t)(*start) + chunk_size) - 1;

        if (*end > vm_map[segment].end) {
            *end = vm_map[segment].end;
        }
    }
}

void flush_caches(int my_cpu)
{
    if (my_cpu >= 0) {
        bool use_spin_wait = (power_save < POWER_SAVE_HIGH);
        if (use_spin_wait) {
            barrier_spin_wait(run_barrier);
        } else {
            barrier_halt_wait(run_barrier);
        }
        if (my_cpu == master_cpu) {
            cache_flush();
        }
        if (use_spin_wait) {
            barrier_spin_wait(run_barrier);
        } else {
            barrier_halt_wait(run_barrier);
        }
    }
}

void print_usb_mirror(const char *fmt, ...)
{
    static int print_row = 3;
    static int print_col = 0;
	if (print_row == SCREEN_HEIGHT - 4) {
		scroll_screen_region(3, 0, SCREEN_HEIGHT - 5, SCREEN_WIDTH - 1);
		print_row--;
	}

	va_list args;
    va_list args_cp;

	va_start(args, fmt);
    va_copy(args_cp, args);
	(void)vprintf_usb(print_row, print_col, fmt, args);
    va_end(args);

	(void)vprintf(print_row, print_col, fmt, args_cp);
    print_row++;
}

