[bits 32]

extern teclado ; A função C que você já criou
global keyboard_wrapper

keyboard_wrapper:
    pusha
    call teclado
    popa
    iret
extern timer_handler
global timer_wrapper

timer_wrapper:
    pusha          ; Salva todos os registradores (EAX, ECX, etc)
    call timer_handler
    popa           ; Restaura tudo
    iret           ; Interrupt Return (essencial para voltar ao kernel)