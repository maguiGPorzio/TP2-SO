GLOBAL printRegisters
GLOBAL returnToUserland
EXTERN print
EXTERN printRegisterFormat
EXTERN newLine
EXTERN main
EXTERN getStackBase
EXTERN vdSetTextSize
EXTERN vdPrint
EXTERN uint64ToRegisterFormat
section .text


printRegisters:
    push rbp
    mov rbp, rsp

    xor r10, r10 ; r10 = contador inicializado en 0

.loop_registers:
    
    mov rdi, 1
    call vdSetTextSize

    mov rdi, [registers+r10] ; carga el string "RAX = ", "RBX = ", etc
    mov rsi, 0xFF0000 ; color rojo
    call vdPrint

    add r10, 8 ; avanza al siguiente índice
    mov rdi, [rbp+r10+8] ; valor del registro sacado del stack. Los registros están almacenados en el stack automáticamente cuando ocurre una excepción
    mov rsi, buffReg
    call uint64ToRegisterFormat ; deja en buffReg el valor del registro en el formato correcto

    mov rdi, buffReg
    mov rsi, 0xFF0000 
    call vdPrint


    call newLine

    cmp r10, length
    jne .loop_registers

    mov rsp, rbp
    pop rbp
    ret

section .data
    segmentSS db " SS = ", 0
    rflags db " RFLAGS = ", 0
    segmentCS db " CS = ", 0
    registerRIP db " RIP = ", 0
    registerRAX db " RAX = ", 0
    registerRBX db " RBX = ", 0
    registerRCX db " RCX = ", 0
    registerRDX db " RDX = ", 0
    registerRBP db " RBP = ", 0
    registerRDI db " RDI = ", 0
    registerRSI db " RSI = ", 0
    registerRSP db " RSP = ", 0
    registerR8 db  " R8 = ", 0
    registerR9 db  " R9 = ", 0
    registerR10 db " R10 = ", 0
    registerR11 db " R11 = ", 0
    registerR12 db " R12 = ", 0
    registerR13 db " R13 = ", 0
    registerR14 db " R14 = ", 0
    registerR15 db " R15 = ", 0
    registers dq  registerRAX, registerR15, registerR14, registerR13, registerR12, registerR11, registerR10,registerR9, registerR8, registerRSI, registerRDI, registerRBP, registerRDX, registerRCX, registerRBX, registerRIP, segmentCS, rflags, registerRSP, segmentSS
    length equ $-registers

section .bss 
    buffReg resb 17