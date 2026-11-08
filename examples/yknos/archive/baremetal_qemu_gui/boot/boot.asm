; ============================================================
; YknOS Bare-Metal Multiboot1 / Multiboot2 Bootloader Entry
; Architecture: x86 (32-bit protected mode transitioning to 64-bit long mode)
; ============================================================

MBALIGN     equ  1 << 0             ; Align loaded modules on page boundaries
MEMINFO     equ  1 << 1             ; Provide memory map
FLAGS       equ  MBALIGN | MEMINFO  ; Multiboot flag field
MAGIC       equ  0x1BADB002         ; Multiboot 1 Magic number
CHECKSUM    equ -(MAGIC + FLAGS)    ; Checksum (magic + flags + checksum must = 0)

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

section .bss
align 16
stack_bottom:
    resb 32768                      ; 32 KB Kernel stack
stack_top:

section .text
global _start:function (_start.end - _start)
extern kmain

_start:
    ; Initialize stack pointer to top of reserved stack
    mov esp, stack_top

    ; Reset EFLAGS
    push 0
    popf

    ; Push Multiboot parameters (magic in EAX, info struct in EBX)
    push eax
    push ebx

    ; Call Yukinary Kernel Main entry point
    call kmain

    ; If kmain returns, disable interrupts and halt CPU
    cli
.hang:
    hlt
    jmp .hang
.end:

