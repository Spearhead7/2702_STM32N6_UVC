/*
******************************************************************************
**
** @file        : STM32N6XX_AXISRAM2_fsbl.ld
**
** @author      : GPM Application Team
**
** @brief       : Linker script for STM32N6XX Device from STM32N6 series
**                      512 KBytes RAM
**
**                Set heap size, stack size and stack location according
**                to application requirements.
**
**                Set memory bank area and size if external memory is used
**
**  Target      : STMicroelectronics STM32
**
**  Distribution: The file is distributed as is, without any warranty
**                of any kind.
**
******************************************************************************
** @attention
**
** Copyright (c) 2024 STMicroelectronics.
** All rights reserved.
**
** This software is licensed under terms that can be found in the LICENSE file
** in the root directory of this software component.
** If no LICENSE file comes with this software, it is provided AS-IS.
**
******************************************************************************
*/

/* Entry Point */
ENTRY(Reset_Handler)

/* Highest address of the user mode stack */
_estack = ORIGIN(RAM) + LENGTH(RAM); /* end of "RAM" Ram type memory */
_sstack = _estack - _Min_Stack_Size;

_Min_Heap_Size = 0x800; /* required amount of heap */
_Min_Stack_Size = 0xe00; /* required amount of stack */

/* Memories definition */
MEMORY
{
  /* 개발 모드(AXISRAM2 511K 전체 사용). 예제는 ROM 255K / RAM 256K였는데,
     고정 MJPEG 영상 데이터 때문에 Debug 빌드 코드가 넘쳐 경계를 옮겼다.
     RAM 끝 32K(0x341F8000~)는 MPU 비캐시 영역(USB HPCD 등). */
  ROM    (xrw)    : ORIGIN = 0x34180400,   LENGTH = 383K
  RAM    (xrw)    : ORIGIN = 0x341E0000,   LENGTH = 128K
  /* 외부 PSRAM(APS256XX, 32MB, XSPI1 memory-mapped). main()에서 초기화한 뒤에만 쓸 수 있다.
     MPU 비캐시. 영상 프레임·VENC 작업 메모리·인코딩 프레임 큐용. */
  PSRAM  (xrw)    : ORIGIN = 0x90000000,   LENGTH = 32M
  /* AXISRAM2 아래쪽 512K(0x34100000~). 개발 모드에서는 비어 있다. DCMIPP–VENC 핸드셰이크
     줄 버퍼(IN_LINEBUF)용. M6 플래시 부팅 배치 때 다시 정한다. */
  LINEBUF (rw)    : ORIGIN = 0x34100000,   LENGTH = 512K
}

/* Sections */
SECTIONS
{
  /* The startup code into "RAM" Ram type memory */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector)) /* Startup code */
    . = ALIGN(4);
  } >ROM

  /* The program code and other data into "RAM" Ram type memory */
  .text :
  {
    . = ALIGN(4);
    *(.text)           /* .text sections (code) */
    *(.text*)          /* .text* sections (code) */
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)
    *(.RamFunc)        /* .RamFunc sections */
    *(.RamFunc*)       /* .RamFunc* sections */

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;        /* define a global symbols at end of code */
  } >ROM

  /* Constant data into "RAM" Ram type memory */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
    . = ALIGN(4);
  } >ROM

  .ARM.extab   (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
   {
    . = ALIGN(4);
    *(.ARM.extab* .gnu.linkonce.armextab.*)
    . = ALIGN(4);
  } >ROM

  .ARM (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
   {
    . = ALIGN(4);
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
    . = ALIGN(4);
  } >ROM

  .preinit_array     (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
    . = ALIGN(4);
  } >ROM

  .init_array (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
    . = ALIGN(4);
  } >ROM

  .fini_array (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
    . = ALIGN(4);
  } >ROM

  /* Used by the startup to initialize data */
  _sidata = LOADADDR(.data);

  /* Initialized data sections into "RAM" Ram type memory */
  .data :
  {
    . = ALIGN(4);
    _sdata = .;        /* create a global symbol at data start */
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */

    . = ALIGN(4);
    _edata = .;        /* define a global symbol at data end */

  } >RAM AT> ROM

  .noncacheable :
  {
    . = ALIGN(8);
    __snoncacheable = .;/* create symbol for start of section */
    KEEP(*(.noncacheable))
    . = ALIGN(8);
    __enoncacheable = .;  /* create symbol for end of section */
  } > RAM


  .gnu.sgstubs :
  {
    . = ALIGN(4);
    *(.gnu.sgstubs*)   /* Secure Gateway stubs */
    . = ALIGN(4);
  } >ROM
  /* Uninitialized data section into "RAM" Ram type memory */
  . = ALIGN(4);
  .bss :
  {
    /* This is used by the startup in order to initialize the .bss section */
    _sbss = .;         /* define a global symbol at bss start */
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)

    . = ALIGN(4);
    _ebss = .;         /* define a global symbol at bss end */
    __bss_end__ = _ebss;
  } >RAM

  /* User_heap_stack section, used to check that there is enough "RAM" Ram  type memory left */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM
  /* 캐시 RAM(0x341E0000~0x341EFFFF, 64K)을 넘으면 비캐시 영역과 겹친다. */
  ASSERT(. <= 0x341F0000, "cacheable RAM overflow: data/bss > 64K")

  /* M3(USB DMA): RAM 위쪽 64K(0x341F0000~0x341FFFFF)는 MPU 비캐시(main.c MPU_Config).
     USB DMA가 직접 읽고 쓰는 USBX 메모리 풀(엔드포인트 전송 버퍼 포함)을 여기 둔다.
     맨 끝은 MSP 스택(_estack = 0x34200000, 0xE00). */
  .usbx_bss 0x341F0000 (NOLOAD) :
  {
    *(.UsbxPoolSection)
  } >RAM
  ASSERT(. <= 0x341FD000, "USBX pool > 52K overlaps .usbx_data")
  /* PSRAM 영역(IN_PSRAM). NOLOAD: 디버거가 굽지 않고, 시작 코드도 0으로 지우지 않는다. */
  .psram_bss (NOLOAD) :
  {
    . = ALIGN(32);
    __psram_bss_start__ = .;
    *(.psram_bss)
    *(.psram_bss*)
    . = ALIGN(32);
    __psram_bss_end__ = .;
  } >PSRAM

  /* 핸드셰이크 줄 버퍼(IN_LINEBUF). NOLOAD: DCMIPP가 쓰고 VENC가 읽기만 한다. */
  .linebuf_bss (NOLOAD) :
  {
    . = ALIGN(32);
    *(.linebuf_bss)
    *(.linebuf_bss*)
    . = ALIGN(32);
  } >LINEBUF

  .usbx_data 0x341FD000 (NOLOAD):
  {
    *(.UsbHpcdSection)
  } >RAM
  .uart_bss 0x341FD800 (NOLOAD):
  {
    *(.UsbxAppSection)
  } >RAM
  ASSERT(. <= _sstack, "non-cacheable sections overlap MSP stack")
  /* Remove information from the compiler libraries */
  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
