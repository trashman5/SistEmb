

/* ld script for StarterWare AM335x */

/*
** The stack is kept at end of the image, and its size is 128 MB.
** The heap section is placed above the stack to support I/O
** operations using semihosting. The size of the section is 2KB.
*/

MEMORY
{
    DDR0 :   o = 0x80000000,  l = 0x10000000  /* 256MB external DDR Bank 0 */
}

OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)

ENTRY(Entry)

SECTIONS
{

        .startcode :
        {
            . = ALIGN(4);
            *init.o      (.text)
        } >DDR0

        .text :
        {
            . = ALIGN(4);
            *(.text*)
            *(.rodata*)
        } >DDR0

        .data :
        {
            . = ALIGN(4);
            *(.data*)
        } >DDR0

        .bss :
        {
            . = ALIGN(4);
            _bss_start = .;
            __bss_start__ = _bss_start;
            *(.bss*)
            *(COMMON)
            _bss_end = .;
            __bss_end__ = _bss_end;

        } >DDR0

        .heap :
        {
            . = ALIGN(4);
            __end__ = .;
            end = __end__;
            __HeapBase = __end__;
            *(.heap*)
            . = . + 0x800;
            __HeapLimit = .;
        } >DDR0

        .stack :
        {
            . = ALIGN(4);
            __StackLimit = . ;
            *(.stack*)
            . = . + 0x7FFFFF8;
            __StackTop = .;
        } >DDR0
        _stack = __StackTop;

}

