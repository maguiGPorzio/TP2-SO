global  sys_write, sys_read, sys_date, sys_time, sys_regs, sys_clear
global  sys_increase_fontsize, sys_decrease_fontsize, sys_beep
global  sys_screen_size, sys_circle, sys_rectangle, sys_line, sys_draw_string
global  sys_enable_textmode, sys_disable_textmode, sys_put_pixel, sys_key_status
global  sys_sleep, sys_clear_input_buffer, sys_ticks
global  sys_malloc, sys_free, sys_memstatus
; Process/syscalls (scheduler-backed)
global  sys_create_process, sys_exit_current, sys_getpid, sys_kill, sys_block, sys_unblock, sys_wait, sys_nice, sys_processes_info, sys_yield
global sys_sem_open,sys_sem_close,sys_sem_wait,sys_sem_post
global sys_create_pipe, sys_destroy_pipe, sys_open_named_pipe, sys_close_fd, sys_list_pipes
global sys_set_foreground_process, sys_adopt_init_as_parent, sys_get_foreground_process
global generate_invalid_opcode
global printf
global scanf
extern printf_aux
extern scanf_aux

; macro para syscalls
%macro SYSCALL 1
	mov     rax, %1
	int     0x80
	ret
%endmacro

section .text 
; 0 – void regs(char* buf);
sys_regs:
	SYSCALL 0

; 1 – void time(uint8_t* buf);
sys_time:
	SYSCALL 1

; 2 – void date(uint8_t* buf);
sys_date:
	SYSCALL 2

; 3 – uint64_t read(int fd, char* buf, uint64_t count);
sys_read:
	SYSCALL 3
		
; 4 – uint64_t write(uint64_t fd, const char* buf, uint64_t count);
sys_write:
	SYSCALL 4

; 5 – void increase_fontsize(void);
sys_increase_fontsize:
	SYSCALL 5

; 6 – void decrease_fontsize(void);
sys_decrease_fontsize:
	SYSCALL 6

; 7 – void beep(uint32_t freq, uint64_t dur);
sys_beep:
	SYSCALL 7

; 8 – void screen_size(uint32_t* w, uint32_t* h);
sys_screen_size:
	SYSCALL 8

; 9 – void circle(uint64_t fill, uint64_t* info, uint32_t color);
sys_circle:
	SYSCALL 9

;10 – void rectangle(uint64_t fill, uint64_t* info, uint32_t color);
sys_rectangle:
	SYSCALL 10

;11 – void line(uint64_t* info, uint32_t color);
sys_line:
	SYSCALL 11

;12 – void draw_string(const char* s, uint64_t* info, uint32_t color);
sys_draw_string:
	SYSCALL 12

; 13 – void clear(void);
sys_clear:
	SYSCALL 13

; 14 - void speaker_start(uint32_t freq_hz);
sys_speaker_start:
	SYSCALL 14

; 15 - void speaker_stop();
sys_speaker_stop:
	SYSCALL 15

; 16 - void sys_textmode()
sys_enable_textmode:
	SYSCALL 16

; 17 - void sys_videomode()
sys_disable_textmode:
	SYSCALL 17

; 18 - static void sys_putpixel(uint32_t hexColor, uint64_t x, uint64_t y)
sys_put_pixel:
	SYSCALL 18

; 19 – uint64_t key_status(char);
sys_key_status:
	SYSCALL 19

; 20 - void sys_sleep(uint64_t miliseconds)
sys_sleep:
	SYSCALL 20

; 21 - void sys_clear_input_buffer()
sys_clear_input_buffer:
	SYSCALL 21

; 22 - void sys_clear_input_buffer
sys_ticks:
	SYSCALL 22

; 23 - void * sys_malloc(size_t size)
sys_malloc:
    SYSCALL 23

; 24 - void sys_free(void * ptr)
sys_free:
    SYSCALL 24

; 25 - MemStatus sys_memstatus(void)
sys_memstatus:
    SYSCALL 25

; 26 - int64_t sys_create_process(void *entry, int argc, const char **argv, const char *name)
sys_create_process:
    SYSCALL 26

; 27 - void sys_exit_current(int status), NO SE USA POR AHORA
sys_exit:
    SYSCALL 27

; 28 - int64_t sys_getpid(void)
sys_getpid:
    SYSCALL 28

; 29 - int64_t sys_kill(int pid)
sys_kill:
    SYSCALL 29

; 30 - int64_t sys_block(int pid)
sys_block:
    SYSCALL 30

; 31 - int64_t sys_unblock(int pid)
sys_unblock:
    SYSCALL 31

; 32 - int64_t sys_wait(int pid)
sys_wait:
    SYSCALL 32

; 33 - void sys_nice(void)
sys_nice:
    SYSCALL 33

; 34 - void sys_yield()
sys_yield:
    SYSCALL 34

; 35 - int sys_processes_info(process_info_t * buf, int max_count)
sys_processes_info:
    SYSCALL 35

; 36 - int64_t sys_sem_open(const char *name, int value);
sys_sem_open:
    SYSCALL 36

; 37 - void sys_sem_close(int sem_id);
sys_sem_close:
    SYSCALL 37

; 38 - void sys_sem_wait(int sem_id);
sys_sem_wait:
    SYSCALL 38

; 39 - void sys_sem_post(int sem_id);
sys_sem_post:
    SYSCALL 39

; 40 - int sys_create_pipe(int fds[2])
sys_create_pipe:
    SYSCALL 40

; 41 - void sys_destroy_pipe(int fd)
sys_destroy_pipe:
    SYSCALL 41

; 42 - int sys_set_foreground_process(int pid)
sys_set_foreground_process:
    SYSCALL 42

; 43 - int sys_adopt_init_as_parent(int pid)
sys_adopt_init_as_parent:
    SYSCALL 43

; 44 - int sys_get_foreground_process(void)
sys_get_foreground_process:
    SYSCALL 44


; 45 - int sys_open_named_pipe(char * name, int fds[2]);
sys_open_named_pipe: 
    SYSCALL 45

; 46 - int sys_close_fd(int fd);
sys_close_fd:
    SYSCALL 46

; 47 - void sys_list_pipes(void);
sys_list_pipes:
    SYSCALL 47

generate_invalid_opcode:
    ud2         ; Genera excepción de opcode inválido
    ret

printf:
    push rbp
    mov  rbp, rsp

    ; Guarda los 5 registros variables en un array local args[5] que creamos arriba de la pila
    sub  rsp, 104               ; 13 × 8 bytes (5 punteros/enteros, etc y 8 para flotantes)
    mov  [rsp+8*0], rsi        ; args[0] = 2.º parámetro real
    mov  [rsp+8*1], rdx
    mov  [rsp+8*2], rcx
    mov  [rsp+8*3], r8
    mov  [rsp+8*4], r9
    
    ; Guardar argumentos floats/double (primeros 2 registros XMM)
    movsd [rsp+8*5], xmm0      ; float_args[0]
    movsd [rsp+8*6], xmm1      ; float_args[1]
    movsd [rsp+8*7], xmm2      ; float_args[2]
    movsd [rsp+8*8], xmm3      ; float_args[3]
    movsd [rsp+8*9], xmm4      ; float_args[4]
    movsd [rsp+8*10], xmm5      ; float_args[5]
    movsd [rsp+8*11], xmm6      ; float_args[6]
    movsd [rsp+8*12], xmm7      ; float_args[7]

    ; Argumentos para printf_aux
    ; 1er argumento de printf_aux: rdi ya tenía *fmt desde el principio
    lea  rsi, [rsp]            ; 2do argumento de printf_aux: rsi = &args[0] (dirección del primer elemento del arreglo que se creo en el stack con los registros) 
    lea  rdx, [rbp+16]         ; 3er argumento de printf_aux: rdx = puntero al 1er arg en stack (los argumentos que no entraron en los registros para pasarse a printf)
    lea  rcx, [rsp+8*5]        ; 4to argumento: rcx = &float_args[0]
    call printf_aux         ; kvprintf_core(fmt, regArgs, stackPtr, floatArgs)

    leave
    ret

scanf:
    push rbp
    mov  rbp, rsp

    ; Guarda los 5 registros variables en un array local args[5] que creamos arriba de la pila
    sub  rsp, 40               ; 5 × 8 bytes 
    mov  [rsp+8*0], rsi        ; args[0] = 2.º parámetro real
    mov  [rsp+8*1], rdx
    mov  [rsp+8*2], rcx
    mov  [rsp+8*3], r8
    mov  [rsp+8*4], r9

    ; Argumentos para printf_aux
    ; 1er argumento de printf_aux: rdi ya tenía *fmt desde el principio
    lea  rsi, [rsp]            ; 2do argumento de printf_aux: rsi = &args[0] (dirección del primer elemento del arreglo que se creo en el stack con los registros) 
    lea  rdx, [rbp+16]         ; 3er argumento de printf_aux: rdx = puntero al 1er arg en stack (los argumentos que no entraron en los registros para pasarse a printf)
    call scanf_aux         ; kvprintf_core(fmt, regArgs, stackPtr, floatArgs)

    leave
    ret
