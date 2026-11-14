/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    ux_device_video.h
  * @author  MCD Application Team
  * @brief   USBX Device Video applicative header file
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __UX_DEVICE_VIDEO_H__
#define __UX_DEVICE_VIDEO_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "ux_api.h"
#include "ux_device_class_video.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "main.h"
#include "ux_device_descriptors.h"
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
VOID USBD_VIDEO_Activate(VOID *video_instance);
VOID USBD_VIDEO_Deactivate(VOID *video_instance);
VOID USBD_VIDEO_StreamChange(UX_DEVICE_CLASS_VIDEO_STREAM *video_stream,
                             ULONG alternate_setting);
VOID USBD_VIDEO_StreamPayloadDone(UX_DEVICE_CLASS_VIDEO_STREAM *video_stream,
                                  ULONG length);
UINT USBD_VIDEO_StreamRequest(UX_DEVICE_CLASS_VIDEO_STREAM *video_stream,
                              UX_SLAVE_TRANSFER *transfer);
ULONG USBD_VIDEO_StreamGetMaxPayloadBufferSize(VOID);
/* 페이로드 채우기 스레드 생성(USBX 장치 초기화에서 한 번) */
UINT  USBD_VIDEO_FillThreadInit(VOID);

/* USER CODE BEGIN EFP */
/* CLI status 명령용 UVC 상태. fps·지연은 통계 주기(5초) 평균 */
typedef struct
{
  uint8_t  streaming;      /* 호스트가 영상을 열었는지 */
  uint32_t sent_fps_x10;   /* 보낸 프레임 fps × 10 */
  uint32_t skipped;        /* 키프레임 대기로 건너뛴 프레임(누적) */
  uint32_t isoinc;         /* isochronous IN incomplete(누적, 패킷 손실 의심) */
  uint32_t buferr;         /* USBX 영상 버퍼 오류(누적) */
  uint32_t lat_valid;      /* 지연 값이 있는지(스트리밍 중 한 주기 이상) */
  uint32_t lat_avg_us;     /* 장치 내부 지연(캡처 시작 → USB 전송 완료) 평균 */
  uint32_t lat_max_us;
} uvc_status_t;

void uvc_status_get(uvc_status_t *st);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* UVC uses only 26 first bytes */
typedef struct
{
  uint16_t bmHint;
  uint8_t bFormatIndex;
  uint8_t bFrameIndex;
  uint32_t dwFrameInterval;
  uint16_t wKeyFrameRate;
  uint16_t wPFrameRate;
  uint16_t wCompQuality;
  uint16_t wCompWindowSize;
  uint16_t wDelay;
  uint32_t dwMaxVideoFrameSize;
  uint32_t dwMaxPayloadTransferSize;
  uint32_t dwClockFrequency;
  uint8_t bmFramingInfo;
  uint8_t bPreferedVersion;
  uint8_t bMinVersion;
  uint8_t bMaxVersion;
} __PACKED USBD_VideoControlTypeDef;

/* USER CODE END PD */

#ifndef USBD_VIDEO_STREAM_NMNBER
#define USBD_VIDEO_STREAM_NMNBER  1
#endif

#ifndef USBD_VIDEO_PAYLOAD_BUFFER_NUMBER
#define USBD_VIDEO_PAYLOAD_BUFFER_NUMBER  8   /* M4-6: 채우기 스레드가 앞서 채울 여유 */
#endif

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

#ifdef __cplusplus
}
#endif
#endif  /* __UX_DEVICE_VIDEO_H__ */
