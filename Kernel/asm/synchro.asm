; sync.asm

GLOBAL acquire_lock
GLOBAL release_lock

section .text
; void acquire(lock_t *lock)
acquire_lock:
    mov al, 0
.retry:
    xchg [rdi], al
    test al, al
    jz .retry
    ret

; void release(lock_t *lock)
release_lock:
    mov byte [rdi], 1
    ret