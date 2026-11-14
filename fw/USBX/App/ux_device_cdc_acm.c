/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    ux_device_cdc_acm.c
  * @author  MCD Application Team
  * @brief   USBX Device applicative file
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
/* 출처: STM32CubeN6 V1.4.1 USBPD_SNK_Ux_Device_HID_CDC_ACM.
   예제의 USB-UART 브리지를 없애고 대화형 CLI(M5, PRD 7장, App/Src/cli.c)를 붙였다. */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "ux_device_cdc_acm.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "cli.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* 한 번에 읽는 최대 크기: HS bulk 패킷 하나 */
#define APP_RX_DATA_SIZE   512U
/* 배너 스레드: 터미널이 포트를 열면(DTR 0 → 1) 배너와 프롬프트를 보낸다.
   CLI 스레드는 읽기에서 계속 기다리므로(타임아웃을 쓰면 전송 취소 뒤 수신이 멈췄다, M5 보드 확인)
   배너는 따로 보낸다. 쓰기는 USBX가 엔드포인트 뮤텍스로 순서를 지킨다. */
#define CLI_BANNER_STACK_SIZE  1024U
#define CLI_BANNER_PRIO        20U
/* 한 번에 보내는 최대 크기 = CDC data IN Tx FIFO 크기(usb_otg.c 0x40 word) */
#define CLI_TX_CHUNK           256U

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

UX_SLAVE_CLASS_CDC_ACM  *cdc_acm;

static uint8_t UserRxBuffer[APP_RX_DATA_SIZE];

static TX_SEMAPHORE     cli_banner_sem;
static volatile uint8_t cli_banner_ready = 0U;   /* 세마포어가 만들어졌는지 */
static TX_THREAD        cli_banner_thread;
static ULONG            cli_banner_stack[CLI_BANNER_STACK_SIZE / sizeof(ULONG)];
static uint8_t          cli_dtr_prev = 0U;

/* USB에서는 실제 속도와 상관없는 값이다. 호스트가 바꾸면 저장만 한다(PRD 7장). */
UX_SLAVE_CLASS_CDC_ACM_LINE_CODING_PARAMETER CDC_VCP_LineCoding =
{
  115200, /* baud rate */
  0x00,   /* stop bits-1 */
  0x00,   /* parity - none */
  0x08    /* nb. of bits 8 */
};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  USBD_CDC_ACM_Activate
  *         This function is called when insertion of a CDC ACM device.
  * @param  cdc_acm_instance: Pointer to the cdc acm class instance.
  * @retval none
  */
VOID USBD_CDC_ACM_Activate(VOID *cdc_acm_instance)
{
  /* USER CODE BEGIN USBD_CDC_ACM_Activate */

  /* Save the CDC instance */
  cdc_acm = (UX_SLAVE_CLASS_CDC_ACM*) cdc_acm_instance;

  /* Set device class_cdc_acm with default parameters */
  if (ux_device_class_cdc_acm_ioctl(cdc_acm, UX_SLAVE_CLASS_CDC_ACM_IOCTL_SET_LINE_CODING,
                                    &CDC_VCP_LineCoding) != UX_SUCCESS)
  {
    Error_Handler();
  }

  printf("CDC: activate\r\n");

  /* USER CODE END USBD_CDC_ACM_Activate */

  return;
}

/**
  * @brief  USBD_CDC_ACM_Deactivate
  *         This function is called when extraction of a CDC ACM device.
  * @param  cdc_acm_instance: Pointer to the cdc acm class instance.
  * @retval none
  */
VOID USBD_CDC_ACM_Deactivate(VOID *cdc_acm_instance)
{
  /* USER CODE BEGIN USBD_CDC_ACM_Deactivate */
  UX_PARAMETER_NOT_USED(cdc_acm_instance);

  /* Reset the cdc acm instance */
  cdc_acm = UX_NULL;

  printf("CDC: deactivate\r\n");

  /* USER CODE END USBD_CDC_ACM_Deactivate */

  return;
}

/**
  * @brief  USBD_CDC_ACM_ParameterChange
  *         This function is invoked to manage the CDC ACM class requests.
  * @param  cdc_acm_instance: Pointer to the cdc acm class instance.
  * @retval none
  */
VOID USBD_CDC_ACM_ParameterChange(VOID *cdc_acm_instance)
{
  /* USER CODE BEGIN USBD_CDC_ACM_ParameterChange */
  UX_PARAMETER_NOT_USED(cdc_acm_instance);

  ULONG request;
  UX_SLAVE_TRANSFER *transfer_request;
  UX_SLAVE_DEVICE *device;

  /* Get the pointer to the device */
  device = &_ux_system_slave -> ux_system_slave_device;

  /* Get the pointer to the transfer request associated with the control endpoint */
  transfer_request = &device -> ux_slave_device_control_endpoint.ux_slave_endpoint_transfer_request;

  request = *(transfer_request -> ux_slave_transfer_request_setup + UX_SETUP_REQUEST);

  switch (request)
  {
    case UX_SLAVE_CLASS_CDC_ACM_SET_LINE_CODING :

      /* 호스트가 보낸 값을 저장만 한다 */
      if (ux_device_class_cdc_acm_ioctl(cdc_acm, UX_SLAVE_CLASS_CDC_ACM_IOCTL_GET_LINE_CODING,
                                        &CDC_VCP_LineCoding) != UX_SUCCESS)
      {
        Error_Handler();
      }

      break;

    case UX_SLAVE_CLASS_CDC_ACM_GET_LINE_CODING :

      /* Set the Line Coding parameters */
      if (ux_device_class_cdc_acm_ioctl(cdc_acm, UX_SLAVE_CLASS_CDC_ACM_IOCTL_SET_LINE_CODING,
                                        &CDC_VCP_LineCoding) != UX_SUCCESS)
      {
        Error_Handler();
      }

      break;

    case UX_SLAVE_CLASS_CDC_ACM_SET_CONTROL_LINE_STATE :
      /* 터미널이 포트를 열면(DTR 0 → 1) 배너 스레드에 알린다(인터럽트 문맥일 수 있음) */
      if ((cdc_acm != UX_NULL) && (cdc_acm->ux_slave_class_cdc_acm_data_dtr_state != 0U) &&
          (cli_dtr_prev == 0U) && (cli_banner_ready != 0U))
      {
        (void)tx_semaphore_put(&cli_banner_sem);
      }
      cli_dtr_prev = (cdc_acm != UX_NULL) ? cdc_acm->ux_slave_class_cdc_acm_data_dtr_state : 0U;
      break;

    default :
      break;
  }

  /* USER CODE END USBD_CDC_ACM_ParameterChange */

  return;
}

/* USER CODE BEGIN 1 */

/* CLI 출력: CDC bulk IN으로 보낸다(CLI 스레드와 배너 스레드에서 불림) */
static void cdc_cli_write(const uint8_t *data, uint32_t len)
{
  ULONG written;
  UX_SLAVE_CLASS_CDC_ACM *inst = cdc_acm;

  /* CDC data IN의 Tx FIFO가 0x40 word(256 B)라(usb_otg.c, 나머지는 영상 EP 몫) HS 최대 패킷 512 B를
     다 담지 못한다. 256 B보다 큰 패킷을 보내면 전송이 끝나지 않고 멈췄다(M5 보드 확인: help 출력 276 B).
     그래서 256 B 이하로 나눠 보낸다(각각 짧은 패킷 하나로 끝난다). */
  while ((inst != UX_NULL) && (len != 0U))
  {
    uint32_t n = (len > CLI_TX_CHUNK) ? CLI_TX_CHUNK : len;

    if (ux_device_class_cdc_acm_write(inst, (UCHAR *)data, n, &written) != UX_SUCCESS)
    {
      break;
    }
    data += n;
    len -= n;
  }
}

/* 배너 스레드: DTR 상승 알림을 기다렸다가 배너와 프롬프트를 보낸다 */
static VOID cdc_cli_banner_thread_entry(ULONG thread_input)
{
  UX_PARAMETER_NOT_USED(thread_input);

  while (1)
  {
    if (tx_semaphore_get(&cli_banner_sem, TX_WAIT_FOREVER) == TX_SUCCESS)
    {
      /* 터미널이 포트를 연 직후라 조금 기다렸다 보낸다 */
      tx_thread_sleep(MS_TO_TICK(50));
      /* 그사이 여러 번 왔으면 하나로 합친다 */
      while (tx_semaphore_get(&cli_banner_sem, TX_NO_WAIT) == TX_SUCCESS)
      {
      }
      cli_banner();
    }
  }
}

/**
  * @brief  CDC 대화형 CLI 스레드(M5).
  *         읽기는 데이터가 올 때까지 계속 기다린다(M2~M4 에코와 같은 방식).
  *         포트 열기 배너는 배너 스레드가 보낸다.
  * @param  thread_input: Not used
  * @retval none
  */
VOID usbx_cdc_acm_cli_thread_entry(ULONG thread_input)
{
  ULONG actual_length;
  UINT  status;
  UX_SLAVE_DEVICE *device;

  UX_PARAMETER_NOT_USED(thread_input);

  device = &_ux_system_slave->ux_system_slave_device;
  cli_init(cdc_cli_write);

  if ((tx_semaphore_create(&cli_banner_sem, "cdc_banner", 0U) == TX_SUCCESS) &&
      (tx_thread_create(&cli_banner_thread, "cdc_banner", cdc_cli_banner_thread_entry, 0U,
                        cli_banner_stack, sizeof(cli_banner_stack), CLI_BANNER_PRIO,
                        CLI_BANNER_PRIO, TX_NO_TIME_SLICE, TX_AUTO_START) == TX_SUCCESS))
  {
    cli_banner_ready = 1U;
  }

  while (1)
  {
    UX_SLAVE_CLASS_CDC_ACM *inst = cdc_acm;

    if ((device->ux_slave_device_state != UX_DEVICE_CONFIGURED) || (inst == UX_NULL))
    {
      tx_thread_sleep(MS_TO_TICK(10));
      continue;
    }

    status = ux_device_class_cdc_acm_read(inst, UserRxBuffer, APP_RX_DATA_SIZE, &actual_length);
    if ((status == UX_SUCCESS) && (actual_length != 0U))
    {
      cli_input(UserRxBuffer, actual_length);
    }
    else if (status != UX_SUCCESS)
    {
      /* 분리·리셋 등: 잠깐 쉬고 다시 */
      tx_thread_sleep(MS_TO_TICK(10));
    }
  }
}

/* USER CODE END 1 */
