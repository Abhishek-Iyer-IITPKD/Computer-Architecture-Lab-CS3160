OUTPUT_ARCH("riscv")
ENTRY(_start)

/*----------------------------------------------------------------------*/
/* Sections                                                             */
/*----------------------------------------------------------------------*/

SECTIONS
{
  /* code section: RX */
  . = 0x80000000;
  .text.init : { *(.text.init) } :text_seg

  . = ALIGN(0x1000);
  .tohost : { *(.tohost) } :text_seg

  . = ALIGN(0x1000);
  .text : { *(.text) } :text_seg

  /* data section: RW */
  . = 0x80008000;
  .data : { *(.data) } :data_seg

  .sdata : {
    __global_pointer$ = . + 0x800;
    *(.srodata.cst16) *(.srodata.cst8) *(.srodata.cst4) *(.srodata.cst2) *(.srodata*)
    *(.sdata .sdata.* .gnu.linkonce.s.*)
  } :data_seg

  .sbss : {
    *(.sbss .sbss.* .gnu.linkonce.sb.*)
    *(.scommon)
  } :data_seg

  .bss : { *(.bss) } :data_seg

  .tdata :
  {
    _tdata_begin = .;
    *(.tdata)
    _tdata_end = .;
  } :data_seg

  .tbss :
  {
    *(.tbss)
    _tbss_end = .;
  } :data_seg

  _end = .;
}

/*----------------------------------------------------------------------*/
/* Program Headers with permissions                                     */
/*----------------------------------------------------------------------*/
PHDRS
{
  text_seg PT_LOAD FLAGS(5);    /* PF_R | PF_X */
  data_seg PT_LOAD FLAGS(6);    /* PF_R | PF_W */
}

