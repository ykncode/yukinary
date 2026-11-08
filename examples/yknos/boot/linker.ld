/* ============================================================
 * YknOS Bare-Metal Kernel Linker Script
 * Load address: 1MB (0x00100000)
 * ============================================================ */

ENTRY(_start)

SECTIONS
{
    . = 1M;

    /* Multiboot header must be within the first 8 KB of the binary */
    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)
        *(.text)
    }

    /* Read-only data */
    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata)
    }

    /* Read-write initialized data */
    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
    }

    /* Read-write uninitialized data and stack */
    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }

    _kernel_end = .;
}

