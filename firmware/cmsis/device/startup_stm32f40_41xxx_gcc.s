/**
 ******************************************************************************
 * @file    startup_stm32f40_41xxx_gcc.s
 * @brief   STM32F40xxx/41xxx 向量表与复位入口 (GNU 汇编语法)
 *          仅用于 GNU Arm Toolchain 编译; 原 Keil 启动文件不受影响
 ******************************************************************************
 */

  .syntax unified
  .cpu cortex-m4
  .fpu fpv4-sp-d16
  .thumb

.global  g_pfnVectors
.global  Default_Handler

/* .data 段初始化所需的链接脚本符号 */
.word  _sidata
.word  _sdata
.word  _edata
.word  _sbss
.word  _ebss

  .section  .text.Reset_Handler
  .weak  Reset_Handler
  .type  Reset_Handler, %function
Reset_Handler:
  ldr   sp, =_estack          /* 设置栈指针 */

  /* 把 .data 的初始值从 Flash 拷贝到 RAM */
  movs  r1, #0
  b     LoopCopyDataInit

CopyDataInit:
  ldr   r3, =_sidata
  ldr   r3, [r3, r1]
  str   r3, [r0, r1]
  adds  r1, r1, #4

LoopCopyDataInit:
  ldr   r0, =_sdata
  ldr   r3, =_edata
  adds  r2, r0, r1
  cmp   r2, r3
  bcc   CopyDataInit

  /* .bss 段清零 */
  ldr   r2, =_sbss
  b     LoopFillZerobss

FillZerobss:
  movs  r3, #0
  str   r3, [r2], #4

LoopFillZerobss:
  ldr   r3, =_ebss
  cmp   r2, r3
  bcc   FillZerobss

  /* 调用 SystemInit (来自 system_stm32f4xx.c) */
  bl    SystemInit
  /* 进入 main */
  bl    main
  bx    lr

.size  Reset_Handler, .-Reset_Handler

/* 默认中断处理: 死循环, 便于调试时发现问题 */
  .section  .text.Default_Handler,"ax",%progbits
Default_Handler:
  b  .
  .size  Default_Handler, .-Default_Handler

/* 宏: 把每个中断向量定义为弱引用, 默认指向 Default_Handler */
.macro  def_irq_vector  handler
  .weak  \handler
  .set   \handler, Default_Handler
.endm

/* 系统异常 */
def_irq_vector  NMI_Handler
def_irq_vector  HardFault_Handler
def_irq_vector  MemManage_Handler
def_irq_vector  BusFault_Handler
def_irq_vector  UsageFault_Handler
def_irq_vector  SVC_Handler
def_irq_vector  DebugMon_Handler
def_irq_vector  PendSV_Handler
def_irq_vector  SysTick_Handler

/* 外设中断 */
def_irq_vector  WWDG_IRQHandler
def_irq_vector  PVD_IRQHandler
def_irq_vector  TAMP_STAMP_IRQHandler
def_irq_vector  RTC_WKUP_IRQHandler
def_irq_vector  FLASH_IRQHandler
def_irq_vector  RCC_IRQHandler
def_irq_vector  EXTI0_IRQHandler
def_irq_vector  EXTI1_IRQHandler
def_irq_vector  EXTI2_IRQHandler
def_irq_vector  EXTI3_IRQHandler
def_irq_vector  EXTI4_IRQHandler
def_irq_vector  DMA1_Stream0_IRQHandler
def_irq_vector  DMA1_Stream1_IRQHandler
def_irq_vector  DMA1_Stream2_IRQHandler
def_irq_vector  DMA1_Stream3_IRQHandler
def_irq_vector  DMA1_Stream4_IRQHandler
def_irq_vector  DMA1_Stream5_IRQHandler
def_irq_vector  DMA1_Stream6_IRQHandler
def_irq_vector  ADC_IRQHandler
def_irq_vector  CAN1_TX_IRQHandler
def_irq_vector  CAN1_RX0_IRQHandler
def_irq_vector  CAN1_RX1_IRQHandler
def_irq_vector  CAN1_SCE_IRQHandler
def_irq_vector  EXTI9_5_IRQHandler
def_irq_vector  TIM1_BRK_TIM9_IRQHandler
def_irq_vector  TIM1_UP_TIM10_IRQHandler
def_irq_vector  TIM1_TRG_COM_TIM11_IRQHandler
def_irq_vector  TIM1_CC_IRQHandler
def_irq_vector  TIM2_IRQHandler
def_irq_vector  TIM3_IRQHandler
def_irq_vector  TIM4_IRQHandler
def_irq_vector  I2C1_EV_IRQHandler
def_irq_vector  I2C1_ER_IRQHandler
def_irq_vector  I2C2_EV_IRQHandler
def_irq_vector  I2C2_ER_IRQHandler
def_irq_vector  SPI1_IRQHandler
def_irq_vector  SPI2_IRQHandler
def_irq_vector  USART1_IRQHandler
def_irq_vector  USART2_IRQHandler
def_irq_vector  USART3_IRQHandler
def_irq_vector  EXTI15_10_IRQHandler
def_irq_vector  RTC_Alarm_IRQHandler
def_irq_vector  OTG_FS_WKUP_IRQHandler
def_irq_vector  TIM8_BRK_TIM12_IRQHandler
def_irq_vector  TIM8_UP_TIM13_IRQHandler
def_irq_vector  TIM8_TRG_COM_TIM14_IRQHandler
def_irq_vector  TIM8_CC_IRQHandler
def_irq_vector  DMA1_Stream7_IRQHandler
def_irq_vector  FSMC_IRQHandler
def_irq_vector  SDIO_IRQHandler
def_irq_vector  TIM5_IRQHandler
def_irq_vector  SPI3_IRQHandler
def_irq_vector  UART4_IRQHandler
def_irq_vector  UART5_IRQHandler
def_irq_vector  TIM6_DAC_IRQHandler
def_irq_vector  TIM7_IRQHandler
def_irq_vector  DMA2_Stream0_IRQHandler
def_irq_vector  DMA2_Stream1_IRQHandler
def_irq_vector  DMA2_Stream2_IRQHandler
def_irq_vector  DMA2_Stream3_IRQHandler
def_irq_vector  DMA2_Stream4_IRQHandler
def_irq_vector  ETH_IRQHandler
def_irq_vector  ETH_WKUP_IRQHandler
def_irq_vector  CAN2_TX_IRQHandler
def_irq_vector  CAN2_RX0_IRQHandler
def_irq_vector  CAN2_RX1_IRQHandler
def_irq_vector  CAN2_SCE_IRQHandler
def_irq_vector  OTG_FS_IRQHandler
def_irq_vector  DMA2_Stream5_IRQHandler
def_irq_vector  DMA2_Stream6_IRQHandler
def_irq_vector  DMA2_Stream7_IRQHandler
def_irq_vector  USART6_IRQHandler
def_irq_vector  I2C3_EV_IRQHandler
def_irq_vector  I2C3_ER_IRQHandler
def_irq_vector  OTG_HS_EP1_OUT_IRQHandler
def_irq_vector  OTG_HS_EP1_IN_IRQHandler
def_irq_vector  OTG_HS_WKUP_IRQHandler
def_irq_vector  OTG_HS_IRQHandler
def_irq_vector  DCMI_IRQHandler
def_irq_vector  CRYP_IRQHandler
def_irq_vector  HASH_RNG_IRQHandler
def_irq_vector  FPU_IRQHandler

/* 向量表 */
  .section  .isr_vector,"a",%progbits
  .type  g_pfnVectors, %object

g_pfnVectors:
  .word  _estack
  .word  Reset_Handler
  .word  NMI_Handler
  .word  HardFault_Handler
  .word  MemManage_Handler
  .word  BusFault_Handler
  .word  UsageFault_Handler
  .word  0
  .word  0
  .word  0
  .word  0
  .word  SVC_Handler
  .word  DebugMon_Handler
  .word  0
  .word  PendSV_Handler
  .word  SysTick_Handler

  .word  WWDG_IRQHandler
  .word  PVD_IRQHandler
  .word  TAMP_STAMP_IRQHandler
  .word  RTC_WKUP_IRQHandler
  .word  FLASH_IRQHandler
  .word  RCC_IRQHandler
  .word  EXTI0_IRQHandler
  .word  EXTI1_IRQHandler
  .word  EXTI2_IRQHandler
  .word  EXTI3_IRQHandler
  .word  EXTI4_IRQHandler
  .word  DMA1_Stream0_IRQHandler
  .word  DMA1_Stream1_IRQHandler
  .word  DMA1_Stream2_IRQHandler
  .word  DMA1_Stream3_IRQHandler
  .word  DMA1_Stream4_IRQHandler
  .word  DMA1_Stream5_IRQHandler
  .word  DMA1_Stream6_IRQHandler
  .word  ADC_IRQHandler
  .word  CAN1_TX_IRQHandler
  .word  CAN1_RX0_IRQHandler
  .word  CAN1_RX1_IRQHandler
  .word  CAN1_SCE_IRQHandler
  .word  EXTI9_5_IRQHandler
  .word  TIM1_BRK_TIM9_IRQHandler
  .word  TIM1_UP_TIM10_IRQHandler
  .word  TIM1_TRG_COM_TIM11_IRQHandler
  .word  TIM1_CC_IRQHandler
  .word  TIM2_IRQHandler
  .word  TIM3_IRQHandler
  .word  TIM4_IRQHandler
  .word  I2C1_EV_IRQHandler
  .word  I2C1_ER_IRQHandler
  .word  I2C2_EV_IRQHandler
  .word  I2C2_ER_IRQHandler
  .word  SPI1_IRQHandler
  .word  SPI2_IRQHandler
  .word  USART1_IRQHandler
  .word  USART2_IRQHandler
  .word  USART3_IRQHandler
  .word  EXTI15_10_IRQHandler
  .word  RTC_Alarm_IRQHandler
  .word  OTG_FS_WKUP_IRQHandler
  .word  TIM8_BRK_TIM12_IRQHandler
  .word  TIM8_UP_TIM13_IRQHandler
  .word  TIM8_TRG_COM_TIM14_IRQHandler
  .word  TIM8_CC_IRQHandler
  .word  DMA1_Stream7_IRQHandler
  .word  FSMC_IRQHandler
  .word  SDIO_IRQHandler
  .word  TIM5_IRQHandler
  .word  SPI3_IRQHandler
  .word  UART4_IRQHandler
  .word  UART5_IRQHandler
  .word  TIM6_DAC_IRQHandler
  .word  TIM7_IRQHandler
  .word  DMA2_Stream0_IRQHandler
  .word  DMA2_Stream1_IRQHandler
  .word  DMA2_Stream2_IRQHandler
  .word  DMA2_Stream3_IRQHandler
  .word  DMA2_Stream4_IRQHandler
  .word  ETH_IRQHandler
  .word  ETH_WKUP_IRQHandler
  .word  CAN2_TX_IRQHandler
  .word  CAN2_RX0_IRQHandler
  .word  CAN2_RX1_IRQHandler
  .word  CAN2_SCE_IRQHandler
  .word  OTG_FS_IRQHandler
  .word  DMA2_Stream5_IRQHandler
  .word  DMA2_Stream6_IRQHandler
  .word  DMA2_Stream7_IRQHandler
  .word  USART6_IRQHandler
  .word  I2C3_EV_IRQHandler
  .word  I2C3_ER_IRQHandler
  .word  OTG_HS_EP1_OUT_IRQHandler
  .word  OTG_HS_EP1_IN_IRQHandler
  .word  OTG_HS_WKUP_IRQHandler
  .word  OTG_HS_IRQHandler
  .word  DCMI_IRQHandler
  .word  CRYP_IRQHandler
  .word  HASH_RNG_IRQHandler
  .word  FPU_IRQHandler

  .size  g_pfnVectors, .-g_pfnVectors
