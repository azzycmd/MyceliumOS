ENTRY(_start)

SECTIONS
{
    . = 1M; /* O GRUB carrega o kernel aqui */

    .text : ALIGN(4K)
    {
        *(.multiboot) /* Garante que o header do boot.asm fique no início */
        *(.text)
    }

    .rodata : ALIGN(4K)
    {
        *(.rodata)
    }

    .data : ALIGN(4K)
    {
        *(.data)
    }

    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }
}