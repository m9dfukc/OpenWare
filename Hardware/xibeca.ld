/* Entry Point */
ENTRY(Reset_Handler)

/* Generate a link error if heap and stack don't fit into RAM */
_Min_Heap_Size  = 0x200; /* required amount of heap  */
_Min_Stack_Size = 0x200; /* required amount of stack */

/* Specify the memory areas */
MEMORY
{
/* DTCMRAM  (rwx)   : ORIGIN = 0x20000000, LENGTH = 128K */
/* RAM_D1   (rwx)   : ORIGIN = 0x24000000, LENGTH = 512K */
/* RAM_D2   (rwx)   : ORIGIN = 0x30000000, LENGTH = 288K */
/* RAM_D3   (rwx)   : ORIGIN = 0x38000000, LENGTH = 64K */
/* ITCMRAM  (rwx)   : ORIGIN = 0x00000000, LENGTH = 64K */
/* FLASH    (rx)    : ORIGIN = 0x08000000, LENGTH = 128K */
DTCMRAM  (rwx)   : ORIGIN = 0x20000000, LENGTH = 48K
PATCHRAM (rwx)   : ORIGIN = 0x2000c000, LENGTH = 80K
PLUSRAM  (rwx)   : ORIGIN = 0x24000000, LENGTH = 512K /* D1 RAM */
DMA_D2   (rwx)   : ORIGIN = 0x30000000, LENGTH = 64K
RAM_D2   (rwx)   : ORIGIN = 0x30010000, LENGTH = 224K
RAM_D3   (rwx)   : ORIGIN = 0x38000000, LENGTH = 64K
ITCMRAM  (rwx)   : ORIGIN = 0x00000000, LENGTH = 4K
CCMRAM   (rwx)   : ORIGIN = 0x00001000, LENGTH = 60K
EXTRAM   (rwx)   : ORIGIN = 0xC0000000, LENGTH = 32M /* 32M in Xibeca red */
/* EXTRAM  (rwx)    : ORIGIN = 0xC0000000, LENGTH = 64M /\* 64M in Xibeca Rev01/Rev02 green prototypes *\/ */
BOOTLOADER (rx)  : ORIGIN = 0x08000000, LENGTH = 0K
FLASH   (rx)     : ORIGIN = 0x08000000, LENGTH = 128K /* 128K total flash in Xibeca red */
/* FLASH   (rx)     : ORIGIN = 0x08000000, LENGTH = 384K /\* 2M total flash in Xibeca Rev01/02 green prototypes *\/ */
/* BOOTLOADER (rx)  : ORIGIN = 0x08000000, LENGTH = 128K */
/* FLASH   (rx)     : ORIGIN = 0x08020000, LENGTH = 384K */
STORAGE (rx)     : ORIGIN = 0x08080000, LENGTH = 512K  /* Assuming 1Mb FLASH */
/* STORAGE (rx)     : ORIGIN = 0x08080000, LENGTH = 1536K /\* Assuming 2Mb FLASH *\/ */
}

/* Define output sections */
SECTIONS
{
  /* The startup code goes first into FLASH */
  .isr_vector :
  {
    . = ALIGN(4);
    PROVIDE ( _ISR_VECTOR = . );
    KEEP(*(.isr_vector)) /* Startup code */
    . = ALIGN(4);
    PROVIDE ( _ISR_VECTOR_END = . );
  } >FLASH
  _ISR_VECTOR_SIZE = _ISR_VECTOR_END - _ISR_VECTOR;

  /* CCM section, vars must be located here explicitly */
  /* Example: int foo __attribute__ ((section (".ccmdata"))); */
  /* AC/DC fork: moved up from below so that it stays at the start of CCMRAM
     (0x1000, PROGRAMSTACK) and .itcm_text follows it. NOLOAD, so moving it in
     the script changes no flash address. */
  .ccmdata (NOLOAD) :
  {
    *(.ccmdata)
    . = ALIGN(8);
  } >CCMRAM

  /* AC/DC fork: hot path in ITCM.
     On this hardware the position of code in flash shifts the block synchronous
     noise lines (fs/8, USB SOF comb, mixing products). Everything that runs
     periodically (SAI DMA ISR, FreeRTOS scheduler and idle task, audio task,
     USB OTG ISR and audio class, SysTick, ADC DMA, onLoop LED code) runs from
     ITCM instead, together with a copy of the vector table (VTOR), so that its
     timing no longer depends on the flash layout.
     - Placed after PROGRAMSTACK and before _CCMRAM: the patch heap segment
       [_CCMRAM, _CCMRAM_END) is zeroed at every patch start (ProgramManager.cpp).
     - Must come before .text: an input section goes to the first rule it matches.
     - ITCM has 64 bit ECC: start, end and load address are 8 byte aligned and
       Reset_Handler copies 64 bit words (Source/startup_owl3.s).
     - Nothing here may run before that copy (SystemInit, constructors).
     - Calls out to flash go through linker veneers placed in ITCM; they are
       audited with tools/check_itcm.py (only cold or error paths may leave). */
  .itcm_vectors (NOLOAD) :
  {
    . = ALIGN(1024);   /* VTOR: 166 entries -> 1 KB alignment */
    _sitcm_vect = .;
    . = . + _ISR_VECTOR_SIZE;
    . = ALIGN(8);
    _eitcm_vect = .;
  } >CCMRAM

  .itcm_text : ALIGN(8)
  {
    _sitcm_text = .;
    /* A: SAI1 DMA ISR (6 kHz) */
    *stm32h7xx_it.o(.text.DMA1_Stream0_IRQHandler .text.DMA1_Stream1_IRQHandler)
    *stm32h7xx_hal_dma.o(.text.HAL_DMA_IRQHandler)
    *stm32h7xx_hal_sai.o(.text.SAI_DMATxHalfCplt .text.SAI_DMATxCplt .text.SAI_DMARxHalfCplt .text.SAI_DMARxCplt)
    *stm32h7xx_hal_sai.o(.text.HAL_SAI_RxHalfCpltCallback .text.HAL_SAI_RxCpltCallback)
    /* Patch 11g: with Patch 12b the strong RX callbacks (audioCallback) are in Codec.o */
    *Codec.o(.text.HAL_SAI_RxHalfCpltCallback .text.HAL_SAI_RxCpltCallback)
    *Codec.o(.text.HAL_SAI_TxHalfCpltCallback .text.HAL_SAI_TxCpltCallback)
    *ProgramManager.o(.text.audioCallback)
    *tasks.o(.text.vTaskNotifyGiveFromISR)
    *port.o(.text.vPortValidateInterruptPriority)
    /* B: scheduler and idle task */
    *port.o(.text.PendSV_Handler .text.vPortEnterCritical .text.vPortExitCritical)
    *tasks.o(.text.vTaskSwitchContext .text.ulTaskNotifyTake .text.prvAddCurrentTaskToDelayedList .text.prvIdleTask)
    *list.o(.text.uxListRemove .text.vListInsertEnd .text.vListInsert)
    /* C: audio task (onProgramReady, USB conversion, parameters, MIDI) */
    *ProgramManager.o(.text.onProgramReady .text._Z15usbd_tx_convertPlj .text._Z15usbd_rx_convertPlj .text.getProgramVector)
    *ACDC.o(.text.updateParameters)
    *MidiWriter.o(.text._ZN10MidiWriter8transmitEv)
    *MidiReceiver.o(.text._ZN12MidiReceiver7receiveEv)
    *usbd_audio.o(.text.usbd_midi_ready .text.usbd_midi_tx)
    /* D: USB OTG FS ISR, USBD core and audio class, feedback (1 kHz SOF) */
    *stm32h7xx_it.o(.text.OTG_FS_IRQHandler)
    *stm32h7xx_hal_pcd.o(.text.HAL_PCD_IRQHandler .text.HAL_PCD_EP_GetRxCount .text.HAL_PCD_EP_Receive .text.HAL_PCD_EP_Transmit .text.HAL_PCD_EP_Flush)
    *stm32h7xx_ll_usb.o(.text.USB_ReadInterrupts .text.USB_ReadDevAllOutEpInterrupt .text.USB_ReadDevAllInEpInterrupt .text.USB_ReadDevOutEPInterrupt .text.USB_ReadDevInEPInterrupt .text.USB_GetMode .text.USB_ReadPacket .text.USB_WritePacket .text.USB_EPStartXfer .text.USB_FlushTxFifo .text.USB_FlushRxFifo)
    *usbd_conf.o(.text.HAL_PCD_SOFCallback .text.HAL_PCD_DataOutStageCallback .text.HAL_PCD_DataInStageCallback .text.HAL_PCD_ISOINIncompleteCallback .text.HAL_PCD_ISOOUTIncompleteCallback .text.USBD_LL_GetRxDataSize .text.USBD_LL_PrepareReceive .text.USBD_LL_Transmit .text.USBD_LL_FlushEP)
    *usbd_core.o(.text.USBD_LL_SOF .text.USBD_LL_DataOutStage .text.USBD_LL_DataInStage .text.USBD_LL_IsoINIncomplete .text.USBD_LL_IsoOUTIncomplete)
    *usbd_audio.o(.text._ZL14USBD_AUDIO_SOFP19_USBD_HandleTypeDef .text._ZL18USBD_AUDIO_DataOutP19_USBD_HandleTypeDefh .text._ZL17USBD_AUDIO_DataInP19_USBD_HandleTypeDefh .text._ZL26USBD_AUDIO_IsoINIncompleteP19_USBD_HandleTypeDefh .text._ZL27USBD_AUDIO_IsoOutIncompleteP19_USBD_HandleTypeDefh .text.usbd_audio_sof_callback)
    *ProgramManager.o(.text.usbd_audio_get_rx_count)
    *Codec.o(.text._ZN5Codec16getSampleCounterEv)
    *pcm3168a.o(.text.codec_ndtr)
    /* E: SysTick */
    *stm32h7xx_it.o(.text.SysTick_Handler)
    *stm32h7xx_hal.o(.text.HAL_IncTick .text.HAL_GetTick)
    *tasks.o(.text.xTaskGetSchedulerState .text.xTaskIncrementTick .text.xTaskIncrementTick.part.0)
    *port.o(.text.xPortSysTickHandler)
    /* F: ADC DMA */
    *stm32h7xx_it.o(.text.DMA1_Stream3_IRQHandler)
    *stm32h7xx_hal_adc.o(.text.ADC_DMAConvCplt .text.ADC_DMAHalfConvCplt .text.HAL_ADC_ConvCpltCallback .text.HAL_ADC_ConvHalfCpltCallback)
    /* onLoop (every 2 ms): 07d input LED code and its helpers */
    KEEP(*ACDC.o(.late_text .late_text.*))
    *ACDC.o(.text.setLed .text.setLed.constprop.0)
    *ProgramManager.o(.text.getParameterValue .text.setParameterValue)
    /* H: main loop (StartDefaultTask -> loop -> onLoop, Owl::loop, every 2 ms) */
    *main.o(.text.StartDefaultTask)
    *callbacks.o(.text.loop)
    *ACDC.o(.text.onLoop)
    *Owl.o(.text._ZN3Owl4loopEv)
    *ProgramManager.o(.text._ZN14ProgramManager16isProgramRunningEv)
    *support.o(.text.device_watchdog)
    *tasks.o(.text.vTaskDelayUntil .text.vTaskSuspendAll .text.xTaskResumeAll .text.xTaskResumeAll.part.0)
    /* libc helpers called from the above */
    *libc_nano.a:*(.text.memset .text.memcpy .text.abs)
    . = ALIGN(8);
    _eitcm_text = .;
  } >CCMRAM AT>FLASH
  _litcm_text = LOADADDR(.itcm_text);
  PROVIDE (_CCMRAM = _eitcm_text);
  ASSERT((_sitcm_vect & 0x3FF) == 0, "ITCM vector table not 1 KB aligned")
  ASSERT((_sitcm_text & 7) == 0 && (_eitcm_text & 7) == 0 && (_litcm_text & 7) == 0, ".itcm_text not 8 byte aligned (ITCM ECC)")

  /* The program code and other data goes into FLASH */
  .text :
  {
    . = ALIGN(4);
    /* AC/DC fork: flash code and strings whose addresses the ITCM block holds
       (vector table entries, veneer targets on cold/error paths, debug strings
       of the hot functions). Kept together at the start of .text, so that code
       behind them can move (layout test fx3ip) without changing one byte of the
       ITCM content. tools/check_itcm.py checks that nothing referenced from ITCM
       lies behind _eitcm_anchor. */
    _sitcm_anchor = .;
    *startup_owl3.o(.text.Reset_Handler .text.Default_Handler)
    *errorhandlers.o(.text.*_Handler .text.*_IRQHandler)
    *stm32h7xx_it.o(.text.*_IRQHandler)
    *port.o(.text.SVC_Handler)
    *usbd_conf.o(.text.HAL_PCD_SetupStageCallback .text.HAL_PCD_ResetCallback .text.HAL_PCD_ConnectCallback .text.HAL_PCD_DisconnectCallback .text.HAL_PCD_ResumeCallback .text.HAL_PCD_SuspendCallback .text.USBD_LL_StallEP)
    *stm32h7xx_hal_pcd_ex.o(.text.HAL_PCDEx_LPM_Callback)
    *usbd_ioreq.o(.text.USBD_CtlContinueRx .text.USBD_CtlContinueSendData .text.USBD_CtlReceiveStatus .text.USBD_CtlSendStatus)
    *stm32h7xx_ll_usb.o(.text.USB_EP0StartXfer .text.USB_EP0_OutStart .text.USB_ActivateSetup .text.USB_GetDevSpeed .text.USB_SetTurnaroundTime)
    *stm32h7xx_hal_rcc.o(.text.HAL_RCC_GetHCLKFreq)
    *stm32h7xx_hal_adc.o(.text.HAL_ADC_ErrorCallback)
    *heap_4.o(.text.vPortFree)
    *MidiReceiver.o(.text.usbd_midi_rx)
    *ProgramManager.o(.text.usbd_audio_rx_start_callback .text.vApplicationStackOverflowHook)
    *usbd_audio.o(.text.usbd_audio_error)
    *message.o(.text._Z12debugMessagePKcff .text._Z12debugMessagePKcfff)
    *callbacks.o(.text.setup)
    *usbd_audio.o(.rodata._ZL18USBD_AUDIO_DataOutP19_USBD_HandleTypeDefh.str* .rodata._ZL17USBD_AUDIO_DataInP19_USBD_HandleTypeDefh.str* .rodata.usbd_audio_sof_callback.str*)
    *ProgramManager.o(.rodata._Z15usbd_rx_convertPlj.str* .rodata._Z15usbd_tx_convertPlj.str*)
    _eitcm_anchor = .;
    *(.text)           /* .text sections (code) */
    *(.text*)          /* .text* sections (code) */
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;        /* define a global symbols at end of code */
  } >FLASH

  /* Constant data goes into FLASH */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
    . = ALIGN(4);
  } >FLASH

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

  /* Initialized data sections goes into RAM, load LMA copy after code */
  .data : 
  {
    . = ALIGN(4);
    _sdata = .;        /* create a global symbol at data start */
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */

    . = ALIGN(4);
    _edata = .;        /* define a global symbol at data end */
  } >DTCMRAM AT> FLASH

  /* AC/DC fork: code placed after everything else in flash, so that adding it
     does not move any other code or data (see ACDC/Core/Src/ACDC.cpp onLoop) */
  .late_text :
  {
    . = ALIGN(4);
    KEEP(*(.late_text*))
    . = ALIGN(4);
  } >FLASH

  
  /* Uninitialized data section */
  . = ALIGN(4);
  .bss :
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

    end = .;           /* system heap, used e.g. by printf in DEBUG builds */
    . = . + _Min_Heap_Size;
  } >DTCMRAM

  /* User_heap_stack section, used to check that there is enough RAM left */
  ._user_heap_stack :
  {
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >ITCMRAM
  _estack = ORIGIN(ITCMRAM)+LENGTH(ITCMRAM);

  /* AC/DC fork: .ccmdata moved up in front of .itcm_text (see there) */
  _CCMRAM_END = ORIGIN(CCMRAM) + LENGTH(CCMRAM);
  _CCMRAM_SIZE = _CCMRAM_END - _CCMRAM;

  /* Bootloader end will be used for finding bootloader token */
  .bootloader (NOLOAD) :
  {
    PROVIDE (_BOOTLOADER = .);
  } >BOOTLOADER
  _BOOTLOADER_END = ORIGIN(BOOTLOADER) + LENGTH(BOOTLOADER);

  /* External memory, vars must be located here explicitly */
  /* Example: int foo __attribute__ ((section (".extdata"))); */
  .extdata (NOLOAD) :
  {
    /* *(.extdata) */
    /* . = ALIGN(8); */
    PROVIDE ( _EXTRAM = . );
  /* } >RAM_D1 */
  /* _EXTRAM_END = ORIGIN(RAM_D1) + LENGTH(RAM_D1); */
  /* _EXTRAM_SIZE = LENGTH(RAM_D1); */
  } >EXTRAM
  _EXTRAM_END = ORIGIN(EXTRAM) + LENGTH(EXTRAM);
  _EXTRAM_SIZE = LENGTH(EXTRAM);

  .patchram (NOLOAD) :
  {
    PROVIDE (_PATCHRAM = .);
  } >PATCHRAM
  _PATCHRAM_END = ORIGIN(PATCHRAM) + LENGTH(PATCHRAM);
  _PATCHRAM_SIZE = LENGTH(PATCHRAM);

  .plusram (NOLOAD) :
  {
    PROVIDE (_PLUSRAM = .);
  } >PLUSRAM
  _PLUSRAM_END = ORIGIN(PLUSRAM) + LENGTH(PLUSRAM);
  _PLUSRAM_SIZE = LENGTH(PLUSRAM);

  .heapd2 (NOLOAD) :
  {
    *(.d2data)
    PROVIDE (_HEAP_D2 = .);
  } >RAM_D2
  _HEAP_D2_END = ORIGIN(RAM_D2) + LENGTH(RAM_D2) - 64; /* AC/DC fork: top 64 B for .late_bss */
  _HEAP_D2_SIZE = _HEAP_D2_END - _HEAP_D2;

  /* AC/DC fork: state of .late_text code at the top of RAM_D2, not zeroed at boot */
  .late_bss _HEAP_D2_END (NOLOAD) :
  {
    KEEP(*(.late_bss*))
  } >RAM_D2
  ASSERT(SIZEOF(.late_bss) <= 64, ".late_bss larger than the 64 B reserved at the top of RAM_D2")

  .heapd3 (NOLOAD) :
  {
    *(.d3data)
    PROVIDE (_HEAP_D3 = .);
  } >RAM_D3
  _HEAP_D3_END = ORIGIN(RAM_D3) + LENGTH(RAM_D3);
  _HEAP_D3_SIZE = _HEAP_D3_END - _HEAP_D3;

  .dmadata (NOLOAD) :
  {
    PROVIDE (_DMA_DATA = .);
    *(.dmadata)
  } >DMA_D2

  .flash_storage (NOLOAD) :
  {
    PROVIDE (_FLASH_STORAGE_BEGIN = .);
  } >STORAGE
  _FLASH_STORAGE_END = ORIGIN(STORAGE) + LENGTH(STORAGE);  
  _FLASH_STORAGE_SIZE = LENGTH(STORAGE);

  /* Remove information from the standard libraries */
  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}


