// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2020-2022 Martin Whitaker.
// Copyright (C) 2004-2022 Sam Demeulemeester.

#include "display.h"

#include <stdbool.h>
#include <stdint.h>

#include "barrier.h"
#include "build_version.h"
#include "config.h"
#include "cpuid.h"
#include "cpuinfo.h"
#include "error.h"
#include "hwctrl.h"
#include "io.h"
#include "keyboard.h"
#include "pmem.h"
#include "serial.h"
#include "smbios.h"
#include "smbus.h"
#include "spinlock.h"
#include "temperature.h"
#include "tests.h"
#include "tsc.h"

#include "xhci.h"
#include "microcontroller.h"

extern uintptr_t global_ws;
uint32_t usb_heartbeat_period = 5;
uint8_t hb_buf[64];

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

#define POP_STAT_R 12
#define POP_STAT_C 18

#define POP_STAT_W 44
#define POP_STAT_H 11

#define POP_STAT_LAST_R (POP_STAT_R + POP_STAT_H - 1)
#define POP_STAT_LAST_C (POP_STAT_C + POP_STAT_W - 1)

#define POP_STATUS_REGION \
	POP_STAT_R, POP_STAT_C, POP_STAT_LAST_R, POP_STAT_LAST_C

#define SPINNER_PERIOD 100 // milliseconds

#define NUM_SPIN_STATES 4

static const char spin_state[NUM_SPIN_STATES] = { '|', '/', '-', '\\' };

static const char cpu_mode_str[3][4] = { "PAR", "SEQ", "RR " };

//------------------------------------------------------------------------------
// Private Variables
//------------------------------------------------------------------------------

static bool scroll_lock = false;
static bool scroll_wait = false;

static int spin_idx = 0; // current spinner position

static int pass_ticks = 0; // current value (ticks_per_pass is final value)
static int test_ticks = 0; // current value (ticks_per_test is final value)

static int pass_bar_length = 0; // currently displayed length
static int test_bar_length = 0; // currently displayed length

static uint64_t run_start_time = 0; // TSC time stamp
static uint64_t next_spin_time = 0; // TSC time stamp

static int prev_sec = -1; // previous second
static bool timed_update_done = false; // update cycle status

bool big_status_displayed = false;
static uint16_t popup_status_save_buffer[POP_STAT_W * POP_STAT_H];

//------------------------------------------------------------------------------
// Variables
//------------------------------------------------------------------------------

int scroll_message_row;

int max_cpu_temp = 0;

display_mode_t display_mode = DISPLAY_MODE_NA;

//------------------------------------------------------------------------------
// Public Functions
//------------------------------------------------------------------------------

void display_init(void)
{
	cursor_off();

	clear_screen();

	/* The commented horizontal lines provide visual cue for where and how
	 * they will appear on the screen. They are drawn down below using
	 * Extended ASCII characters.
	 */

	set_foreground_colour(RED);
	set_background_colour(WHITE);
	clear_screen_region(0, 0, 0, 43);
	prints(0, 0, "DDR5 Fault Injection Control Software v" MT_VERSION);
	set_foreground_colour(WHITE);
	set_background_colour(BLACK);
	prints(0, 45, "Created by COMSEC @ ETH ZUERICH");
	prints(1, 0, "Adapted from Memtest86+ v6.1 licensed under GPL-2.0");

	set_foreground_colour(BLACK);
	set_background_colour(WHITE);
	clear_screen_region(ROW_FOOTER, 0, ROW_FOOTER, SCREEN_WIDTH - 1);
	prints(ROW_FOOTER, 64, MT_VERSION "." GIT_HASH);
#if TESTWORD_WIDTH > 32
	prints(ROW_FOOTER, 76, ".x64");
#else
	prints(ROW_FOOTER, 76, ".x32");
#endif

	if (cpu_model) {
		display_cpu_model(cpu_model);
    }

	set_foreground_colour(WHITE);
	set_background_colour(BLACK);

	if (ram_speed) {
		// display_ram_speed(ram_speed);
	}
	scroll_message_row = ROW_SCROLL_T;
}

void display_cpu_topology(void)
{
	extern int num_enabled_cpus;
	int num_cpu_sockets = 1;
}

void post_display_init(void)
{
	print_smbios_startup_info();
	print_smbus_startup_info();

	// if (false) {
	//  Try to get RAM information from IMC (TODO)
	// display_spec_mode("IMC: ");
	// display_spec_ddr(ram.freq, ram.type, ram.tCL, ram.tCL_dec,
	//	 ram.tRCD, ram.tRP, ram.tRAS);
	// display_mode = DISPLAY_MODE_IMC;
	//} else if (ram.freq > 0 && ram.tCL > 0) {
	// If not available, grab max memory specs from SPD
	// display_spec_mode("RAM: ");
	// if (ram.freq <= 166) {
	// display_spec_sdr(ram.freq, ram.type, ram.tCL, ram.tRCD,
	//		 ram.tRP, ram.tRAS);
	//} else {
	// display_spec_ddr(ram.freq, ram.type, ram.tCL,
	//	 ram.tCL_dec, ram.tRCD, ram.tRP,
	//		 ram.tRAS);
	//}
	// display_mode = DISPLAY_MODE_SPD;
	//} else {
	// If nothing available, fallback to "Using Core" Display
	display_mode = DISPLAY_MODE_NA;
	//}
}

void display_start_run(void)
{
	if (!enable_trace && !enable_sm) {
		clear_message_area();
	}

	clear_screen_region(10, 10, 10, 57); // run time
	clear_screen_region(11, 10, 11, 57); // pass number
	clear_screen_region(12, 10, 12, SCREEN_WIDTH - 1); // error count
	display_pass_count(0);
	display_error_count(0);
	display_status("Testing");

	if (enable_tty) {
		tty_full_redraw();
	}
}

void display_start_pass(void)
{
	clear_screen_region(4, 0, 1, SCREEN_WIDTH - 1); // progress bar
	clear_screen_region(7, 14, 7, SCREEN_WIDTH - 1);
	// display_pass_percentage(0);
	pass_bar_length = 0;
	pass_ticks = 0;
}

void display_start_test(void)
{
	// clear_screen_region(5, 6, 6,
	//		    SCREEN_WIDTH - 1); // progress bars
	clear_screen_region(7, 14, 7, SCREEN_WIDTH - 1); // runtime
	clear_screen_region(11, 14, 15, SCREEN_WIDTH - 1); // other stuff
	// display_test_percentage(0);
	display_test_number(test_num);
	display_test_description(test_list[test_num].description);
	test_bar_length = 0;
	test_ticks = 0;
}

void display_temperature(void)
{
	if (!enable_temperature) {
		return;
	}

	int actual_cpu_temp = get_cpu_temperature();

	if (actual_cpu_temp == 0) {
		if (max_cpu_temp == 0) {
			enable_temperature = false;
			no_temperature = true;
		}
		return;
	}

	if (max_cpu_temp < actual_cpu_temp) {
		max_cpu_temp = actual_cpu_temp;
	}

	int offset = actual_cpu_temp / 100 + max_cpu_temp / 100;

	clear_screen_region(1, 18, 1, 22);
	printf(1, 20 - offset, "%2i/%2i%cC", actual_cpu_temp, max_cpu_temp,
	       0xF8);
}

void display_big_status(bool pass)
{
	if (!enable_big_status || big_status_displayed) {
		return;
	}

	save_screen_region(POP_STATUS_REGION, popup_status_save_buffer);

	set_background_colour(BLACK);
	set_foreground_colour(pass ? GREEN : RED);
	clear_screen_region(POP_STATUS_REGION);

	if (pass) {
		prints(POP_STAT_R + 1, POP_STAT_C + 5,
		       "######      ##      #####    #####  ");
		prints(POP_STAT_R + 2, POP_STAT_C + 5,
		       "##   ##    ####    ##   ##  ##   ## ");
		prints(POP_STAT_R + 3, POP_STAT_C + 5,
		       "##   ##   ##  ##   ##       ##      ");
		prints(POP_STAT_R + 4, POP_STAT_C + 5,
		       "######   ##    ##   #####    #####  ");
		prints(POP_STAT_R + 5, POP_STAT_C + 5,
		       "##       ########       ##       ## ");
		prints(POP_STAT_R + 6, POP_STAT_C + 5,
		       "##       ##    ##  ##   ##  ##   ## ");
		prints(POP_STAT_R + 7, POP_STAT_C + 5,
		       "##       ##    ##   #####    #####  ");
	} else {
		prints(POP_STAT_R + 1, POP_STAT_C + 5,
		       "#######     ##      ######   ##     ");
		prints(POP_STAT_R + 2, POP_STAT_C + 5,
		       "##         ####       ##     ##     ");
		prints(POP_STAT_R + 3, POP_STAT_C + 5,
		       "##        ##  ##      ##     ##     ");
		prints(POP_STAT_R + 4, POP_STAT_C + 5,
		       "#####    ##    ##     ##     ##     ");
		prints(POP_STAT_R + 5, POP_STAT_C + 5,
		       "##       ########     ##     ##     ");
		prints(POP_STAT_R + 6, POP_STAT_C + 5,
		       "##       ##    ##     ##     ##     ");
		prints(POP_STAT_R + 7, POP_STAT_C + 5,
		       "##       ##    ##   ######   ###### ");
	}

	prints(POP_STAT_R + 8, POP_STAT_C + 5,
	       "                                    ");
	prints(POP_STAT_R + 9, POP_STAT_C + 5,
	       "Press any key to remove this banner ");

	set_background_colour(BLACK);
	set_foreground_colour(WHITE);
	big_status_displayed = true;
}

void restore_big_status(void)
{
	if (!big_status_displayed) {
		return;
	}

	restore_screen_region(POP_STATUS_REGION, popup_status_save_buffer);
	big_status_displayed = false;
}

void check_input(void)
{
	char input_key = get_key();

	if (input_key == '\0') {
		return;
	} else if (big_status_displayed) {
		restore_big_status();
	}

	switch (input_key) {
	case ESC:
		clear_message_area();
		display_notice("Rebooting...");
		reboot();
		break;
	case '1':
		config_menu(false);
		break;
	case ' ':
		set_scroll_lock(!scroll_lock);
		break;
	case '\n':
		scroll_wait = false;
		break;
	default:
		break;
	}
}

void set_scroll_lock(bool enabled)
{
	scroll_lock = enabled;
	set_foreground_colour(BLACK);
	prints(ROW_FOOTER, 48, scroll_lock ? "unlock" : "lock  ");
	set_foreground_colour(WHITE);
}

void toggle_scroll_lock(void)
{
	set_scroll_lock(!scroll_lock);
}

void scroll(void)
{
	if (scroll_message_row < ROW_SCROLL_B) {
		scroll_message_row++;
	} else {
		if (scroll_lock) {
			display_footer_message("<Enter> Single step     ");
		}
		scroll_wait = true;
		do {
			check_input();
		} while (scroll_wait && scroll_lock);

		scroll_wait = false;
		clear_footer_message();
		scroll_screen_region(ROW_SCROLL_T, 0, ROW_SCROLL_B,
				     SCREEN_WIDTH - 1);
	}
}

void do_tick(int my_cpu)
{
	int act_sec = 0;
	bool use_spin_wait = (power_save < POWER_SAVE_HIGH);
	if (use_spin_wait) {
		barrier_spin_wait(run_barrier);
	} else {
		barrier_halt_wait(run_barrier);
	}
	if (master_cpu == my_cpu) {
		check_input();
		error_update();
	}
	if (use_spin_wait) {
		barrier_spin_wait(run_barrier);
	} else {
		barrier_halt_wait(run_barrier);
	}

	// Only the master CPU does the update.
	if (master_cpu != my_cpu) {
		return;
	}

	test_ticks++;
	pass_ticks++;

	pass_type_t pass_type = (pass_num == 0) ? FAST_PASS : FULL_PASS;

	int pct = 0;
	if (ticks_per_test[pass_type][test_num] > 0) {
		pct = 100 * test_ticks / ticks_per_test[pass_type][test_num];
		if (pct > 100) {
			pct = 100;
		}
	}
	// display_test_percentage(pct);
	// display_test_bar((BAR_LENGTH * pct) / 100);

	pct = 0;
	if (ticks_per_pass[pass_type] > 0) {
		pct = 100 * pass_ticks / ticks_per_pass[pass_type];
		if (pct > 100) {
			pct = 100;
		}
	}
	// display_pass_percentage(pct);
	// display_pass_bar((BAR_LENGTH * pct) / 100);

	int secs = 0;
	int mins = 0;
	int hours = 0;

	bool update_spinner = true;
	if (clks_per_msec > 0) {
		uint64_t current_time = get_tsc();

		secs = (current_time - run_start_time) /
		       (1000 * (uint64_t)clks_per_msec);
		mins = secs / 60;
		secs %= 60;
		act_sec = secs;
		hours = mins / 60;
		mins %= 60;
		display_run_time(hours, mins, secs);

		if (current_time >= next_spin_time) {
			next_spin_time =
				current_time + SPINNER_PERIOD * clks_per_msec;
		} else {
			update_spinner = false;
		}
	}

	/* ---------------
	 * Timed functions
	 * --------------- */

	// update spinner every SPINNER_PERIOD ms
	if (update_spinner) {
		spin_idx = (spin_idx + 1) % NUM_SPIN_STATES;
		display_spinner(spin_state[spin_idx]);
	}

	// This only tick one time per second
	if (!timed_update_done) {
		// Display FAIL banner if (new) errors detected
		if (err_banner_redraw && !big_status_displayed &&
		    error_count > 1) {
			display_big_status(false);
		}

		// Update temperature
		// display_temperature();

		// Update TTY one time every TTY_UPDATE_PERIOD second(s)
		if (enable_tty) {
			if (act_sec % tty_update_period == 0) {
				tty_partial_redraw();
			}
		}

		if (act_sec % usb_heartbeat_period == 0) {
			// USB heart beat
			hb_buf[0] = 0xBB;
			hb_buf[1] = 0x14;
			memset(hb_buf + 3, 0x00, 64 - 3);
			/* Also send runtime */
			hb_buf[3] = (hours % 0xff00) >> 8;
			hb_buf[4] = hours % 0x00ff;
			hb_buf[5] = mins;
			hb_buf[6] = secs;
			hb_buf[7] = 0;

			send_data_uc_ep(global_ws, hb_buf);
		}

		timed_update_done = true;
	}

	if (act_sec != prev_sec) {
		prev_sec = act_sec;
		timed_update_done = false;
	}
}

void do_trace(int my_cpu, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	spin_lock(error_mutex);
	scroll();
	printi(scroll_message_row, 0, my_cpu, 2, false, false);
	vprintf(scroll_message_row, 4, fmt, args);
	spin_unlock(error_mutex);
	va_end(args);
}
