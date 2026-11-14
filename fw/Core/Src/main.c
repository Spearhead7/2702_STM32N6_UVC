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
#include "m6_reset_test.h"
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
/* M6 진단: 부트 ROM이 넘겨준 상태(main 진입 직후 값). 전원 부팅과 리셋 부팅을 비교한다. */
static uint32_t boot_regs[12];

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

/* M6 진단: 리셋 버튼(NRST) 뒤 부팅이 멈추는 위치를 LED로 본다(디버거가 붙지 않음).
   초록 LED1(PO1)은 High 켜짐, 빨강 LED2(PG10)는 Low 켜짐.
     둘 다 꺼짐            : 앱이 시작되지 않음(부트 ROM)
     초록 + 빨강           : SystemClock_Config에서 멈춤
     초록만, 배너 없음     : 주변장치 초기화(GPIO~USBPD)에서 멈춤
     빨강만                : PSRAM 초기화에서 멈춤
     초록만, 배너 있음     : ThreadX까지 진행
     빨강 깜빡임           : Error_Handler */
static void diag_led(uint32_t green, uint32_t red)
{
  HAL_GPIO_WritePin(GPIOO, GPIO_PIN_1, (green != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOG, GPIO_PIN_10, (red != 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static void diag_led_init(void)
{
  GPIO_InitTypeDef g = {0};

  __HAL_RCC_GPIOO_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  g.Mode  = GPIO_MODE_OUTPUT_PP;
  g.Pull  = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_LOW;
  g.Pin   = GPIO_PIN_1;
  HAL_GPIO_Init(GPIOO, &g);
  g.Pin   = GPIO_PIN_10;
  HAL_GPIO_Init(GPIOG, &g);
  diag_led(1U, 1U);
}

/* M6 진단(m6_reset_test.h): 이 단계까지 온 뒤 초록 LED 약 1.5초, 둘 다 끄고 약 0.5초 뒤 IWDG 리셋.
   리셋 뒤 다시 부팅하면 초록 LED가 계속 깜빡이고, 부트 ROM에서 멈추면 LED가 꺼진 채로 있다. */
static void m6_reset_test(uint32_t stage)
{
  if ((M6_RESET_TEST == 0) || (stage != (uint32_t)M6_RESET_TEST))
  {
    return;
  }
  diag_led(1U, 0U);
  for (volatile uint32_t i = 0U; i < 30000000U; i++)
  {
  }
  diag_led(0U, 0U);
  for (volatile uint32_t i = 0U; i < 10000000U; i++)
  {
  }
  __disable_irq();
  IWDG->KR  = 0xCCCCU;
  IWDG->KR  = 0x5555U;
  IWDG->PR  = 0U;
  IWDG->RLR = 1U;
  while (IWDG->SR != 0U)
  {
  }
  IWDG->KR  = 0xAAAAU;
  while (1)
  {
  }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  boot_regs[0]  = RCC->RSR;
  boot_regs[1]  = PWR->CR1;
  boot_regs[2]  = PWR->VOSCR;
  boot_regs[3]  = PWR->SVMCR3;
  boot_regs[4]  = RCC->CR;
  boot_regs[5]  = RCC->SR;
  boot_regs[6]  = RCC->CFGR1;
  boot_regs[7]  = RCC->PLL1CFGR1;
  boot_regs[8]  = RCC->IC1CFGR;
  boot_regs[9]  = RCC->DIVENR;
  boot_regs[10] = RCC->AHB5ENR;
  boot_regs[11] = RCC->CCIPR6;
  diag_led_init();
  m6_reset_test(1U);
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
  diag_led(1U, 0U);
  m6_reset_test(2U);

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  diag_led(1U, 0U);   /* MX_GPIO_Init이 LED를 끄므로 다시 켠다 */
  MX_GPDMA1_Init();
  MX_USART1_UART_Init();
  MX_UCPD1_Init();
  MX_USB1_OTG_HS_PCD_Init();
  SystemIsolation_Config();
  /* Call PreOsInit function */
  USBPD_PreInitOs();
  /* USER CODE BEGIN 2 */
  printf("\r\n=== STM32N6 UVC (M4: camera H.264 720p30 HW handshake + CDC echo) ===\r\n");
  {
    uint32_t f = boot_regs[0];
    printf("RESET: RSR 0x%08lx%s%s%s%s%s%s%s\r\n", (unsigned long)f,
           ((f & RCC_RSR_PORRSTF) != 0U) ? " POR" : "",
           ((f & RCC_RSR_BORRSTF) != 0U) ? " BOR" : "",
           ((f & RCC_RSR_PINRSTF) != 0U) ? " PIN" : "",
           ((f & RCC_RSR_SFTRSTF) != 0U) ? " SFT" : "",
           ((f & RCC_RSR_IWDGRSTF) != 0U) ? " IWDG" : "",
           ((f & RCC_RSR_WWDGRSTF) != 0U) ? " WWDG" : "",
           ((f & RCC_RSR_LCKRSTF) != 0U) ? " LOCKUP" : "");
    /* 리셋 플래그는 지우지 않는다: 지우면 리셋 버튼·reboot 첫 번째부터 부트 ROM이 멈췄다(M6 보드 확인) */
    printf("BOOT: PWR CR1 %08lx VOSCR %08lx SVMCR3 %08lx\r\n",
           (unsigned long)boot_regs[1], (unsigned long)boot_regs[2], (unsigned long)boot_regs[3]);
    printf("BOOT: RCC CR %08lx SR %08lx CFGR1 %08lx PLL1 %08lx IC1 %08lx\r\n",
           (unsigned long)boot_regs[4], (unsigned long)boot_regs[5], (unsigned long)boot_regs[6],
           (unsigned long)boot_regs[7], (unsigned long)boot_regs[8]);
    printf("BOOT: RCC DIVENR %08lx AHB5ENR %08lx CCIPR6 %08lx\r\n",
           (unsigned long)boot_regs[9], (unsigned long)boot_regs[10], (unsigned long)boot_regs[11]);
  }
  /* 외부 PSRAM: 영상 버퍼가 있으므로 ThreadX(영상 스레드) 시작 전에 켠다 */
  m6_reset_test(3U);
  diag_led(0U, 1U);
  PSRAM_Init();
  diag_led(1U, 0U);
  m6_reset_test(4U);
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
  /* M6: 예제(USBPD_SNK FSBL)는 여기서 PWR_EXTERNAL_SOURCE_SUPPLY(내부 SMPS 끔)를 썼다. 이 설정은
     POR 뒤 한 번만 쓸 수 있고 리셋으로는 지워지지 않는다(HAL 주석). 플래시 부팅에서 리셋 버튼·reboot 뒤
     부트 ROM이 멈춰(LED 진단: 앱 시작 전) 의심한다. x-cube-n6-camera-capture(같은 DK)는 이 호출이 없다.
     DK의 VDDCORE는 외부 SMPS(BSP_SMPS_Init 핀)라 부르지 않아도 동작한다. */
#if 0
  if (HAL_PWREx_ConfigSupply(PWR_EXTERNAL_SOURCE_SUPPLY) != HAL_OK)
  {
    Error_Handler();
  }
#endif

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
  * @brief  소프트웨어 리셋 직전 처리(M6).
  *         VDDIO2(PSRAM 패드) 1.8 V 범위 비트는 POR로만 초기화되고, 1로 남은 채 리셋되면
  *         부트 ROM이 멈춘다(분할 시험 5). 부트 ROM이 넘겨주는 기본값(3.3 V 범위)으로 되돌린다.
  *         되돌린 뒤에는 PSRAM 접근이 끝나지 않으므로 인터럽트를 끈 채 바로 리셋해야 한다.
  *         리셋 버튼(NRST)은 이 처리를 거치지 않으므로 여전히 전원을 다시 넣어야 한다.
  */
void board_prepare_reset(void)
{
  HAL_PWREx_ConfigVddIORange(PWR_VDDIO2, PWR_VDDIO_RANGE_3V3);
  __DSB();
}

/**
  * @brief  printf 출력을 USART1(ST-LINK 가상 COM, 115200 8N1)로 보낸다. 디버그 로그용.
  */
int __io_putchar(int ch)
{
  uint8_t c = (uint8_t)ch;
  (void)HAL_UART_Transmit(&huart1, &c, 1U, 10U);
  return ch;
}

/* 1.8 V 고속 패드(HSLV) OTP 퓨즈(OTP 124). ST 예제 기준 VDDIO2 = XSPIM Port 1(PSRAM, XSPI1),
   VDDIO3 = Port 2(NOR 플래시, XSPI2). M6 전까지는 PSRAM 쪽을 VDDIO3 비트로 잘못 읽고 있었다. */
#define OTP_HW_CONFIG       124U
#define OTP_HSLV_VDDIO3     (1UL << 15)   /* NOR(XSPI2) */
#define OTP_HSLV_VDDIO2     (1UL << 16)   /* PSRAM(XSPI1) */

/**
  * @brief  HSLV OTP 워드를 읽는다. 읽기만 한다. ST 예제의 FSBL(OTP_Config)은 이 퓨즈를 직접 굽지만
  *         (되돌릴 수 없음) 이 펌웨어는 굽지 않는다.
  * @retval OTP 124 값(읽기 실패 시 0)
  */
static uint32_t OTP_HslvRead(void)
{
  BSEC_HandleTypeDef hbsec = {0};
  uint32_t data = 0U;

  __HAL_RCC_BSEC_CLK_ENABLE();
  hbsec.Instance = BSEC;
  if (HAL_BSEC_OTP_Read(&hbsec, OTP_HW_CONFIG, &data) != HAL_OK)
  {
    return 0U;
  }
  return data;
}

/**
  * @brief  외부 PSRAM(APS256XX 32MB, XSPI1)을 memory-mapped(0x90000000)로 켠다.
  *         XSPI1 클럭은 IC3 = PLL1(1200 MHz)/6 = 200 MHz. CubeN6 VENC_USB는 HCLK(200 MHz)를
  *         썼지만 이 프로젝트는 HCLK와 따로 정하려고 IC3를 쓴다.
  *         VDDIO2 HSLV 퓨즈가 꺼져 있으면 3.3 V 범위 패드로 200 MHz를 못 낼 수 있어 100 MHz(/12)로 낮춘다
  *         (영상에 필요한 대역폭 약 120 MB/s는 100 MHz에서도 충분하다).
  *         BSP가 처음엔 1/4 속도로 설정을 쓴 뒤 최종 속도로 올린다.
  */
static void PSRAM_Init(void)
{
  RCC_PeriphCLKInitTypeDef clk = {0};
  volatile uint32_t *p = (volatile uint32_t *)0x90000000U;
  uint32_t errors = 0U;
  uint32_t otp = OTP_HslvRead();
  uint32_t hslv = ((otp & OTP_HSLV_VDDIO2) != 0U) ? 1U : 0U;

  printf("PSRAM: HSLV fuse VDDIO2(PSRAM) %s, VDDIO3(NOR) %s\r\n",
         (hslv != 0U) ? "set" : "NOT set (100 MHz)",
         ((otp & OTP_HSLV_VDDIO3) != 0U) ? "set" : "not set");

  clk.PeriphClockSelection = RCC_PERIPHCLK_XSPI1;
  clk.Xspi1ClockSelection = RCC_XSPI1CLKSOURCE_IC3;
  clk.ICSelection[RCC_IC3].ClockSelection = RCC_ICCLKSOURCE_PLL1;
  clk.ICSelection[RCC_IC3].ClockDivider = (hslv != 0U) ? 6U : 12U;
  if (HAL_RCCEx_PeriphCLKConfig(&clk) != HAL_OK)
  {
    printf("PSRAM: clock config failed\r\n");
    Error_Handler();
  }

  /* M6 분할 시험 5: PSRAM 쪽 IO 전압 범위(VDDIO2 1.8 V, BSP MSP와 같은 설정)만 한 뒤 리셋 */
  if (M6_RESET_TEST == 5)
  {
    HAL_PWREx_EnableVddIO2();
    HAL_PWREx_ConfigVddIORange(PWR_VDDIO2, PWR_VDDIO_RANGE_1V8);
    m6_reset_test(5U);
  }

  if (BSP_XSPI_RAM_Init(0) != BSP_ERROR_NONE)
  {
    printf("PSRAM: init failed\r\n");
    Error_Handler();
  }
  /* BSP MSP는 VDDIO2를 1.8 V 범위(PWR SVMCR3 VDDIO2VRSEL = 1)로 둔다. PSRAM은 이 범위에서만 동작한다
     (3.3 V 범위로 바꾸면 100·200 MHz 모두 접근이 멈췄다). 그런데 이 비트는 POR로만 초기화되고, 1로
     남은 채 리셋되면 부트 ROM이 멈춘다(M6 분할 시험 5). 그래서 리셋 직전에 board_prepare_reset()으로
     3.3 V 범위로 되돌린다. */
  /* M6 분할 시험 6: PSRAM 칩 설정(지연·x16 모드)까지, memory-mapped 전 */
  m6_reset_test(6U);
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
  /* M6: VDDIO2를 3.3 V 범위로 둔 채 200 MHz가 안정적인지 보려고 앞 4 MB를 주소 패턴으로 쓰고 읽는다 */
  for (uint32_t i = 0U; i < (0x400000U / 4U); i++)
  {
    p[i] = (i * 4U) ^ 0x5AA55AA5U;
  }
  for (uint32_t i = 0U; i < (0x400000U / 4U); i++)
  {
    if (p[i] != ((i * 4U) ^ 0x5AA55AA5U))
    {
      errors++;
    }
  }
  printf("PSRAM: %s (XSPI1 %lu MHz, VDDIO2 %s range, 4 MB test errors %lu)\r\n",
         (errors == 0U) ? "OK" : "TEST FAILED",
         (unsigned long)(HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_XSPI1) / 1000000U),
         ((PWR->SVMCR3 & PWR_SVMCR3_VDDIO2VRSEL) != 0U) ? "1.8 V" : "3.3 V", (unsigned long)errors);
  if (errors != 0U)
  {
    Error_Handler();
  }

  /* M6 분할 시험 7: PSRAM을 memory-mapped에서 빼고 칩을 Global Reset(기본 x8 모드)한 뒤 리셋.
     시험 8: 7 + VDDIO2 범위를 3.3 V(기본값)로 되돌린 뒤 리셋. */
  if ((M6_RESET_TEST == 7) || (M6_RESET_TEST == 8))
  {
    (void)BSP_XSPI_RAM_DisableMemoryMappedMode(0);
    (void)APS256XX_Reset(&hxspi_ram[0]);
    if (M6_RESET_TEST == 8)
    {
      HAL_PWREx_ConfigVddIORange(PWR_VDDIO2, PWR_VDDIO_RANGE_3V3);
    }
    m6_reset_test((uint32_t)M6_RESET_TEST);
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
