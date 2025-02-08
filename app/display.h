// SPDX-License-Identifier: GPL-2.0
#ifndef DISPLAY_H
#define DISPLAY_H
/**
 * \file
 *
 * Provides (macro) functions that implement the UI display.
 * All knowledge about the display layout is encapsulated here.
 *
 *//*
 * Copyright (C) 2020-2022 Martin Whitaker.
 * Copyright (C) 2004-2022 Sam Demeulemeester.
 */

#include <stdbool.h>

#include "print.h"
#include "screen.h"
#include "string.h"
#include "test.h"

#define ROW_MESSAGE_T 10
#define ROW_MESSAGE_B (SCREEN_HEIGHT - 2)

#define ROW_SCROLL_T (ROW_MESSAGE_T + 2)
#define ROW_SCROLL_B (SCREEN_HEIGHT - 2)

#define ROW_FOOTER (SCREEN_HEIGHT - 1)

#define BAR_LENGTH 60

#define ERROR_LIMIT UINT64_C(999999999999)

typedef enum {
	DISPLAY_MODE_NA,
	DISPLAY_MODE_SPD,
	DISPLAY_MODE_IMC
} display_mode_t;

#define display_cpu_model(str) prints(ROW_FOOTER, 0, str)

#define display_cpu_clk(freq) printf(15, 10, "CLK: %iMHz", freq)

#define display_cpu_addr_mode(str) prints(4, 75, str)

#define display_l1_cache_size(size) printf(4, 9, "%6kB", (uintptr_t)(size))

#define display_l2_cache_size(size) printf(5, 9, "%6kB", (uintptr_t)(size))

#define display_l3_cache_size(size) printf(6, 9, "%6kB", (uintptr_t)(size))

#define display_memory_size(size) printf(7, 9, "%6kB", (uintptr_t)(size))

#define display_l1_cache_speed(speed) \
	printf(4, 18, "%S6kB/s", (uintptr_t)(speed))

#define display_l2_cache_speed(speed) \
	printf(5, 18, "%S6kB/s", (uintptr_t)(speed))

#define display_l3_cache_speed(speed) \
	printf(6, 18, "%S6kB/s", (uintptr_t)(speed))

#define display_ram_speed(speed) printf(7, 18, "%S6kB/s", (uintptr_t)(speed))

#define display_status(status) printf(10, 0, "Status:        %s", status)

#define display_threading(nb, mode) printf(7, 31, "%uT (%s)", nb, mode)

#define display_threading_disabled() prints(7, 31, "Disabled")

#define display_cpu_topo_hybrid(num_pcores, num_ecores, num_threads)        \
	{                                                                   \
		clear_screen_region(7, 5, 7, 25);                           \
		printf(7, 5, "%uP+%uE-Cores (%uT)", num_pcores, num_ecores, \
		       num_threads);                                        \
	}

#define display_cpu_topo_hybrid_short(num_threads) \
	printf(7, 5, "%u Threads (Hybrid)", num_threads)

#define display_cpu_topo_multi_socket(num_sockets, num_cores, num_threads) \
	printf(7, 5, "%uS / %uC / %uT", num_sockets, num_cores, num_threads)

#define display_cpu_topo(num_cores, num_threads) \
	printf(7, 5, "%u Cores %u Threads", num_cores, num_threads)

#define display_cpu_topo_short(num_cores, num_threads) \
	printf(7, 5, "%u Cores (%uT)", num_cores, num_threads)

#define display_spec_mode(mode) prints(8, 0, mode);

#define display_spec_ddr(freq, type, cl, cl_dec, rcd, rp, ras)                \
	printf(8, 5, "%uMHz (%s-%u) CAS %u%s-%u-%u-%u", freq / 2, type, freq, \
	       cl, cl_dec ? ".5" : "", rcd, rp, ras);

#define display_spec_sdr(freq, type, cl, rcd, rp, ras)                        \
	printf(8, 5, "%uMHz (%s PC%u) CAS %u-%u-%u-%u", freq, type, freq, cl, \
	       rcd, rp, ras);

#define display_dmi_mb(sys_ma, sys_sku)       \
	dmicol = prints(23, dmicol, sys_man); \
	prints(23, dmicol + 1, sys_sku);

#define display_active_cpu(cpu_num) \
	prints(8, 7, "Core #");     \
	printi(8, 13, cpu_num, 3, false, true)

#define display_all_active() prints(8, 7, "All Cores")

#define display_spinner(spin_state) printc(7, 77, spin_state)

#define display_pass_percentage(pct) printf(15, 0, "Pass progress: %i/100", pct)

#define display_pass_bar(length)                     \
	printf(5, 0, "Pass: ");                      \
	while (length > pass_bar_length) {           \
		printc(5, 6 + pass_bar_length, '#'); \
		pass_bar_length++;                   \
	}

#define display_test_percentage(pct) printf(14, 0, "Test progress: %i/100", pct)

#define display_test_bar(length)                     \
	printf(6, 0, "Test: ");                      \
	while (length > test_bar_length) {           \
		printc(6, 6 + test_bar_length, '#'); \
		test_bar_length++;                   \
	}

#define display_test_number(number) printf(12, 0, "Test No.:      %i", number)

#define display_test_description(str) printf(11, 0, "Test:          %s", str)

#define display_test_addresses(pb, pe, total)                               \
	{                                                                   \
		clear_screen_region(4, 0, 4, SCREEN_WIDTH - 6);             \
		printf(4, 0, "%kB - %kB [%kB of %kB]", pb, pe, (pe) - (pb), \
		       total);                                              \
	}

#define display_test_stage_description(...)                      \
	{                                                        \
		clear_screen_region(16, 0, 4, SCREEN_WIDTH - 6); \
		printf(16, 0, __VA_ARGS__);                      \
	}

#define display_test_pattern_name(str)                           \
	{                                                        \
		clear_screen_region(13, 0, 5, SCREEN_WIDTH - 1); \
		printf(13, 0, "Pattern:       %s", str);         \
	}

#define display_test_pattern_value(pattern)                             \
	{                                                               \
		clear_screen_region(12, 0, 12, SCREEN_WIDTH - 1);       \
		printf(13, 0, "Test pattern:  0x%0*x", TESTWORD_DIGITS, \
		       pattern);                                        \
	}

#define display_test_pattern_values(pattern, offset)                         \
	{                                                                    \
		clear_screen_region(13, 0, 13, SCREEN_WIDTH - 1);            \
		printf(13, 0, "Test patterns: 0x%0*x - %i", TESTWORD_DIGITS, \
		       pattern, offset);                                     \
	}

#define display_run_time(hours, mins, secs) \
	printf(7, 0, "Runtime:       %i:%02i:%02i", hours, mins, secs)

#define display_pass_count(count) printf(8, 0, "Pass count:    %i", count)

#define display_error_count(count) printf(9, 0, "Error count:   %i", count)

#define clear_message_area()                                         \
	{                                                            \
		clear_screen_region(ROW_MESSAGE_T, 0, ROW_MESSAGE_B, \
				    SCREEN_WIDTH - 1);               \
		scroll_message_row = ROW_SCROLL_T - 1;               \
	}

#define display_pinned_message(row, col, ...) \
	printf(ROW_MESSAGE_T + row, col, __VA_ARGS__)

#define display_scrolled_message(col, ...) \
	printf(scroll_message_row, col, __VA_ARGS__)

#define display_notice(str) \
	prints(ROW_MESSAGE_T + 8, (SCREEN_WIDTH - strlen(str)) / 2, str)

#define display_notice_with_args(length, ...) \
	printf(ROW_MESSAGE_T + 8, (SCREEN_WIDTH - length) / 2, __VA_ARGS__)

#define clear_footer_message()                                  \
	{                                                       \
		set_background_colour(WHITE);                   \
		clear_screen_region(ROW_FOOTER, 56, ROW_FOOTER, \
				    SCREEN_WIDTH - 1);          \
		set_background_colour(BLACK);                   \
	}

#define display_footer_message(str)           \
	{                                     \
		set_foreground_colour(BLACK); \
		prints(ROW_FOOTER, 56, str);  \
		set_foreground_colour(WHITE); \
	}

#define trace(my_cpu, ...) \
	if (enable_trace)  \
	do_trace(my_cpu, __VA_ARGS__)

extern int scroll_message_row;

extern display_mode_t display_mode;

void display_init(void);

void display_cpu_topology(void);

void post_display_init(void);

void display_start_run(void);

void display_start_pass(void);

void display_start_test(void);

void display_temperature(void);

void display_big_status(bool pass);

void restore_big_status(void);

void check_input(void);

void set_scroll_lock(bool enabled);

void toggle_scroll_lock(void);

void scroll(void);

void do_tick(int my_cpu);

void do_trace(int my_cpu, const char *fmt, ...);

#endif // DISPLAY_H
