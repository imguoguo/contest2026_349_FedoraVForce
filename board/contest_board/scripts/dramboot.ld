/****************************************************************************
 * vendor/openvela/boards/contest2026_349_board/scripts/dramboot.ld
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ****************************************************************************/

OUTPUT_ARCH(aarch64)
ENTRY(__start)

PHDRS
{
  text PT_LOAD;
}

SECTIONS
{
  /*
   * U-Boot's ARM64 booti path loads the Image at 0x03000000 and, because
   * the Image header has a 0x480000 text offset, relocates its entry to
   * 0x03080000.  Link the image at that final address so absolute symbols
   * and function pointers refer to the code that is actually executed.
   */
  . = 0x03080000;
  _start = .;

  .text :
  {
    _stext = .;
    *(.start .start.*)
    *(.text .text.*)
    *(.text.cold)
    *(.text.unlikely)
    *(.fixup)
    *(.gnu.warning)
  } :text = 0x9090

  . = ALIGN(4096);

  .init_section :
  {
    _sinit = ABSOLUTE(.);
    KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
    KEEP(*(.init_array .ctors))
    _einit = ABSOLUTE(.);
  } :text

  . = ALIGN(4096);

  .vector :
  {
    _vector_start = .;
    KEEP(*(.exc_vector_table))
    KEEP(*(.exc_vector_table.*))
    KEEP(*(.vectors))
    _vector_end = .;
  } :text

  . = ALIGN(4096);
  _etext = .;
  _sztext = _etext - _stext;

  . = ALIGN(4096);

  .rodata :
  {
    _srodata = .;
    *(.rodata .rodata.*)
    *(.data.rel.ro .data.rel.ro.*)
  } :text

  . = ALIGN(4096);
  _erodata = .;
  _szrodata = _erodata - _srodata;
  _eronly = .;

  . = ALIGN(4096);

  .data :
  {
    _sdata = .;
    *(.data.page_aligned)
    *(.data .data.*)
    . = ALIGN(8);
    *(.data.rel .data.rel.*)
    CONSTRUCTORS
  } :text
  _edata = .;

  .bss :
  {
    . = ALIGN(8);
    _sbss = .;
    *(.bss .bss.*)
    *(COMMON)
    . = ALIGN(8);
  } :text

  . = ALIGN(4096);
  _ebss = .;
  _szbss = _ebss - _sbss;

  .initstack :
  {
    _s_initstack = .;
    *(.initstack)
    . = ALIGN(16);
  } :text

  . = ALIGN(4096);
  _szdata = . - _sdata;

  /* DMA memory that s6_boot.c maps non-cacheable.  It is left out of
   * _szdata, which the common MMU code maps cacheable after the board
   * regions.
   */

  .noncache (NOLOAD) :
  {
    _snoncache = .;
    *(.noncache .noncache.*)
    . = ALIGN(4096);
    _enoncache = .;
  } :text

  _sznoncache = _enoncache - _snoncache;

  _e_initstack = .;
  g_idle_topstack = .;

  /DISCARD/ :
  {
    *(.exit.text)
    *(.exit.data)
    *(.exitcall.exit)
    *(.eh_frame)
  }

  .stab 0 : { *(.stab) }
  .stabstr 0 : { *(.stabstr) }
  .comment 0 : { *(.comment) }
}
