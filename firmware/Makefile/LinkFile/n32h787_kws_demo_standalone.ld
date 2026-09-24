/* Entry Point */
ENTRY(Reset_Handler)

/* NTFx CODE START */
/* Highest address of the user mode stack */
/* AXI SRAM1 is the always-present 128 KiB bank at 0x24000000.  TCM sizing
   changes the other banks, but never extends this contiguous region.  Reset
   code calls SystemInit before any application setup, so an out-of-range MSP
   here faults on that very first BL instruction. */
_estack = 0x24020000;
/* Generate a link error if heap and stack don't fit into RAM */
/* This image has no dynamic allocation. */
_Min_Heap_Size = 0x0;
_Min_Stack_Size = 0x1000;

/* Specify the memory areas */
MEMORY
{
/* Standalone layout: no USB-MSC bootloader.

   This is a byte-for-byte copy of h760_tf_CM7_boot.ld with the application
   moved down over the 0x15000000..0x15005fff window that the bootloader split
   reserved for itself.  Nothing else about the image changes -- same _estack,
   same ITCM code region, same MODEL_DTCM/DTCM arena placement, same .kws_audio
   DMA constraint -- so load addresses shift by exactly 0x6000 and the vectors
   land on the flash base the part boots from.

   Use it when the board has no bootloader, which is what the H787 on the bench
   turned out to be: 0x15000000 held a monolithic legacy image (vector table,
   Reset_Handler at 0x15000400, 632 KiB of TFLM/camera code) rather than the
   USB-MSC bootloader this project's Makefile assumes.  Flash fills the whole
   0x1e0000 LUN here; the boot linker script's 0x1da000 is that LUN minus the
   bootloader's 24 KiB.

   m7_flash_poll()'s boot_request path and OpenOCD's n32h7x_boot_usb both jump
   to the vector table at 0x15000000, which under this layout is this image
   itself: they restart the application instead of entering a bootloader.  The
   old 0x15080000 M4 boundary no longer applies: there is no second core. */
FLASH (rx)     : ORIGIN = 0x15000000, LENGTH = 0x1e0000
SHARED (rw)    : ORIGIN = 0x30000000, LENGTH = 0x400
/* I2S1 receive ring.  DMA cannot reach TCM on this part, and this bank is
   mapped Normal non-cacheable by m7_cache.c region 1, so the CPU and DMA
   agree on its contents without any cache maintenance.  Must stay below
   0x2401e000, where m7_cache.c region 2 switches the same SRAM back to
   write-back for the M7 -- DMA into a cached line corrupts silently, so
   .kws_audio carries an ASSERT against that boundary below. */
KWS_AUDIO (rw) : ORIGIN = 0x24000000, LENGTH = 0xC000
QuickCodes_SRAM (rx)     : ORIGIN = 0x400 , LENGTH = 0x3fc00
RW_SRAM (xrw)     : ORIGIN = 0x2401e000, LENGTH = 0x2000
MODEL_DTCM (r) : ORIGIN = 0x20000000, LENGTH = 0x20000
DTCM (rw)     : ORIGIN = 0x20020000, LENGTH = 0x60000
SDRAM (xrw)   : ORIGIN = 0xC0000000, LENGTH = 0x2000000
/* NTFx CODE END */

}

/* Define output sections */
SECTIONS
{
  .m4_shared (NOLOAD) : { KEEP(*(.m4_shared)) } >SHARED
  /* The startup code goes first into FLASH */
  .start_flash :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector)) /* Startup code */
    KEEP (*(.init))
    KEEP (*(.fini))
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)
    /* GCC emits startup_n32h73x_76x_gcc.o (lowercase); keep reset code in
       Flash so Reset_Handler is executable before RAM/ITCM is initialized. */
    *startup_n32h7*.o (.text .text.* .rodata .rodata.* .gnu.linkonce.t.* .gnu.linkonce.r.*)
    *system_n32h7xx.o (.text .text.* .rodata .rodata.* .gnu.linkonce.t.* .gnu.linkonce.r.*)
    . = ALIGN(4);
  } >FLASH

  /* Initialized QuickCodes sections goes into SRAM, load from FLASH */
 _siQuickCodes = LOADADDR(.QuickCodes);
  .QuickCodes :
  {
    . = ALIGN(4);
    _sQuickCodes = .;        /* create a global symbol at QuickCodes start */
    *(.text .text.*)          /* .text .text.* sections (code) */
    *(.rodata .rodata.* )        /* .rodata .rodata.*  sections (constants, strings, etc.) */
    *(EXCLUDE_FILE(*startup_n32h7*.o *system_n32h7xx.o ) .text .text.* .rodata .rodata.* .gnu.linkonce.t.* .gnu.linkonce.r.*)
    . = ALIGN(4);
    _eQuickCodes = .;        /* define a global symbols at end of QuickCodes */
  } >QuickCodes_SRAM AT> FLASH

  /* Flash holds only the load image.  TFLM reads the DTCM copy. */
  _sikws_model = LOADADDR(.kws_model);
  .kws_model :
  {
    . = ALIGN(16);
    _skws_model = .;
    KEEP(*(.kws_model))
    . = ALIGN(16);
    _ekws_model = .;
  } >MODEL_DTCM AT> FLASH

  /* startup_n32h78x_cm7_gcc.s is a vendor file shared with the bootloader
     image, so it still imports the person-detect names for the "copy the
     model to DTCM" and "zero the NOLOAD arena" loops.  Aliasing here keeps
     that file, bootloader.ld and m7_bench.ld untouched. */
  _sperson_model = _skws_model;
  _eperson_model = _ekws_model;
  _siperson_model = _sikws_model;

  ASSERT(_skws_model == ORIGIN(MODEL_DTCM), "Model must run from DTCM")
  ASSERT(_sikws_model >= ORIGIN(FLASH) &&
         _sikws_model + SIZEOF(.kws_model) <= ORIGIN(FLASH) + LENGTH(FLASH),
         "Model load image overflows application Flash")
  ASSERT(_ekws_model <= ORIGIN(DTCM), "Model overlaps inference workspace")
  /* Both .QuickCodes and .kws_model load from Flash and the model follows the
     ITCM image there, so this is the check that a growing frontend cannot
     overwrite the model's load image.  _eQuickCodes is a VMA symbol in ITCM
     and says nothing about the load image; the LMA end is the SIZEOF form. */
  ASSERT(_sikws_model >= _siQuickCodes + SIZEOF(.QuickCodes),
         "Model load image overlaps the QuickCodes load image")

  /* Tensor arena, TFLM object state and the KWS diagnostics array.  The reset
     code clears this whole range before __libc_init_array, so C++ objects
     placed here are constructed into zeroed memory. */
  .kws_arena (NOLOAD) :
  {
    . = ALIGN(16);
    _skws_arena = .;
    *(.kws_arena)
    *(.kws_state)
    *(.kws_diagnostics)
    . = ALIGN(16);
    _ekws_arena = .;
  } >DTCM

  _sperson_arena = _skws_arena;
  _eperson_arena = _ekws_arena;

  ASSERT(_ekws_arena <= ORIGIN(DTCM) + LENGTH(DTCM),
         "KWS arena exceeds the DTCM region")
  /* The upper half of DTCM from 0x20050000 is the FlashOS scratch window that
     firmware/openocd/n32h7x_cmsisdap.tcl stages the flash algorithm, its
     stack and its 62 KiB buffers in.  Overlapping it would not corrupt flash
     -- only this arena, which is rebuilt from the model on every boot -- but
     it would quietly make the flasher's layout depend on how big the arena
     happens to be, and the comment block in that script exists because two
     earlier versions of this window failed exactly that way.  Keep the two
     apart deliberately; move the FlashOS base if the arena ever needs to
     grow. */
  ASSERT(_ekws_arena <= 0x20050000,
         "KWS arena intrudes into the FlashOS scratch window at 0x20050000")

  /* I2S1 DMA ring and its link-list descriptors.  NOLOAD: the DMA engine fills
     the ring and the CPU writes the descriptors at runtime, so nothing is
     copied in.  The descriptors live here too rather than in .kws_arena
     because general-purpose DMA cannot read TCM on this part. */
  .kws_audio (NOLOAD) :
  {
    . = ALIGN(32);
    _skws_audio = .;
    *(.kws_audio)
    . = ALIGN(32);
    _ekws_audio = .;
  } >KWS_AUDIO

  /* 0x2401e000 is m7_cache.c region 2, which maps this SRAM write-back for
     the M7.  A DMA ring reaching into it would be cached and the CPU would
     read stale audio.  Keep the literal equal to that region's base. */
  ASSERT(ORIGIN(KWS_AUDIO) + SIZEOF(.kws_audio) <= 0x2401e000,
         "Audio ring overlaps the write-back MPU region")

  .ARM.extab   : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH

  .ARM : {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH
  .preinit_array     :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >FLASH
  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >FLASH
  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >FLASH

  /* used by the startup to initialize data */
  _sidata = LOADADDR(.data);

  /* Initialized data sections goes into RW_SRAM, load LMA copy after code */
  .data : /*可读写且需要初始化数据 */
  {
    . = ALIGN(4);
    _sdata = .;        /* create a global symbol at data start */
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */

    . = ALIGN(4);
    _edata = .;        /* define a global symbol at data end */
  } >RW_SRAM AT> FLASH


  /* Uninitialized data section */
  . = ALIGN(4);
  .bss :/*可读写的置零初始化数据  */
  {
    /* This is used by the startup in order to initialize the .bss secion */
    _sbss = .;         /* define a global symbol at bss start */
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)

    . = ALIGN(4);
    _ebss = .;         /* define a global symbol at bss end */
    __bss_end__ = _ebss;
  } >RW_SRAM

  /* User_heap_stack section, used to check that there is enough RAM left */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RW_SRAM



  /* Remove information from the standard libraries */
  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
