GLOBAL cpuVendor
GLOBAL getTime
; GLOBAL get_pressed_key
GLOBAL snapshot
GLOBAL port_reader
GLOBAL port_writer
GLOBAL get_day
GLOBAL get_month
GLOBAL get_year
GLOBAL get_seconds
GLOBAL get_minutes
GLOBAL get_hour
GLOBAL set_timer_freq

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

set_timer_freq:
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

get_seconds:
	mov al, 0
	out 0x70, al
	in al, 0x71
	ret

get_minutes:
	mov al, 2
	out 0x70, al
	in al, 0x71
	ret

get_hour:
	mov al, 4
	out 0x70, al
	in al, 0x71
	ret

get_day:
	mov al, 7
	out 0x70, al
	in al, 0x71
	ret

get_month:
	mov al, 8
	out 0x70, al
	in al, 0x71
	ret

get_year:
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




