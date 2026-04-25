ENTRY(_start)

SECTIONS
{
    . = 1M; /* Endereço padrão para kernels Multiboot */

    .text : ALIGN(4K)
    {
        *(.multiboot) /* Garante que o cabeçalho do boot.asm seja o PRIMEIRO */
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