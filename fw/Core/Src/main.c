/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "app_threadx.h"
#include "main.h"
#include "gpdma.h"
#include "ucpd.h"
#include "usart.h"
#include "usb_otg.h"
#include "gpio.h"
#include "usbpd.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "stm32n6570_discovery_xspi.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */
extern DMA_HandleTypeDef handle_GPDMA1_Channel2 ;
extern DMA_HandleTypeDef handle_GPDMA1_Channel3 ;

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void SystemIsolation_Config(void);
/* USER CODE BEGIN PFP */
static void PSRAM_Init(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  HAL_PWREx_EnableVddA();
  HAL_PWREx_EnableVddIO2();
  HAL_PWREx_EnableVddIO3();
  HAL_PWREx_EnableVddIO4();
  HAL_PWREx_EnableVddIO5();
  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* Enable the CPU Cache */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_GPDMA1_Init();
  MX_USART1_UART_Init();
  MX_UCPD1_Init();
  MX_USB1_OTG_HS_PCD_Init();
  SystemIsolation_Config();
  /* Call PreOsInit function */
  USBPD_PreInitOs();
  /* USER CODE BEGIN 2 */
  printf("\r\n=== STM32N6 UVC (M4: camera H.264 720p30 HW handshake + CDC echo) ===\r\n");
  /* 외부 PSRAM: 영상 버퍼가 있으므로 ThreadX(영상 스레드) 시작 전에 켠다 */
  PSRAM_Init();
  /* USER CODE END 2 */

  MX_ThreadX_Init();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}
/* USER CODE BEGIN CLK 1 */
/* USER CODE END CLK 1 */

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the System Power Supply
  */
  if (HAL_PWREx_ConfigSupply(PWR_EXTERNAL_SOURCE_SUPPLY) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /* Enable HSI */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL1.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.PLL2.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.PLL3.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.PLL4.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /* Wait HSE stabilization time before its selection as PLL source. */
  HAL_Delay(HSE_STARTUP_TIMEOUT);

  /** Initializes TIMPRE when TIM is used as Systick Clock Source
  */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_TIM;
  PeriphClkInitStruct.TIMPresSelection = RCC_TIMPRES_DIV1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Get current CPU/System buses clocks configuration and if necessary switch
 to intermediate HSI clock to ensure target clock can be set
  */
  HAL_RCC_GetClockConfig(&RCC_ClkInitStruct);
  if ((RCC_ClkInitStruct.CPUCLKSource == RCC_CPUCLKSOURCE_IC1) ||
     (RCC_ClkInitStruct.SYSCLKSource == RCC_SYSCLKSOURCE_IC2_IC6_IC11))
  {
    RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_CPUCLK | RCC_CLOCKTYPE_SYSCLK);
    RCC_ClkInitStruct.CPUCLKSource = RCC_CPUCLKSOURCE_HSI;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct) != HAL_OK)
    {
      /* Initialization Error */
      Error_Handler();
    }
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS_DIGITAL;
  RCC_OscInitStruct.PLL1.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL1.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL1.PLLM = 3;
  RCC_OscInitStruct.PLL1.PLLN = 75;
  RCC_OscInitStruct.PLL1.PLLFractional = 0;
  RCC_OscInitStruct.PLL1.PLLP1 = 1;
  RCC_OscInitStruct.PLL1.PLLP2 = 1;
  RCC_OscInitStruct.PLL2.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.PLL3.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.PLL4.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_CPUCLK|RCC_CLOCKTYPE_HCLK
                              |RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1
                              |RCC_CLOCKTYPE_PCLK2|RCC_CLOCKTYPE_PCLK5
                              |RCC_CLOCKTYPE_PCLK4;
  RCC_ClkInitStruct.CPUCLKSource = RCC_CPUCLKSOURCE_IC1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_IC2_IC6_IC11;
  /* M4-6: HCLK 200 MHz(SYSCLK 400 / 2). VENC가 HCLK로 돌아서 100 MHz에서는 720p H.264 인코딩이
     약 18 ms 걸렸다. CubeN6 VENC_USB·x-cube-n6-ai-h264-usb-uvc도 HCLK = SYSCLK / 2. */
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;
  RCC_ClkInitStruct.APB5CLKDivider = RCC_APB5_DIV1;
  RCC_ClkInitStruct.IC1Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
  RCC_ClkInitStruct.IC1Selection.ClockDivider = 2;
  RCC_ClkInitStruct.IC2Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
  RCC_ClkInitStruct.IC2Selection.ClockDivider = 3;
  RCC_ClkInitStruct.IC6Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
  RCC_ClkInitStruct.IC6Selection.ClockDivider = 3;
  RCC_ClkInitStruct.IC11Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
  RCC_ClkInitStruct.IC11Selection.ClockDivider = 3;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief RIF Initialization Function
  * @param None
  * @retval None
  */
  static void SystemIsolation_Config(void)
{

/* USER CODE BEGIN RIF_Init 0 */

/* USER CODE END RIF_Init 0 */

  /* set all required IPs as secure privileged */
  __HAL_RCC_RIFSC_CLK_ENABLE();

  /*RIMC configuration*/
  RIMC_MasterConfig_t RIMC_master = {0};
  RIMC_master.MasterCID = RIF_CID_1;
  RIMC_master.SecPriv = RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV;
  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_OTG1, &RIMC_master);

  /*RISUP configuration*/
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_OTG1HS , RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_ADC12 , RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);

  /* RIF-Aware IPs Config */

  /* set up GPDMA configuration */
  /* set GPDMA1 channel 2 used by USART1 */
  if (HAL_DMA_ConfigChannelAttributes(&handle_GPDMA1_Channel2,DMA_CHANNEL_SEC|DMA_CHANNEL_PRIV|DMA_CHANNEL_SRC_SEC|DMA_CHANNEL_DEST_SEC)!= HAL_OK )
  {
    Error_Handler();
  }
  /* set GPDMA1 channel 3 used by USART1 */
  if (HAL_DMA_ConfigChannelAttributes(&handle_GPDMA1_Channel3,DMA_CHANNEL_SEC|DMA_CHANNEL_PRIV|DMA_CHANNEL_SRC_SEC|DMA_CHANNEL_DEST_SEC)!= HAL_OK )
  {
    Error_Handler();
  }

  /* set up GPIO configuration */
  HAL_GPIO_ConfigPinAttributes(GPIOA,GPIO_PIN_0,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOA,GPIO_PIN_1,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOC,GPIO_PIN_1,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOC,GPIO_PIN_13,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_5,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_6,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOF,GPIO_PIN_11,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOG,GPIO_PIN_10,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOH,GPIO_PIN_9,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_12,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOO,GPIO_PIN_1,GPIO_PIN_SEC|GPIO_PIN_NPRIV);

/* USER CODE BEGIN RIF_Init 1 */
  /* M3: 카메라(DCMIPP, CSI)와 하드웨어 인코더(VENC). 값은 CubeN6 VENC_USB RISAF_Config()와 같다.
     DCMIPP·VENC는 버스 마스터로 PSRAM을 직접 읽고 쓴다. */
  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_DCMIPP, &RIMC_master);
  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_VENC, &RIMC_master);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_DCMIPP, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_CSI, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_VENC, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);

/* USER CODE END RIF_Init 1 */
/* USER CODE BEGIN RIF_Init 2 */

/* USER CODE END RIF_Init 2 */

}

/* USER CODE BEGIN 4 */
/**
  * @brief  printf 출력을 USART1(ST-LINK 가상 COM, 115200 8N1)로 보낸다. 디버그 로그용.
  */
int __io_putchar(int ch)
{
  uint8_t c = (uint8_t)ch;
  (void)HAL_UART_Transmit(&huart1, &c, 1U, 10U);
  return ch;
}

/**
  * @brief  VDDIO3(XSPI1 포트, PSRAM) 1.8V 고속 패드(HSLV) OTP 퓨즈가 켜져 있는지 읽는다.
  *         읽기만 한다. ST 예제의 FSBL(OTP_Config)은 이 퓨즈를 직접 굽지만(되돌릴 수 없음)
  *         이 펌웨어는 굽지 않는다.
  * @retval 1: 켜짐, 0: 꺼짐 또는 읽기 실패
  */
static uint32_t PSRAM_HslvFused(void)
{
  BSEC_HandleTypeDef hbsec = {0};
  uint32_t data = 0U;

  __HAL_RCC_BSEC_CLK_ENABLE();
  hbsec.Instance = BSEC;
  if (HAL_BSEC_OTP_Read(&hbsec, 124U /* HW config */, &data) != HAL_OK)
  {
    return 0U;
  }
  return ((data & (1UL << 15) /* HSLV_VDDIO3 */) != 0U) ? 1U : 0U;
}

/**
  * @brief  외부 PSRAM(APS256XX 32MB, XSPI1)을 memory-mapped(0x90000000)로 켠다.
  *         XSPI1 클럭은 IC3 = PLL1(1200 MHz)/6 = 200 MHz. CubeN6 VENC_USB는 HCLK(200 MHz)를
  *         썼지만 이 프로젝트는 HCLK와 따로 정하려고 IC3를 쓴다.
  *         HSLV 퓨즈가 꺼져 있으면 패드가 200 MHz를 못 낼 수 있어 100 MHz(/12)로 낮춘다
  *         (영상에 필요한 대역폭 약 120 MB/s는 100 MHz에서도 충분하다).
  *         BSP가 처음엔 1/4 속도로 설정을 쓴 뒤 최종 속도로 올린다.
  */
static void PSRAM_Init(void)
{
  RCC_PeriphCLKInitTypeDef clk = {0};
  volatile uint32_t *p = (volatile uint32_t *)0x90000000U;
  uint32_t errors = 0U;
  uint32_t hslv = PSRAM_HslvFused();

  printf("PSRAM: VDDIO3 HSLV fuse %s\r\n", (hslv != 0U) ? "set" : "NOT set (using 100 MHz)");

  clk.PeriphClockSelection = RCC_PERIPHCLK_XSPI1;
  clk.Xspi1ClockSelection = RCC_XSPI1CLKSOURCE_IC3;
  clk.ICSelection[RCC_IC3].ClockSelection = RCC_ICCLKSOURCE_PLL1;
  clk.ICSelection[RCC_IC3].ClockDivider = (hslv != 0U) ? 6U : 12U;
  if (HAL_RCCEx_PeriphCLKConfig(&clk) != HAL_OK)
  {
    printf("PSRAM: clock config failed\r\n");
    Error_Handler();
  }

  if (BSP_XSPI_RAM_Init(0) != BSP_ERROR_NONE)
  {
    printf("PSRAM: init failed\r\n");
    Error_Handler();
  }
  if (BSP_XSPI_RAM_EnableMemoryMappedMode(0) != BSP_ERROR_NONE)
  {
    printf("PSRAM: memory-mapped mode failed\r\n");
    Error_Handler();
  }

  /* 간단한 읽기/쓰기 확인: 1 MB 간격으로 32곳(32 MB 전체) */
  for (uint32_t i = 0U; i < 32U; i++)
  {
    p[i * (0x100000U / 4U)] = 0xA5000000U | i;
  }
  for (uint32_t i = 0U; i < 32U; i++)
  {
    if (p[i * (0x100000U / 4U)] != (0xA5000000U | i))
    {
      errors++;
    }
  }
  printf("PSRAM: %s (XSPI1 %lu MHz)\r\n", (errors == 0U) ? "OK" : "TEST FAILED",
         (unsigned long)(HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_XSPI1) / 1000000U));
  if (errors != 0U)
  {
    Error_Handler();
  }
}
/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};
  MPU_Attributes_InitTypeDef MPU_AttributesInit = {0};
  uint32_t primask_bit = __get_PRIMASK();
  __disable_irq();

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region 0 and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  /* M3(USB DMA): 비캐시 32K → 64K. USBX 풀(0x341F0000, USB DMA 버퍼)·hpcd·MSP 스택 (링커 참고) */
  MPU_InitStruct.BaseAddress = 0x341F0000;
  MPU_InitStruct.LimitAddress = 0x341FFFFF;
  MPU_InitStruct.AttributesIndex = MPU_ATTRIBUTES_NUMBER0;
  MPU_InitStruct.AccessPermission = MPU_REGION_PRIV_RW;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
  MPU_InitStruct.DisablePrivExec = MPU_PRIV_INSTRUCTION_ACCESS_ENABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /** Initializes and configures the Attribute 0 and the memory to be protected
  */
  MPU_AttributesInit.Number = MPU_ATTRIBUTES_NUMBER0;
  MPU_AttributesInit.Attributes = INNER_OUTER(MPU_NOT_CACHEABLE);

  HAL_MPU_ConfigMemoryAttributes(&MPU_AttributesInit);

  /* M3: 외부 PSRAM 32MB 비캐시. 영상 버퍼는 DCMIPP·VENC가 직접 읽고 쓰므로 캐시 관리를 피한다.
     CubeN6 VENC_USB도 PSRAM을 비캐시로 둔다. */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER1;
  MPU_InitStruct.BaseAddress = 0x90000000;
  MPU_InitStruct.LimitAddress = 0x91FFFFFF;
  MPU_InitStruct.AttributesIndex = MPU_ATTRIBUTES_NUMBER0;
  MPU_InitStruct.AccessPermission = MPU_REGION_ALL_RW;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.DisablePrivExec = MPU_PRIV_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

  /* Exit critical section to lock the system and avoid any issue around MPU mechanism */
  __set_PRIMASK(primask_bit);

}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */
  USBPD_DPM_TimerCounter();
#if defined(_GUI_INTERFACE)
  GUI_TimerCounter();
#endif /* _GUI_INTERFACE */
  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @param None
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  while (1)
  {
    HAL_GPIO_TogglePin(LED_RED_GPIO_Port, LED_RED_Pin);
    HAL_Delay(200);
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* Infinite loop */
  while (1)
  {
  }
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
