bits 32

; --- Multiboot Header ---
MB_MAGIC    equ 0x1BADB002
MB_FLAGS    equ 1 << 0 | 1 << 1
MB_CHECKSUM equ -(MB_MAGIC + MB_FLAGS)

section .multiboot
    align 4
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM

; --- Código do Boot ---
section .text
    global _start
    extern main

_start:
    mov esp, stack_top

    ; 2. Carregar a GDT para evitar Triple Faults
    lgdt [gdt_descriptor]
    jmp 0x08:.reload_cs    ; Salto para atualizar o CS (Code Segment)

.reload_cs:
    mov ax, 0x10           ; Seletor de dados (Data Segment)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; 3. Chamar o kernel em C
    call main

halt:
    cli
    hlt
    jmp halt

; --- Dados da GDT ---
section .data
align 4
gdt_start:
    dq 0x0                ; Nulo
gdt_code:                 ; 0x08
    dw 0xFFFF, 0x0000
    db 0x00, 0x9A, 0xCF, 0x00
gdt_data:                 ; 0x10
    dw 0xFFFF, 0x0000
    db 0x00, 0x92, 0xCF, 0x00
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

; --- Pilha do Sistema ---
section .bss
align 16
stack_bottom:
    resb 16384 ; 16 KB para a stack
stack_top: