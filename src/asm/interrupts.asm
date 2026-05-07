bits 32

; --- IMPORTANTE: Nomes que o Linker vai procurar no C ---
extern teclado        ; Certifique-se que no seu kbd.c/kernel.c a função é: void teclado()
extern timer_handler  ; Certifique-se que no C a função é: void timer_handler()
extern mouse_handler
extern exception_handler

; --- Nomes que o seu kernel.c vai procurar para a IDT ---
global keyboard_wrapper
global timer_wrapper
global mouse_wrapper

%macro ISR_NOERR 1
global isr%1
isr%1:
    cli
    push dword 0
    push dword %1
    jmp exception_wrapper
%endmacro

%macro ISR_ERR 1
global isr%1
isr%1:
    cli
    push dword %1
    jmp exception_wrapper
%endmacro

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR   29
ISR_ERR   30
ISR_NOERR 31

section .text

exception_wrapper:
    pusha
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10
    mov ds, ax
    mov es, ax

    push dword [esp + 64] ; eflags
    push dword [esp + 64] ; cs
    push dword [esp + 64] ; eip
    push dword [esp + 64] ; erro
    push dword [esp + 64] ; vetor
    call exception_handler
    add esp, 20

    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8
    iret

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

mouse_wrapper:
    pusha
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10
    mov ds, ax
    mov es, ax

    call mouse_handler

    pop gs
    pop fs
    pop es
    pop ds
    popa
    iret
