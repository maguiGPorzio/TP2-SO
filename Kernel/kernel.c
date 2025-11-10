// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include <stdint.h>
// #include <string.h>
#include <lib.h>
#include <moduleLoader.h>
#include "keyboard.h"
#include "idtLoader.h"
#include "time.h"
#include "videoDriver.h"
#include "sound.h"
#include "interrupts.h"
#include "memoryManager.h"
#include "process.h"
#include "scheduler.h"
#include "synchro.h"
#include "pipes.h"

extern void timer_tick();

extern uint8_t text;
extern uint8_t rodata;
extern uint8_t data;
extern uint8_t bss;
extern uint8_t endOfKernelBinary;
extern uint8_t endOfKernel;

static const uint64_t PageSize = 0x1000;

static void * const sampleCodeModuleAddress = (void*)0x400000;
static void * const sampleDataModuleAddress = (void*)0x500000;

typedef int (*EntryPoint)();


void clearBSS(void * bssAddress, uint64_t bssSize)
{
	memset(bssAddress, 0, bssSize);
}

void * getStackBase()
{
	return (void*)(
		(uint64_t)&endOfKernel
		+ PageSize * 8				//The size of the stack itself, 32KiB
		- sizeof(uint64_t)			//Begin at the top of the stack
	);
}


void * initializeKernelBinary()
{
	void * moduleAddresses[] = {
		sampleCodeModuleAddress,
		sampleDataModuleAddress
	};

	loadModules(&endOfKernelBinary, moduleAddresses);

	clearBSS(&bss, &endOfKernel - &bss);

	load_idt();

	return getStackBase();
}



int main() {

	init_kernel_memory_manager();

	init_scheduler();

	init_semaphore_manager();
	
	//((EntryPoint)sampleCodeModuleAddress)();

	init_pipes();
	
	init_keyboard_sem();

	timer_tick();
	
	return -1;
}
