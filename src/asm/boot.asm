; =============================================================
; boot.asm - Multiboot2
; O bootloader carrega este kernel e entrega o framebuffer via Multiboot2.
; Mudanças em relação ao original:
;   - Solicitado framebuffer linear via Multiboot2 para UEFI/GOP
;   - GDT própria mantida para garantir segmentos corretos após handoff
;   - Stack alinhada em 16 bytes (requisito do ABI x86)
; =============================================================

bits 32

; --- Cabeçalho Multiboot 2 ---
section .multiboot_header
align 8
header_start:
    dd 0xe85250d6                               ; Magic Multiboot2
    dd 0                                        ; Arquitetura: i386 protected mode
    dd header_end - header_start                ; Tamanho do header
    dd 0x100000000 - (0xe85250d6 + 0 + (header_end - header_start)) ; Checksum

    ; --- Tag: Modulos alinhados em pagina ---
    align 8
    dw 6        ; Type: module alignment
    dw 0        ; Flags
    dd 8        ; Size

    ; --- Tag: Framebuffer ---
    align 8
    dw 5        ; Type: framebuffer
    dw 1        ; Flags: opcional
    dd 20       ; Size
    dd 1280     ; width
    dd 720      ; height
    dd 32       ; depth

    ; --- Tag de finalização (OBRIGATÓRIA) ---
    align 8
    dw 0
    dw 0
    dd 8
header_end:

; --- Ponto de entrada ---
section .text
global _start

bits 32
_start:
    cli

    ; Salva os registradores do Multiboot2 ANTES de qualquer coisa
    ; EAX = 0x36d76289 (magic), EBX = endereço da struct de info
    mov edi, eax    ; Salva magic em EDI (não é destruído pelo lgdt)
    mov esi, ebx    ; Salva ponteiro de info em ESI

    ; Desativa paginação (bit 31 do CR0), caso o GRUB tenha ativado
    mov eax, cr0
    and eax, 0x7FFFFFFF
    mov cr0, eax

    ; Carrega nossa GDT
    lgdt [gdt_descriptor]

    ; Far jump para recarregar CS com nosso seletor
    jmp CODE_SEG:reload_segments

reload_segments:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Stack alinhada em 16 bytes (requisito ABI)
    mov esp, stack_top
    and esp, 0xFFFFFFF0

    ; Passa os argumentos para kernel_main(uint32_t magic, uint32_t info)
    ; Convenção cdecl: argumentos na stack da direita para esquerda
    push esi    ; arg2: endereço da struct multiboot_info
    push edi    ; arg1: magic number

    extern kernel_main
    call kernel_main

    ; Se kernel_main retornar (não deveria), trava aqui
    cli
hang:
    hlt
    jmp hang

; --- GDT ---
section .data
align 8
gdt_start:
    dq 0x0              ; Descritor nulo (obrigatório)

gdt_code:
    dw 0xFFFF           ; Limit [0:15]
    dw 0x0000           ; Base  [0:15]
    db 0x00             ; Base  [16:23]
    db 10011010b        ; Access: presente, ring0, código, exec/read
    db 11001111b        ; Flags: 4KB granularidade, 32-bit + Limit [16:19]
    db 0x00             ; Base  [24:31]

gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b        ; Access: presente, ring0, dados, read/write
    db 11001111b
    db 0x00
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start
    dd 0

CODE_SEG equ gdt_code - gdt_start   ; = 0x08
DATA_SEG equ gdt_data - gdt_start   ; = 0x10

; --- Stack ---
section .bss
align 16
stack_bottom:
    resb 16384      ; 16 KB
stack_top:
