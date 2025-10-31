
GLOBAL _cli
GLOBAL _sti
GLOBAL picMasterMask
GLOBAL picSlaveMask
GLOBAL haltcpu
GLOBAL _hlt

GLOBAL _irq00Handler
GLOBAL _irq01Handler
GLOBAL _irq02Handler
GLOBAL _irq03Handler
GLOBAL _irq04Handler
GLOBAL _irq05Handler
GLOBAL _irq128Handler

GLOBAL _exception0Handler
GLOBAL _exception6Handler

GLOBAL getPressedKey
GLOBAL reg_array ; array donde se almacenan los registros cunado se toco ctrl

GLOBAL setup_initial_stack 

GLOBAL timer_tick ; simula un tick del timer

EXTERN irqDispatcher
EXTERN exceptionDispatcher
EXTERN syscalls
EXTERN current_kernel_rsp
EXTERN switch_to_rsp
EXTERN printRegisters
EXTERN getStackBase
EXTERN main

SECTION .text

%macro pushState 0
	push rbx
	push rcx
	push rdx
	push rbp
	push rdi
	push rsi
	push r8
	push r9
	push r10
	push r11
	push r12
	push r13
	push r14
	push r15
	push rax
%endmacro

%macro popState 0
	pop rax
	pop r15
	pop r14
	pop r13
	pop r12
	pop r11
	pop r10
	pop r9
	pop r8
	pop rsi
	pop rdi
	pop rbp
	pop rdx
	pop rcx
	pop rbx
%endmacro

%macro irqHandlerMaster 1
	pushState

	mov rdi, %1 ; pasaje de parametro
	mov rsi, rsp
	call irqDispatcher
	mov rsp, rax

	; EOI
	mov al, 20h
	out 20h, al

	popState
	iretq
%endmacro

%macro exceptionHandler 1
	pushState

	call printRegisters

	mov rdi, %1 ; pasaje de parametro
	call exceptionDispatcher

	popState ; vuelvo a tener en [rsp] los registros que me pusheo en el stack la interrupción
	call getStackBase	        
	mov qword [rsp+8*3], rax				
	mov qword [rsp], userland
	
	iretq
%endmacro

getPressedKey:
	mov rax, [pressed_key]
	ret

_hlt:
	sti
	hlt
	ret

_cli:
	cli
	ret


_sti:
	sti
	ret

picMasterMask:
	push rbp
    mov rbp, rsp
    mov ax, di
    out	21h,al
    pop rbp
    retn

picSlaveMask:
	push    rbp
    mov     rbp, rsp
    mov     ax, di  ; ax = mascara de 16 bits
    out	0A1h,al
    pop     rbp
    retn


;8254 Timer (Timer Tick)
_irq00Handler:
	irqHandlerMaster 0

;Keyboard
_irq01Handler:
	push rax
	xor rax, rax
	in al, 0x60 ; guardo la tecla
	mov [pressed_key], rax
	cmp rax, SNAPSHOT_KEY
	jne .doNotCapture

	pop rax
	mov [reg_array + 0*8],  rax
	mov [reg_array + 1*8],  rbx
	mov [reg_array + 2*8],  rcx
	mov [reg_array + 3*8],  rdx
	mov [reg_array + 4*8],  rbp
	mov [reg_array + 5*8],  rdi
	mov [reg_array + 6*8],  rsi
	mov [reg_array + 7*8],  r8
	mov [reg_array + 8*8],  r9
	mov [reg_array + 9*8], r10
	mov [reg_array + 10*8], r11
	mov [reg_array + 11*8], r12
	mov [reg_array + 12*8], r13
	mov [reg_array + 13*8], r14
	mov [reg_array + 14*8], r15
	mov rax, [rsp+8*0] ; rip
	mov [reg_array + 15*8], rax
	mov rax, [rsp+8*1] ; cs
	mov [reg_array + 16*8], rax
	mov rax, [rsp+8*2] ; rflags
	mov [reg_array + 17*8], rax
	mov rax, [rsp+8*3] ; rsp
	mov [reg_array + 18*8], rax
	mov rax, [rsp+8*4] ; ss
	mov [reg_array + 19*8], rax
	jmp .continue

.doNotCapture:
	pop rax

.continue:
	irqHandlerMaster 1




;Cascade pic never called
_irq02Handler:
	irqHandlerMaster 2

;Serial Port 2 and 4
_irq03Handler:
	irqHandlerMaster 3

;Serial Port 1 and 3
_irq04Handler:
	irqHandlerMaster 4

;USB
_irq05Handler:
	irqHandlerMaster 5

; antes validabamos que el rax sea de una syscall por si habia un 
; index out of bounds
; lo saque por ahora 
; TODO: evaluar si lo queremos volver a poner
_irq128Handler:
	pushState
    call [syscalls + rax * 8] ; llamamos a la syscall
    mov [aux], rax ; preservamos el valor de retorno de la syscall
    popState
    mov rax, [aux]
    iretq



; rdi = caller, rsi = pid, rdx = stack_pointer, rcx
setup_initial_stack:
	mov r8, rsp        ; 1) Guarda el rsp del llamador (kernel actual)
	mov r9, rbp        ;    y rbp para restaurarlos al final
	mov rsp, rdx       ; 2) Cambia a la pila del nuevo proceso (rdx = stack_pointer)
	mov rbp, rdx
	push 0x0           ; 3) Empuja SS (placeholder) para el iretq
	push rdx           ; 4) Empuja RSP que verá el iretq tras popState
	push 0x202         ; 5) Empuja RFLAGS con IF=1 para habilitar IRQs
	push 0x8           ; 6) Empuja CS (selector de código)
	push rdi           ; 7) Empuja RIP = caller
	mov rdi, rsi       ; 8) Prepara 1er arg para caller: rdi = pid
	pushState          ; 10) Simula registros salvados por ISR
	mov rax, rsp       ; 11) Devuelve a C el RSP armado
	mov rsp, r8        ; 12) Restaura pila original del llamador
	mov rbp, r9
	ret                ; 13) Retorna a C con rax = nuevo RSP


timer_tick: 
	int 20h
	ret


;Zero Division Exception
_exception0Handler:
	exceptionHandler 0


; Invalid Operand Exception
_exception6Handler: 
	exceptionHandler 6

haltcpu:
	cli
	hlt
	ret

SECTION .data 
	userland equ 0x400000 

SECTION .bss
	aux resq 1
	pressed_key resq 1
	reg_array resq 20 ; 20 registros
	SNAPSHOT_KEY equ 0x1D ; Left Ctrl key scancode
	syscall_frame_ptr resq 1
	syscall_id_tmp   resq 1