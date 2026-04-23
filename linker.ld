ENTRY(_start)

SECTIONS
{
    /* O Kernel começa no endereço 0x1000 */
    . = 0x1000;

    .text : ALIGN(4K)
    {
        kernel_entry.o(.text)
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