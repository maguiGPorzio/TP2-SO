#include <stdint.h>
#include <string.h>
#include <lib.h>
#include <moduleLoader.h>
#include "keyboard.h"
#include "idtLoader.h"
#include "time.h"
#include "videoDriver.h"
#include "sound.h"
#include "interrupts.h"

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
	 // Inicializar Memory Manager
    initKernelMemoryManager();

	// Test básico del memory manager
    testMemoryManager();

	((EntryPoint)sampleCodeModuleAddress)();
	
	return 0;
}

// Función de test para verificar que funciona
void testMemoryManager() {
    print("\n=== Testing Memory Manager ===\n");
    
    // Test 1: Allocación simple
    void* ptr1 = sys_malloc(100);
    print("Test 1: Allocated 100 bytes at %p\n", ptr1);
    
    // Test 2: Múltiples allocaciones
    void* ptr2 = sys_malloc(200);
    void* ptr3 = sys_malloc(300);
    print("Test 2: Allocated 200 bytes at %p\n", ptr2);
    print("Test 2: Allocated 300 bytes at %p\n", ptr3);
    
    // Mostrar estado
    MemStatus status = sys_memStatus();
    print("\nMemory Status:\n");
    print("  Total: %d KB\n", status.totalMemory / 1024);
    print("  Used: %d KB\n", status.usedMemory / 1024);
    print("  Free: %d KB\n", status.freeMemory / 1024);
    print("  Blocks: %d\n", status.allocatedBlocks);
    
    // Test 3: Free
    sys_free(ptr2);
    print("\nTest 3: Freed ptr2\n");
    
    // Test 4: Realloc en el espacio liberado
    void* ptr4 = sys_malloc(150);
    print("Test 4: Allocated 150 bytes at %p (should reuse freed space)\n", ptr4);
    
    // Liberar todo
    sys_free(ptr1);
    sys_free(ptr3);
    sys_free(ptr4);
    
    print("\nAll memory freed\n");
    status = sys_memStatus();
    print("Final - Used: %d bytes, Blocks: %d\n", 
          status.usedMemory, status.allocatedBlocks);
    
    print("=== Memory Manager Test Complete ===\n\n");
}

