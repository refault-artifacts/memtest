
#include <stdint.h>
#include "display.h"
#include "error.h"
#include "test.h"
#include "vmem.h"
#include "heap.h"
#include "cpuinfo.h"
#include "efi.h"
#include "boot.h"
#include "bootparams.h"
#include "vmem.h"
#include "memsize.h"
#include "smbios.h"
#include "smbus.h"
#include "tsc.h"

#include "test_funcs.h"
#include "test_helper.h"
#include "build_version.h"

#include "xhci.h"

extern uintptr_t global_ws;

bool make_fault = true;

#define HIGH 1
#define LOW 0

// The fault pin refers to the pin number on the Teensy microcontroller board,
// i.e., this is directly passed to digitalWrite(...). In order to figure
// out the CA number, you have to check the schematic of the interposer
// to find the connector pin that connects to the CA pin switch of interest.
// Then, check the injection controller schematic to figure out the mapping
// from Teensy pin to connector pin.
// 36 is CA3.
#define FAULT_INJECTION_PIN 36
#define FAULT_INJECTION_STATE HIGH

#ifndef FAULT_DURATION
	#error "FAULT_DURATION not defined. Specify with -DFAULT_DURATION=..."
#endif

#ifndef FAULT_BURST_COUNT
	#error "FAULT_BURST_COUNT not defined. Specify with -DFAULT_BURST_COUNT=..."
#endif

#ifndef INTER_FAULT_DELAY
	#error "INTER_FAULT_DELAY not defined. Specify with -DINTER_FAULT_DELAY=..."
#endif

#ifndef EXPERIMENT_TITLE
	#error "EXPERIMENT_TITLE not defined. Specify with -DEXPERIMENT_TITLE=..."
#endif

#define LSB(x) __builtin_ctzll(x)


uintptr_t* funcs = NULL;
char* addr_func_string;
uint8_t number_of_funcs = 0;
uint32_t rowshift = 0;


/* INTEL */
// 1 Rank, 4 Bank groups, 4 Banks = 8 GiB
char* addr_func_string_raptor_8gib = "Raptor Lake 1RK,4BG,4BK,8GiB";
uintptr_t addr_funcs_raptor_8gib [] = { 0x00000c3200, 0x0000081100, 0x0049224000,
		      0x0092448000, 0x0024910000};
uint8_t number_of_funcs_raptor_8gib = 5;
uint32_t rowshift_raptor_8gib = 17;

// 1 Rank, 8 Bank groups, 4 Banks = 16 GiB
char* addr_func_string_raptor_16gib = "Raptor Lake 1RK,8BG,4BK,16GiB";
uintptr_t addr_funcs_raptor_16gib [] = { 0x00000c3200, 0x0000081100, 0x0088844000,
			0x0111108000, 0x0222210000, 0x0044420000 };
uint8_t number_of_funcs_raptor_16gib = 6;
uint32_t rowshift_raptor_16gib = 18;

// 2 Ranks, 8 Bank groups, 4 Banks = 32 GiB
char* addr_func_string_raptor_32gib = "Raptor Lake 2RK,8BG,4BK,32GiB";
uintptr_t addr_funcs_raptor_32gib [] = { 0x00000c3200, 0x0000410000, 0x0000081100,
	      0x0222104000, 0x0444208000, 0x0088820000, 0x0111040000 };
uint8_t number_of_funcs_raptor_32gib = 7;
uint32_t rowshift_raptor_32gib = 19;

/* AMD */
// 1 Rank, 4 Bank groups, 4 Banks = 8 GiB
char* addr_func_string_zen4_8gib = "Zen4 1RK,4BG,4BK,8GiB";
uintptr_t addr_funcs_zen4_8gib [] = { 0x1fffe0040, 0x88880100, 0x111100200,
		      0x22220400, 0x44440800};
uint8_t number_of_funcs_zen4_8gib = 5;
uint32_t rowshift_zen4_8gib = 17;

// 1 Rank, 8 Bank groups, 4 Banks = 16 GiB
char* addr_func_string_zen4_16gib = "Zen4 1RK,8BG,4BK,16GiB";
uintptr_t addr_funcs_zen4_16gib [] = { 0x3fffc0040, 0x42100100, 0x84200200,
	      0x108401000, 0x210840400, 0x21080800 };
uint8_t number_of_funcs_zen4_16gib = 6;
uint32_t rowshift_zen4_16gib = 18;

// 2 Ranks, 8 Bank groups, 4 Banks = 32 GiB
char* addr_func_string_zen4_32gib = "Zen4 2RK,8BG,4BK,32GiB";
uintptr_t addr_funcs_zen4_32gib [] = { 0x7fff80040, 0x40000, 0x84200100,
	      0x108400200, 0x210801000, 0x421080400, 0x42100800 };
uint8_t number_of_funcs_zen4_32gib = 7;
uint32_t rowshift_zen4_32gib = 19;

uintptr_t get_row(uint64_t msb, uint64_t row_idx, uint8_t no_of_funcs)
{
	uint64_t addr = msb | (row_idx << rowshift);
	for (int fnc_idx = 0; fnc_idx < no_of_funcs; fnc_idx++) {
		if (__builtin_parityll(addr & funcs[fnc_idx]) != 0) {
			addr ^= 1 << (LSB(funcs[fnc_idx]));
		}
	}
	return addr;
}

inline static void clflush(volatile void *addr)
{
	__asm__ volatile("clflush (%0)" : : "r"(addr));
}
inline static void clflushopt(volatile void *addr)
{
	__asm__ volatile("clflushopt (%0)" : : "r"(addr));
}
inline static void prefetchnta(volatile void *addr)
{
	__asm__ volatile("prefetchnta (%0)" : : "r"(addr));
}
inline static void mfence()
{
	__asm__ volatile("mfence");
}

uint8_t gp_data[64] = { 0xBB, 0x12 };

void ddr5_test()
{
#define SIZE 256 * (1 << MB)


    print_usb_mirror("@EXPERIMENT STARTED");
    print_usb_mirror("VERSION=%s", MT_VERSION);
    print_usb_mirror("GIT_HASH=%s", GIT_HASH);
    print_usb_mirror("BUILD_TIME=%s",BUILD_TIME);
    print_usb_mirror("TITLE=%s",EXPERIMENT_TITLE);
    print_usb_mirror("FAULT_INJECTION_PIN=%i",FAULT_INJECTION_PIN);
    print_usb_mirror("FAULT_DURATION=%i",FAULT_DURATION);
    print_usb_mirror("FAULT_INJECTION_STATE=%s",
            FAULT_INJECTION_STATE==HIGH?"HIGH":"LOW");
    print_usb_mirror("FAULT_BURST_COUNT=%i",FAULT_BURST_COUNT);
    print_usb_mirror("INTER_FAULT_DELAY=%i",INTER_FAULT_DELAY);
    print_usb_mirror("ROW_IDX=%i",ROW_IDX);
    print_usb_mirror("HAMMER_COUNT=%i",HAMMER_COUNT);
    print_usb_mirror("AGGR_A_ROW_IDX=%i",AGGR_A_ROW_IDX);
    print_usb_mirror("AGGR_B_ROW_IDX=%i",AGGR_B_ROW_IDX);
    print_usb_mirror("EXPERIMENT_MEMORY_SIZE=%i",SIZE);
    print_usb_mirror("CPU_MODEL=%s",cpu_model);
    print_usb_mirror("MOTHERBOARD=%s %s",baseboard_man, baseboard_sku);
    print_usb_mirror("DIMM_CAPACITY=%i", DIMM_size); 

    char* intel = strstr(cpu_model, "Intel");
    char* amd = strstr(cpu_model, "AMD");

    uint64_t module_size = DIMM_size;

    /* Address function override */
    if (!strncmp(ADDR_MODE, "override", 4)){
    	print_usb_mirror("WARNING: ADDR FUNC OVERRIDE IN USE");
	module_size = MODULE_SIZE; 
    }

    /* Auto-detect address function */
    if (amd){
	    switch(module_size){
		case 8388608: // 8 GiB
			addr_func_string = addr_func_string_zen4_8gib;
			funcs = addr_funcs_zen4_8gib;
			number_of_funcs = number_of_funcs_zen4_8gib;
			rowshift = rowshift_zen4_8gib;
			break;
		default: 
			print_usb_mirror("WARNING: COULD NOT DETECT DIMM CAPACITY, USING FALLBACK ADDR FUNC");
			 __attribute__((fallthrough));
		case 16777216: // 16 GiB
			addr_func_string = addr_func_string_zen4_16gib;
			funcs = addr_funcs_zen4_16gib;
			number_of_funcs = number_of_funcs_zen4_16gib;
			rowshift = rowshift_zen4_16gib;
			break;
		case 33554432: // 32 GiB
			addr_func_string = addr_func_string_zen4_32gib;
			funcs = addr_funcs_zen4_32gib;
			number_of_funcs = number_of_funcs_zen4_32gib;
			rowshift = rowshift_zen4_32gib;
			break;
	    }
    }
    else {
    	if (!intel){
		print_usb_mirror("WARNING: COULD NOT DETECT CPU MODEL, USING FALLBACK");
	}
	    switch(module_size){
		case 8388608: // 8 GiB
			addr_func_string = addr_func_string_raptor_8gib;
			funcs = addr_funcs_raptor_8gib;
			number_of_funcs = number_of_funcs_raptor_8gib;
			rowshift = rowshift_raptor_8gib;
			break;
		default: 
			print_usb_mirror("WARNING: COULD NOT DETECT DIMM CAPACITY, USING FALLBACK ADDR FUNC");
			 __attribute__((fallthrough));
		case 16777216: // 16 GiB
			addr_func_string = addr_func_string_raptor_16gib;
			funcs = addr_funcs_raptor_16gib;
			number_of_funcs = number_of_funcs_raptor_16gib;
			rowshift = rowshift_raptor_16gib;
			break;
		case 33554432: // 32 GiB
			addr_func_string = addr_func_string_raptor_32gib;
			funcs = addr_funcs_raptor_32gib;
			number_of_funcs = number_of_funcs_raptor_32gib;
			rowshift = rowshift_raptor_32gib;
			break;
	    }

    }
    print_usb_mirror("ADDR_FUNC=%s", addr_func_string);
    print_usb_mirror("ADDR_MODE=%s", ADDR_MODE);
    print_usb_mirror("DIMM=%s %s %s %kB W%i-%i",DIMM_type, DIMM_man, DIMM_sku, DIMM_size, DIMM_week, DIMM_year);
    print_usb_mirror("----------------------------------------------");

    
	uint8_t buf[64] = { 0xAA, 0xBB };

	buf[2] = FAULT_INJECTION_PIN;
	buf[3] = (FAULT_DURATION >> 8) & 0xff;
	buf[4] = FAULT_DURATION & 0xff;
	buf[5] = FAULT_INJECTION_STATE;
	buf[6] = FAULT_BURST_COUNT;
	buf[7] = (INTER_FAULT_DELAY >> 8) & 0xff;
	buf[8] = INTER_FAULT_DELAY & 0xff;

    uintptr_t heap_addr = heap_alloc(HEAP_TYPE_HM_1, SIZE, SIZE);
	print_usb_mirror("heap addr: 0x%016x", heap_addr);
    if (heap_addr == (uintptr_t)NULL){
        print_usb_mirror("heap alloc returned NULL, giving up.");
        while (1);
    }

    //print_usb_mirror("mapping heap addr to virt addr 2 GB, uncached");
    //map_window_uncached(heap_addr, SIZE / (2 << MB), 1);
    uintptr_t addr = heap_addr;//((uintptr_t)2 << GB);
    
    /*
    print_usb_mirror("performing sanity check of mapping...");
    for (uintptr_t i = addr; i < (addr + SIZE / 8); i++){
        clflushopt((uint64_t*)addr);
        clflushopt((uint64_t*)heap_addr);
        mfence();
        uint64_t heap_data = *((uint64_t*)heap_addr);
        uint64_t uncached_data = *((uint64_t*)addr);
        if (heap_data != uncached_data) {
            print_usb_mirror("check failed, giving up.");
            while(1);
        }
    }
	*/    
    //uint64_t bitflips[8] = {0,0,0,0,0,0,0,0};
    uint64_t bitflips;
	
    /* fill memory */
	for (uintptr_t p = addr; p < addr + SIZE; p += sizeof(uintptr_t)) {
		//*(uintptr_t *)p = p;
		*(uintptr_t *)p = 0x00;
		clflushopt((uintptr_t *)p);
	}
	print_usb_mirror("memory filled");

	//volatile uintptr_t *scan_start =
	//	(volatile uintptr_t *)get_row(heap_addr, 7 + (ROW_IDX - 1));
	volatile uintptr_t *a =
		(volatile uintptr_t *)get_row(heap_addr, AGGR_A_ROW_IDX + ROW_IDX, number_of_funcs);
	volatile uintptr_t *b =
		(volatile uintptr_t *)get_row(heap_addr, AGGR_B_ROW_IDX + ROW_IDX, number_of_funcs);
	//volatile uintptr_t *scan_end =
	//	(volatile uintptr_t *)get_row(heap_addr, 15 + (ROW_IDX + 1));

	print_usb_mirror("a: 0x%016x", a);
	print_usb_mirror("b: 0x%016x", b);

	for (volatile uint64_t i = 0; i < 0x1ffff; i++);

    	volatile uint64_t start = get_tsc();

    	uint64_t end = start + ((FAULT_DURATION + INTER_FAULT_DELAY) * FAULT_BURST_COUNT * clks_per_usec);

	/* trigger fault */
	send_data_uc_ep(global_ws, buf);

	for (uint64_t i = 0; i < HAMMER_COUNT; i++) {
		*a;
		*b;
		//*a = 0;
		//*b = 0;
		clflushopt(a);
		clflushopt(b);
		mfence();
	}

    	while (end > get_tsc());

    	//map_window_uncached(heap_addr, SIZE / (2 << MB), 0);

    	//print_usb_mirror("TSC start: 0x%016x, end: 0x%016x", start, end);
   	//print_usb_mirror("clks_per_msec: %i, clks_per_usec: %i", clks_per_msec, clks_per_usec);
	print_usb_mirror("waited for %i ms", (uint64_t)(((double)(end - start) / clks_per_msec)));
    	print_usb_mirror("checking memory");

	/* checking memory for bit errors */
    	uint8_t splits = 32;
    	print_usb_mirror("splits: 0x%016x", (uint64_t)&splits);
	for (uint8_t split = 0; split < splits; split++) {
        for (uintptr_t p = addr + ((SIZE / splits) * split); 
                p < addr + ((SIZE / splits) * (split + 1));
                p += sizeof(uint64_t)) {
		    clflushopt((uintptr_t *)p);
		    mfence();
		    //prefetchnta((uintptr_t *)p);
		    //mfence();
		    uintptr_t read = *(uintptr_t *)p;
		    bitflips += __builtin_popcount(read);
		    if (read != 0) {
                print_usb_mirror("addr 0x%08x: expected 0x%016x, got 0x%016x", p, 0, read);
			/*for (int j=0; j<8; j++){
				if ((uint8_t)(*(uint8_t *)(p+j) ) != 0x0){
					bitflips[j]++;
				}
			}
			*/
		    }
	    }
        //print_usb_mirror("%i %% done.", 
          //      (uint8_t)((double)split*(100.0/splits)));
    } 

    uint64_t bitflips_tot=bitflips;

    //for (uint8_t i=0; i<8; i++){
    //	bitflips_tot+=bitflips[i];
    //}
    print_usb_mirror(">>> FOUND %i BIT ERRORS", bitflips_tot);
    /*print_usb_mirror("Errors per byte: %04i %04i %04i %04i %04i %04i %04i %04i",
		    bitflips[7],bitflips[6],bitflips[5],bitflips[4],
		    bitflips[3],bitflips[2],bitflips[1],bitflips[0]);
    print_usb_mirror("base addr offset [ 7] [ 6] [ 5] [ 4] [ 3] [ 2] [ 1] [ 0]");
    char string[16];
    if (bitflips_tot > 16){
	uint8_t ptr = 0;
	uint8_t first = 1;
    	for (uint8_t i=0; i<8; i++){
		if (bitflips[i] > bitflips_tot/16){
			if (!first){
				string[ptr++]=',';
			}
			first = 0;
			string[ptr++] = i+'0';
		}
    	}
	string[ptr]='\0';
	print_usb_mirror("Bytes {%s} seem to be corrupted", string);
    }
	*/
    print_usb_mirror("");
    print_usb_mirror("EXPERIMENT COMPLETED");
    print_usb_flush();
}
