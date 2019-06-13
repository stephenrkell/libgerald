SECTIONS
{
  .text : { *(.text) __real__dl_init = SIZEOF(.text); }
};
