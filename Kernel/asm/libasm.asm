GLOBAL cpuVendor
GLOBAL getTime
; GLOBAL getPressedKey
GLOBAL snapshot
GLOBAL port_reader
GLOBAL port_writer
GLOBAL getDayOfMonth
GLOBAL getMonth
GLOBAL getYear
GLOBAL getSeconds
GLOBAL getMinutes
GLOBAL getHour
GLOBAL setTimerFreq

extern store_snapshot

section .text
	
cpuVendor:
	push rbp
	mov rbp, rsp

	push rbx

	mov rax, 0
	cpuid


	mov [rdi], ebx
	mov [rdi + 4], edx
	mov [rdi + 8], ecx

	mov byte [rdi+13], 0

	mov rax, rdi

	pop rbx

	mov rsp, rbp
	pop rbp
	ret

setTimerFreq:
    push rbp
    mov rbp, rsp
    
    ; Configurar el modo del timer (canal 0, modo 3 - square wave)
    mov al, 0x36    ; 00110110b - canal 0, lobyte/hibyte, modo 3
    out 0x43, al    ; puerto de comando del timer
    
    ; Enviar el divisor (lobyte primero, luego hibyte)
    mov ax, di      ; divisor en ax
    out 0x40, al    ; enviar lobyte al canal 0
    mov al, ah
    out 0x40, al    ; enviar hibyte al canal 0
    
    pop rbp
    ret

getSeconds:
	mov al, 0
	out 0x70, al
	in al, 0x71
	ret

getMinutes:
	mov al, 2
	out 0x70, al
	in al, 0x71
	ret

getHour:
	mov al, 4
	out 0x70, al
	in al, 0x71
	ret

getDayOfMonth:
	mov al, 7
	out 0x70, al
	in al, 0x71
	ret

getMonth:
	mov al, 8
	out 0x70, al
	in al, 0x71
	ret

getYear:
	mov al, 9
	out 0x70, al
	in al, 0x71
	ret

; Lee 1 byte de un puerto. Recibe:
; 	rdi = puerto (1er arg)
port_reader:
	push rbp
	mov rbp, rsp

	mov rdx, rdi ; 
	in al, dx ; al = 8 bits leídos del puerto

	mov rsp, rbp
	pop rbp
	ret

; Escribe 1 byte en un puerto. Recibe:
; 	rdi = puerto (1er arg)
; 	rsi = dato (2do arg)
port_writer:
	push rbp
	mov rbp, rsp 

	mov rax, rsi ; al = dato
	mov rdx, rdi  ; dx = puerto
	out dx, al 

	mov rsp, rbp
	pop rbp
	ret




