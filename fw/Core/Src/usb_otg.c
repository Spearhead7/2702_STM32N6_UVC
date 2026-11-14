/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usb_otg.c
  * @brief   This file provides code for the configuration
  *          of the USB_OTG instances.
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
#include "usb_otg.h"

/* USER CODE BEGIN 0 */
#include "string.h"
/* Set hpcd_USB1_OTG_HS at the end of non-cacheable */
#if defined ( __ICCARM__ ) /* IAR Compiler */
#pragma location = ".UsbHpcdSection"
#else /* GNU and AC6 Compiler */
__attribute__((section(".UsbHpcdSection")))
#endif
/* USER CODE END 0 */

PCD_HandleTypeDef hpcd_USB_OTG_HS1;

/* USB1_OTG_HS init function */

void MX_USB1_OTG_HS_PCD_Init(void)
{

  /* USER CODE BEGIN USB1_OTG_HS_Init 0 */

  /* USER CODE END USB1_OTG_HS_Init 0 */

  /* USER CODE BEGIN USB1_OTG_HS_Init 1 */
  memset(&hpcd_USB_OTG_HS1, 0x0, sizeof(PCD_HandleTypeDef));
  /* USER CODE END USB1_OTG_HS_Init 1 */
  hpcd_USB_OTG_HS1.Instance = USB1_OTG_HS;
  hpcd_USB_OTG_HS1.Init.dev_endpoints = 9;
  hpcd_USB_OTG_HS1.Init.speed = PCD_SPEED_HIGH;
  hpcd_USB_OTG_HS1.Init.phy_itface = USB_OTG_HS_EMBEDDED_PHY;
  hpcd_USB_OTG_HS1.Init.Sof_enable = DISABLE;
  hpcd_USB_OTG_HS1.Init.low_power_enable = DISABLE;
  hpcd_USB_OTG_HS1.Init.lpm_enable = DISABLE;
  hpcd_USB_OTG_HS1.Init.use_dedicated_ep1 = DISABLE;
  hpcd_USB_OTG_HS1.Init.vbus_sensing_enable = DISABLE;
  /* M3: USB 내부 DMA 사용(x-cube-n6-camera-capture와 같음). slave 모드는 isochronous 영상에서
     패킷을 제때 못 채워 MJPEG 아래쪽이 깨졌다. DMA 버퍼(USBX 풀)는 비캐시 영역에 있어야 한다. */
  hpcd_USB_OTG_HS1.Init.dma_enable = ENABLE;
  if (HAL_PCD_Init(&hpcd_USB_OTG_HS1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USB1_OTG_HS_Init 2 */

  /* FIFO 단위는 32비트 word, 전체 약 1024 word(DMA 모드는 끝부분 일부를 코어가 쓴다).
     합계 0xC0+0x10+0x40+0x10+0x280 = 0x3A0 word (x-cube-n6-camera-capture는 0x390).
     영상 EP는 1024 B x 3/마이크로프레임. DMA가 전송 중에 FIFO를 채우므로 2.5패킷분(0x280)으로 둔다. */

  /* Rx FIFO: 모든 OUT 공용 (CDC bulk OUT 512 B 1개 + SETUP·상태 여유) */
  HAL_PCDEx_SetRxFiFo(&hpcd_USB_OTG_HS1, 0xC0);

  /* Tx FIFO 0: EP0 control */
  HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_HS1, 0, 0x10);

  /* Tx FIFO 1: CDC data IN (0x81). 256 B라 HS 최대 패킷(512 B)보다 작다.
     CLI 출력(ux_device_cdc_acm.c)은 256 B 이하로 나눠 보낸다(M5). */
  HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_HS1, 1, 0x40);

  /* Tx FIFO 2: CDC command IN (0x82) */
  HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_HS1, 2, 0x10);

  /* Tx FIFO 3: UVC video isochronous IN (0x83) */
  HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_HS1, 3, 0x280);

  /* USER CODE END USB1_OTG_HS_Init 2 */

}

void HAL_PCD_MspInit(PCD_HandleTypeDef* pcdHandle)
{

  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  if(pcdHandle->Instance==USB1_OTG_HS)
  {
  /* USER CODE BEGIN USB1_OTG_HS_MspInit 0 */

  /* USER CODE END USB1_OTG_HS_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USBOTGHS1;
    PeriphClkInitStruct.UsbPhy1ClockSelection = RCC_USBPHY1CLKSOURCE_HSE_DIV2;
    PeriphClkInitStruct.UsbOtgHs1ClockSelection = RCC_USBOTGHS1CLKSOURCE_HSE_DIRECT;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    /* Enable VDDUSB */
    HAL_PWREx_EnableVddUSB();
    /* USB1_OTG_HS clock enable */
    __HAL_RCC_USB1_OTG_HS_CLK_ENABLE();
    __HAL_RCC_USB1_OTG_HS_PHY_CLK_ENABLE();

    /* USB1_OTG_HS interrupt Init */
    /* M3: 카메라(DCMIPP/CSI)·VENC 인터럽트(7)보다 높게. slave 모드 isochronous는 125 us 안에 FIFO를 채워야 한다. */
    HAL_NVIC_SetPriority(USB1_OTG_HS_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(USB1_OTG_HS_IRQn);
  /* USER CODE BEGIN USB1_OTG_HS_MspInit 1 */
    /* Reset USB peripherals and configure the HSE clock */
    RESET_USB_MACRO();

    /* Required few clock cycles before accessing USB PHY Controller Registers */
    HAL_Delay(1);

    USB1_HS_PHYC->USBPHYC_CR &= ~(0x7 << 0x4);

    USB1_HS_PHYC->USBPHYC_CR |= (0x1 << 16) |
                                (0x2 << 4)  |
                                (0x1 << 2)  |
                                 0x1U;

    __HAL_RCC_USB1_OTG_HS_PHY_RELEASE_RESET();

    /* Required few clock cycles before Releasing Reset */
    HAL_Delay(1);

    __HAL_RCC_USB1_OTG_HS_RELEASE_RESET();
  /* USER CODE END USB1_OTG_HS_MspInit 1 */
  }
}

void HAL_PCD_MspDeInit(PCD_HandleTypeDef* pcdHandle)
{

  if(pcdHandle->Instance==USB1_OTG_HS)
  {
  /* USER CODE BEGIN USB1_OTG_HS_MspDeInit 0 */

  /* USER CODE END USB1_OTG_HS_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USB1_OTG_HS_CLK_DISABLE();
    __HAL_RCC_USB1_OTG_HS_PHY_CLK_DISABLE();

    /* Disable VDDUSB */
      HAL_PWREx_DisableVddUSB();

    /* USB1_OTG_HS interrupt Deinit */
    HAL_NVIC_DisableIRQ(USB1_OTG_HS_IRQn);
  /* USER CODE BEGIN USB1_OTG_HS_MspDeInit 1 */
    /* Enable VDDUSB */
    HAL_PWREx_EnableVddUSB();
  /* USER CODE END USB1_OTG_HS_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

