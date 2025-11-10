#ifndef INTERRUPS_H_
#define INTERRUPTS_H_

// Exposed by interrupts.asm
extern void *current_kernel_rsp;
extern void *switch_to_rsp;
extern void *syscall_frame_ptr; // RSP of shell at time of syscall

#endif /* INTERRUPS_H_ */

#ifndef INTERRUPS_H_
#define INTERRUPS_H_

#include <idt_loader.h>

void _irq00Handler(void);
void _irq01Handler(void);
void _irq02Handler(void);
void _irq03Handler(void);
void _irq04Handler(void);
void _irq05Handler(void);
void _irq128Handler(void);

void _exception0Handler(void);
void _exception6Handler(void);

void _cli(void);

void _sti(void);

void _hlt(void);

void picMasterMask(uint8_t mask);

void picSlaveMask(uint8_t mask);

// Termina la ejecución de la cpu.
void haltcpu(void);

#endif 
