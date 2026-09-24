ENTRY(Reset_Handler)
_estack = 0x20020000;
MEMORY
{
  FLASH (rx) : ORIGIN = 0x15000000, LENGTH = 512K
  ITCM (rx) : ORIGIN = 0x400, LENGTH = 0x3fc00
  /* Keep stack spills and control metadata off the SRAM under test. */
  RAM (rw) : ORIGIN = 0x2001c000, LENGTH = 16K
  DTCM_TEST (rw) : ORIGIN = 0x20000000, LENGTH = 64K
  AXI_TEST (rw) : ORIGIN = 0x24000000, LENGTH = 64K
  AHB_TEST (rw) : ORIGIN = 0x30000000, LENGTH = 64K
}
SECTIONS
{
  .start_flash :
  {
    KEEP(*(.isr_vector))
    KEEP(*(.init))
    KEEP(*(.fini))
    *startup_n32h7*.o(.text .text.* .rodata .rodata.*)
    *system_n32h7xx.o(.text .text.* .rodata .rodata.*)
    . = ALIGN(4);
  } >FLASH
  .QuickCodes :
  {
    _sQuickCodes = .;
    *(.text .text.* .rodata .rodata.*)
    . = ALIGN(4);
    _eQuickCodes = .;
  } >ITCM AT>FLASH
  _siQuickCodes = LOADADDR(.QuickCodes);
  .ARM.extab : { *(.ARM.extab*) } >FLASH
  .ARM.exidx :
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH
  .preinit_array :
  {
    __preinit_array_start = .;
    KEEP(*(.preinit_array*))
    __preinit_array_end = .;
  } >FLASH
  .init_array :
  {
    __init_array_start = .;
    KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array))
    __init_array_end = .;
  } >FLASH
  .fini_array :
  {
    __fini_array_start = .;
    KEEP(*(SORT(.fini_array.*))) KEEP(*(.fini_array))
    __fini_array_end = .;
  } >FLASH
  .data :
  {
    . = ALIGN(4); _sdata = .;
    *(.data .data.*)
    . = ALIGN(4); _edata = .;
  } >RAM AT>FLASH
  _sidata = LOADADDR(.data);
  .bss (NOLOAD) :
  {
    . = ALIGN(8); _sbss = .; __bss_start__ = .;
    *(.bss .bss.* COMMON)
    . = ALIGN(8); _ebss = .; __bss_end__ = .;
    _end = .; end = .;
  } >RAM
  ASSERT(_ebss + 8192 <= _estack, "benchmark stack overlaps globals")
  .bench_dtcm (NOLOAD) : { KEEP(*(.bench_dtcm)) } >DTCM_TEST
  .bench_axi (NOLOAD) : { KEEP(*(.bench_axi)) } >AXI_TEST
  .bench_ahb (NOLOAD) : { KEEP(*(.bench_ahb)) } >AHB_TEST
  /* Empty camera/model sections for the shared startup contract. */
  _sperson_model = 0; _eperson_model = 0; _siperson_model = 0;
  _sperson_arena = 0; _eperson_arena = 0;
  .ARM.attributes 0 : { *(.ARM.attributes) }
}
