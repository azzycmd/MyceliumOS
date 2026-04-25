bits 32

; --- IMPORTANTE: Nomes que o Linker vai procurar no C ---
extern teclado        ; Certifique-se que no seu kbd.c/kernel.c a função é: void teclado()
extern timer_handler  ; Certifique-se que no C a função é: void timer_handler()

; --- Nomes que o seu kernel.c vai procurar para a IDT ---
global keyboard_wrapper
global timer_wrapper

section .text

keyboard_wrapper:
    pusha
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10      ; Carrega o seletor de dados da GDT
    mov ds, ax
    mov es, ax

    call teclado      ; Chama a função C

    pop gs
    pop fs
    pop es
    pop ds
    popa
    iret

timer_wrapper:
    pusha
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10
    mov ds, ax
    mov es, ax

    call timer_handler

    pop gs
    pop fs
    pop es
    pop ds
    popa
    iret