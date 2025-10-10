save_context:
    mov [rdi], rsp
    mov [rsi], rbp
    mov rax, [rsp]
    mov [rdx], rax
    ret

restore_context:
    mov rsp, rdi
    mov rbp, rsi
    jmp rdx