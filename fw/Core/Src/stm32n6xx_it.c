/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32n6xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32n6xx_it.h"
#include "usbpd.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
#if defined(TCPP0203_SUPPORT)
void TCPP0203_PORT0_FLG_EXTI_IRQHANDLER(void);
#endif /* TCPP0203_SUPPORT */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* ---- HardFault 정보 출력 ----------------------------------------------------
   폴트가 나면 원인 레지스터와 폴트 직전 PC/LR, 실행 중이던 ThreadX 스레드 이름을
   USART1(ST-LINK 가상 COM, 115200)로 출력한다. 인터럽트·HAL에 기대지 않고
   레지스터를 직접 쓴다(폴트 상황에서도 동작하도록). */
#include "tx_api.h"

extern TX_THREAD *_tx_thread_current_ptr;

static void fault_putc(char c)
{
  while ((USART1->ISR & USART_ISR_TXE_TXFNF) == 0U)
  {
  }
  USART1->TDR = (uint8_t)c;
}

static void fault_puts(const char *s)
{
  while (*s != '\0')
  {
    fault_putc(*s++);
  }
}

static void fault_hex(const char *name, uint32_t v)
{
  static const char hex[] = "0123456789ABCDEF";
  fault_puts(name);
  fault_puts("=0x");
  for (int i = 28; i >= 0; i -= 4)
  {
    fault_putc(hex[(v >> i) & 0xFU]);
  }
  fault_puts("\r\n");
}

void fault_dump(uint32_t *sp, uint32_t exc_return)
{
  fault_puts("\r\n*** HardFault ***\r\n");
  fault_hex("PC   ", sp[6]);
  fault_hex("LR   ", sp[5]);
  fault_hex("xPSR ", sp[7]);
  fault_hex("R0   ", sp[0]);
  fault_hex("R1   ", sp[1]);
  fault_hex("R2   ", sp[2]);
  fault_hex("R3   ", sp[3]);
  fault_hex("R12  ", sp[4]);
  fault_hex("SP   ", (uint32_t)sp);
  fault_hex("EXC  ", exc_return);
  fault_hex("CFSR ", SCB->CFSR);
  fault_hex("HFSR ", SCB->HFSR);
  fault_hex("MMFAR", SCB->MMFAR);
  fault_hex("BFAR ", SCB->BFAR);
  fault_hex("SFSR ", SAU->SFSR);
  fault_hex("SFAR ", SAU->SFAR);
  fault_puts("THREAD ");
  if ((_tx_thread_current_ptr != TX_NULL) && (_tx_thread_current_ptr->tx_thread_name != TX_NULL))
  {
    fault_puts(_tx_thread_current_ptr->tx_thread_name);
  }
  else
  {
    fault_puts("(none)");
  }
  fault_puts("\r\n");

  while (1)
  {
  }
}

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern DMA_HandleTypeDef handle_GPDMA1_Channel3;
extern DMA_HandleTypeDef handle_GPDMA1_Channel2;
extern UART_HandleTypeDef huart1;
extern PCD_HandleTypeDef hpcd_USB_OTG_HS1;
extern TIM_HandleTypeDef htim6;

/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
   while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
/* naked: 컴파일러가 스택을 건드리기 전에 폴트 당시 스택 포인터를 넘긴다(.ioc 없음, 직접 관리) */
__attribute__((naked)) void HardFault_Handler(void)
{
  __asm volatile(
    "tst   lr, #4      \n"
    "ite   eq          \n"
    "mrseq r0, msp     \n"
    "mrsne r0, psp     \n"
    "mov   r1, lr      \n"
    "b     fault_dump  \n");
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */

  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Prefetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */

  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */

  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Secure fault.
  */
void SecureFault_Handler(void)
{
  /* USER CODE BEGIN SecureFault_IRQn 0 */

  /* USER CODE END SecureFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_SecureFault_IRQn 0 */
    /* USER CODE END W1_SecureFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/******************************************************************************/
/* STM32N6xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32n6xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles EXTI Line13 interrupt.
  */
void EXTI13_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI13_IRQn 0 */

  /* USER CODE END EXTI13_IRQn 0 */
  HAL_GPIO_EXTI_IRQHandler(USER_BUTTON_Pin);
  /* USER CODE BEGIN EXTI13_IRQn 1 */

  /* USER CODE END EXTI13_IRQn 1 */
}

/**
  * @brief This function handles GPDMA1 Channel 0 global interrupt.
  */
void GPDMA1_Channel0_IRQHandler(void)
{
  /* USER CODE BEGIN GPDMA1_Channel0_IRQn 0 */

  /* USER CODE END GPDMA1_Channel0_IRQn 0 */
  /* USER CODE BEGIN GPDMA1_Channel0_IRQn 1 */

  /* USER CODE END GPDMA1_Channel0_IRQn 1 */
}

/**
  * @brief This function handles GPDMA1 Channel 1 global interrupt.
  */
void GPDMA1_Channel1_IRQHandler(void)
{
  /* USER CODE BEGIN GPDMA1_Channel1_IRQn 0 */

  /* USER CODE END GPDMA1_Channel1_IRQn 0 */
  /* USER CODE BEGIN GPDMA1_Channel1_IRQn 1 */

  /* USER CODE END GPDMA1_Channel1_IRQn 1 */
}

/**
  * @brief This function handles GPDMA1 Channel 2 global interrupt.
  */
void GPDMA1_Channel2_IRQHandler(void)
{
  /* USER CODE BEGIN GPDMA1_Channel2_IRQn 0 */

  /* USER CODE END GPDMA1_Channel2_IRQn 0 */
  HAL_DMA_IRQHandler(&handle_GPDMA1_Channel2);
  /* USER CODE BEGIN GPDMA1_Channel2_IRQn 1 */

  /* USER CODE END GPDMA1_Channel2_IRQn 1 */
}

/**
  * @brief This function handles GPDMA1 Channel 3 global interrupt.
  */
void GPDMA1_Channel3_IRQHandler(void)
{
  /* USER CODE BEGIN GPDMA1_Channel3_IRQn 0 */

  /* USER CODE END GPDMA1_Channel3_IRQn 0 */
  HAL_DMA_IRQHandler(&handle_GPDMA1_Channel3);
  /* USER CODE BEGIN GPDMA1_Channel3_IRQn 1 */

  /* USER CODE END GPDMA1_Channel3_IRQn 1 */
}

/**
  * @brief This function handles TIM6 global interrupt.
  */
void TIM6_IRQHandler(void)
{
  /* USER CODE BEGIN TIM6_IRQn 0 */

  /* USER CODE END TIM6_IRQn 0 */
  HAL_TIM_IRQHandler(&htim6);
  /* USER CODE BEGIN TIM6_IRQn 1 */

  /* USER CODE END TIM6_IRQn 1 */
}

/**
  * @brief This function handles USART1 global interrupt.
  */
void USART1_IRQHandler(void)
{
  /* USER CODE BEGIN USART1_IRQn 0 */

  /* USER CODE END USART1_IRQn 0 */
  HAL_UART_IRQHandler(&huart1);
  /* USER CODE BEGIN USART1_IRQn 1 */

  /* USER CODE END USART1_IRQn 1 */
}

/**
  * @brief This function handles UCPD1 global interrupt.
  */
void UCPD1_IRQHandler(void)
{
  /* USER CODE BEGIN UCPD1_IRQn 0 */

  /* USER CODE END UCPD1_IRQn 0 */
  USBPD_PORT0_IRQHandler();

  /* USER CODE BEGIN UCPD1_IRQn 1 */

  /* USER CODE END UCPD1_IRQn 1 */
}

/**
  * @brief This function handles USB1 OTG HS interrupt.
  */
void USB1_OTG_HS_IRQHandler(void)
{
  /* USER CODE BEGIN USB1_OTG_HS_IRQn 0 */

  /* USER CODE END USB1_OTG_HS_IRQn 0 */
  HAL_PCD_IRQHandler(&hpcd_USB_OTG_HS1);
  /* USER CODE BEGIN USB1_OTG_HS_IRQn 1 */

  /* USER CODE END USB1_OTG_HS_IRQn 1 */
}

/* USER CODE BEGIN 1 */
#if defined(TCPP0203_SUPPORT)
/**
  * @brief  This function handles external line interrupt request.
  *         (Associated to FLGn line in case of TCPP0203 management)
  * @retval None
  */
void TCPP0203_PORT0_FLG_EXTI_IRQHANDLER(void)
{
  /* Manage Flags */
  if (TCPP0203_PORT0_FLG_EXTI_IS_ACTIVE_FLAG() != RESET)
  {
    /* Call BSP USBPD PWR callback */
    BSP_USBPD_PWR_EventCallback(USBPD_PWR_TYPE_C_PORT_1);

    /* Clear Flag */
    TCPP0203_PORT0_FLG_EXTI_CLEAR_FLAG();
  }
}
#endif /* TCPP0203_SUPPORT */

/* M3: 카메라 인터럽트(CubeN6 VENC_USB stm32n6xx_it.c와 같음).
   VENC_IRQHandler는 Middlewares/ST/VideoEncoder_EWL/ewl_impl.c에 있다. */
#include "stm32n6570_discovery_camera.h"

void CSI_IRQHandler(void)
{
  HAL_DCMIPP_CSI_IRQHandler(&hcamera_dcmipp);
}

void DCMIPP_IRQHandler(void)
{
  BSP_CAMERA_IRQHandler(0);
}
/* USER CODE END 1 */
