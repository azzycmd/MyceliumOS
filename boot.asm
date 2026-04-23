[bits 16]
[org 0x7c00]

inicio:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    mov [BOOT_DRIVE], dl

    ; Carregar o Kernel
    mov bx, 0x1000
    mov ah, 0x02
    mov al, 50        ; Leia 50 setores (aprox 25KB) para garantir espaço
    mov ch, 0
    mov dh, 0
    mov cl, 2         ; Começa no setor 2 (o 1 é o bootloader)
    int 0x13
    jc erro

    ; Pulo para Modo Protegido
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:init_pm

erro: jmp $

align 16
gdt_start:
    dq 0            ; Nulo
gdt_code:
    dw 0xffff, 0    ; Limite 0-15, Base 0-15
    db 0, 10011010b, 11001111b, 0 ; Base 16-23, Acesso, Flags, Base 24-31
gdt_data:
    dw 0xffff, 0    ; Limite 0-15, Base 0-15
    db 0, 10010010b, 11001111b, 0 ; Base 16-23, Acesso, Flags, Base 24-31
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

[bits 32]
init_pm:
    mov ax, 0x10      ; Seletor de dados da GDT
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ebp, 0x90000
    mov esp, ebp

    jmp 0x08:0x1000   ; Salto longo explícito para o Kernel

BOOT_DRIVE db 0
times 510-($-$$) db 0
dw 0xAA55