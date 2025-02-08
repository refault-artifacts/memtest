// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2020-2022 Martin Whitaker.
//
// Derived from extract of memtest86+ lib.c:
//
// lib.c - MemTest-86  Version 3.4
//
// Released under version 2 of the Gnu Public License.
// By Chris Brady

#include <stdint.h>

#include "cpuid.h"
#include "hwctrl.h"
#include "keyboard.h"
#include "screen.h"
#include "smp.h"

#include "error.h"
#include "display.h"

#include "interrupt.h"

#include "../lib/print.h"

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

#define HLT_OPCODE 0xf4
#define JE_OPCODE 0x74

#ifdef __x86_64__
#define REG_PREFIX "r"
#define REG_DIGITS "16"
#define ADR_DIGITS "12"
#else
#define REG_PREFIX "e"
#define REG_DIGITS "8"
#define ADR_DIGITS "8"
#endif

static const char *codes[] = { "Divide by 0", "Debug",	     "NMI",
			       "Breakpoint",  "Overflow",    "Bounds",
			       "Invalid Op",  "No FPU",	     "Double fault",
			       "Seg overrun", "Invalid TSS", "Seg fault",
			       "Stack fault", "Gen prot.",   "Page fault",
			       "Reserved",    "FPU error",   "Alignment",
			       "Machine chk", "SIMD FPE" };

//------------------------------------------------------------------------------
// Types
//------------------------------------------------------------------------------

#ifdef __x86_64__
typedef uint64_t reg_t;
#else
typedef uint32_t reg_t;
#endif

struct trap_regs {
	reg_t ds;
	reg_t es;
	reg_t ss;
	reg_t ax;
	reg_t bx;
	reg_t cx;
	reg_t dx;
	reg_t di;
	reg_t si;
#ifndef __x86_64__
	reg_t reserved1;
	reg_t reserved2;
	reg_t sp;
#endif
	reg_t bp;
	reg_t vect;
	reg_t code;
	reg_t ip;
	reg_t cs;
	reg_t flags;
#ifdef __x86_64__
	reg_t sp;
#endif
};

//------------------------------------------------------------------------------
// Public Functions
//------------------------------------------------------------------------------

void interrupt(struct trap_regs *trap_regs)
{
	// Get the page fault address.
	uintptr_t address = 0;
	if (trap_regs->vect == 14) {
#ifdef __x86_64__
		__asm__("movq %%cr2, %0" : "=r"(address));
#else
		__asm__("movl %%cr2, %0" : "=r"(address));
#endif
	}

	if (trap_regs->vect == 2) {
		uint8_t *pc = (uint8_t *)trap_regs->ip;
		if (pc[-1] == HLT_OPCODE) {
			// Assume this is a barrier wakeup signal sent via IPI.
			return;
		}
		// Catch the rare case that a core will fail to reach the HLT
		// instruction before its wakeup signal arrives. The barrier
		// code contains an atomic decrement, a JE instruction (two
		// bytes), and a HLT instruction (one byte). The atomic
		// decrement must have completed if another core has reached the
		// point of sending the wakeup signals, so we should find the
		// HLT opcode either at pc[0] or at pc[2]. If we find it, adjust
		// the interrupt return address to point to the following
		// instruction.
		if (pc[0] == HLT_OPCODE ||
		    (pc[0] == JE_OPCODE && pc[2] == HLT_OPCODE)) {
			uintptr_t *return_addr;
			if (cpuid_info.flags.lm == 1) {
				return_addr = (uintptr_t *)(trap_regs->sp - 40);
			} else {
				return_addr = (uintptr_t *)(trap_regs->sp - 12);
			}
			if (pc[2] == HLT_OPCODE) {
				*return_addr += 3;
			} else {
				*return_addr += 1;
			}
			return;
		}
#if REPORT_PARITY_ERRORS
		parity_error();
		return;
#endif
	}

	spin_lock(error_mutex);

    print_usb_mirror("----------------------------------------------------");
	print_usb_mirror("Unexpected interrupt on CPU %i", smp_my_cpu_num());
	if (trap_regs->vect <= 19) {
		print_usb_mirror("Type: %s", codes[trap_regs->vect]);
	} else {
		print_usb_mirror("Type: %i", trap_regs->vect);
	}
	print_usb_mirror("IP: 0x%016x", (uintptr_t)trap_regs->ip);
	print_usb_mirror("CS: 0x%016x", (uintptr_t)trap_regs->cs);
	print_usb_mirror("Flag: 0x%016x", (uintptr_t)trap_regs->flags);
	print_usb_mirror("Code: 0x%016x", (uintptr_t)trap_regs->code);
	print_usb_mirror("DS: 0x%016x", (uintptr_t)trap_regs->ds);
	print_usb_mirror("ES: 0x%016x", (uintptr_t)trap_regs->es);
	print_usb_mirror("SS: 0x%016x", (uintptr_t)trap_regs->ss);
	if (trap_regs->vect == 14) {
		print_usb_mirror("Addr: 0x%016x", address);
	}

	print_usb_mirror("rax: 0x%016x", (uintptr_t)trap_regs->ax);
	print_usb_mirror("rbx: 0x%016x", (uintptr_t)trap_regs->bx);
	print_usb_mirror("rcx: 0x%016x", (uintptr_t)trap_regs->cx);
	print_usb_mirror("rdx: 0x%016x", (uintptr_t)trap_regs->dx);
	print_usb_mirror("rdx: 0x%016x", (uintptr_t)trap_regs->dx);
	print_usb_mirror("rdi: 0x%016x", (uintptr_t)trap_regs->di);
	print_usb_mirror("rsi: 0x%016x", (uintptr_t)trap_regs->si);
	print_usb_mirror("rbp: 0x%016x", (uintptr_t)trap_regs->bp);
	print_usb_mirror("rsp: 0x%016x", (uintptr_t)trap_regs->sp);
	
	print_usb_mirror("Stack:");
	for (int i = 0; i < 12; i++) {
		uintptr_t addr = trap_regs->sp + sizeof(reg_t) * (11 - i);
		reg_t data = *(reg_t *)addr;
		print_usb_mirror("0x%016x: 0x%016x", addr, (uintptr_t)data);
	}

	print_usb_mirror("CS:IP:");
	uint8_t *pp = (uint8_t *)((uintptr_t)trap_regs->ip);
	for (int i = 0; i < 12; i++) {
		print_usb_mirror("%02x", (uintptr_t)pp[i]);
	}

    print_usb_mirror("exiting exception handler...");
    print_usb_mirror("----------------------------------------------------");

	return;
	//	clear_screen_region(ROW_FOOTER, 0, ROW_FOOTER, SCREEN_WIDTH -
	// 1); 	prints(ROW_FOOTER, 0, "Press any key to reboot...");
	/*
		while (get_key() == 0) {
		}
		reboot();*/
}
