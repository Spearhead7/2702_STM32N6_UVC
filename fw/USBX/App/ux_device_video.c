/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    ux_device_video.c
  * @author  MCD Application Team
  * @brief   USBX Device Video applicative source file
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
#include "ux_device_video.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "vframe_queue.h"
#include "vtime.h"
#include <stdio.h>
#include "fast_copy.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define UVC_PLAY_STATUS_STOP       0x00U
#define UVC_PLAY_STATUS_STREAMING  0x02U

/* UVC 페이로드 헤더(2바이트): bHeaderLength, bmHeaderInfo */
#define UVC_HDR_LEN                2U
#define UVC_HDR_FID                0x01U   /* 프레임마다 0/1 토글 */
#define UVC_HDR_EOF                0x02U   /* 프레임 마지막 페이로드 */
#define UVC_HDR_EOH                0x80U

/* 스트림 시작 때 미리 채워 두는 페이로드 수(USBD_VIDEO_PAYLOAD_BUFFER_NUMBER보다 작게) */
#define UVC_PREFILL_PAYLOADS       2U
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
ULONG uvc_state;
UX_DEVICE_CLASS_VIDEO *video;
UX_DEVICE_CLASS_VIDEO_STREAM *stream_write;

/* 프레임 큐에서 빌려 와 보내는 중인 프레임 */
static vfq_frame_t *uvc_frame = UX_NULL;
static uint32_t     uvc_frame_off = 0U;
static uint32_t     uvc_last_seq = 0U;
static uint8_t      uvc_fid = 0U;

/* 통계(디버그용) */
volatile uint32_t   uvc_frames_sent = 0U;
volatile uint32_t   uvc_empty_payloads = 0U;
volatile uint32_t   uvc_payload_get_fail = 0U;
extern volatile ULONG ux_dcd_stm32_iso_in_incomplete_count;   /* ux_dcd_stm32_callback.c */

/* 지연 측정(PRD 8장: 장치 내부 = 캡처 시작 → USB 전송 완료 50 ms 이하).
   구간(vtime 사이클 → us):
     cap   = 캡처 시작(VSYNC) → 캡처 완료
     enc   = 캡처 완료 → 인코딩 완료(인코더 스레드 대기 + 인코딩)
     wait  = 인코딩 완료 → UVC가 프레임을 가져감(앞 프레임 전송 대기)
     usb   = UVC가 가져감 → 마지막(EOF) 페이로드 전송 완료
     total = 캡처 시작 → 마지막 페이로드 전송 완료
   페이로드는 미리 채워 두므로(UVC_PREFILL_PAYLOADS) EOF 페이로드 '작성'이 아니라
   그 페이로드의 '전송 완료' 콜백 시각을 쓴다. */
#define UVC_LAT_NUM    5U
static uint32_t uvc_pay_written = 0U;   /* commit한 페이로드 수 */
static uint32_t uvc_pay_done    = 0U;   /* 전송 완료 콜백 수 */
static uint32_t uvc_usb_start   = 0U;   /* 지금 보내는 프레임을 가져온 시각 */
static struct
{
  uint8_t  valid;
  uint32_t idx;                          /* EOF 페이로드 번호(uvc_pay_written 기준) */
  uint32_t t_cap_start, t_cap_end, t_enc_end, t_usb_start;
} uvc_lat_pend;
static struct
{
  uint32_t n;
  uint32_t sum_us[UVC_LAT_NUM];
  uint32_t max_us[UVC_LAT_NUM];
} uvc_lat;
static VOID uvc_lat_on_payload_done(VOID);

/* Video Probe data structure */
static USBD_VideoControlTypeDef video_Probe_Control =
{
  .bmHint = 0x0000U,
  .bFormatIndex = 0x01U,
  .bFrameIndex = 0x01U,
  .dwFrameInterval = UVC_INTERVAL(UVC_CAM_FPS_FS),
  .wKeyFrameRate = 0x0000U,
  .wPFrameRate = 0x0000U,
  .wCompQuality = 0x0000U,
  .wCompWindowSize = 0x0000U,
  .wDelay = 0x0000U,
  .dwMaxVideoFrameSize = 0x0000U,
  .dwMaxPayloadTransferSize = 0x00000000U,
  .dwClockFrequency = 0x00000000U,
  .bmFramingInfo = 0x00U,
  .bPreferedVersion = 0x00U,
  .bMinVersion = 0x00U,
  .bMaxVersion = 0x00U,
};

/* Video Commit data structure */
static USBD_VideoControlTypeDef video_Commit_Control =
{
  .bmHint = 0x0000U,
  .bFormatIndex = 0x01U,
  .bFrameIndex = 0x01U,
  .dwFrameInterval = UVC_INTERVAL(UVC_CAM_FPS_FS),
  .wKeyFrameRate = 0x0000U,
  .wPFrameRate = 0x0000U,
  .wCompQuality = 0x0000U,
  .wCompWindowSize = 0x0000U,
  .wDelay = 0x0000U,
  .dwMaxVideoFrameSize = 0x0000U,
  .dwMaxPayloadTransferSize = 0x00000000U,
  .dwClockFrequency = 0x00000000U,
  .bmFramingInfo = 0x00U,
  .bPreferedVersion = 0x00U,
  .bMinVersion = 0x00U,
  .bMaxVersion = 0x00U,
};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
static VOID video_write_payload(UX_DEVICE_CLASS_VIDEO_STREAM *stream);
static VOID video_stream_reset(VOID);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  USBD_VIDEO_Activate
  *         This function is called when insertion of a Video device.
  * @param  video_instance: Pointer to the video class instance.
  * @retval none
  */
VOID USBD_VIDEO_Activate(VOID *video_instance)
{
  /* USER CODE BEGIN USBD_VIDEO_Activate */

  /* Save the video instance */
  video = (UX_DEVICE_CLASS_VIDEO*) video_instance;

  /* Get the streams instances */
  ux_device_class_video_stream_get(video, 0, &stream_write);

  /* USER CODE END USBD_VIDEO_Activate */

  return;
}

/**
  * @brief  USBD_VIDEO_Deactivate
  *         This function is called when extraction of a Video device.
  * @param  video_instance: Pointer to the video class instance.
  * @retval none
  */
VOID USBD_VIDEO_Deactivate(VOID *video_instance)
{
  /* USER CODE BEGIN USBD_VIDEO_Deactivate */
  UX_PARAMETER_NOT_USED(video_instance);

  /* Reset the video instance */
  video = UX_NULL;

  /* Reset the video streams */
  stream_write = UX_NULL;

  uvc_state = UVC_PLAY_STATUS_STOP;
  video_stream_reset();

  /* USER CODE END USBD_VIDEO_Deactivate */

  return;
}

/**
  * @brief  USBD_VIDEO_StreamChange
  *         This function is invoked to inform application that the
  *         alternate setting are changed.
  * @param  video_stream: Pointer to video class stream instance.
  * @param  alternate_setting: interface alternate setting.
  * @retval none
  */
VOID USBD_VIDEO_StreamChange(UX_DEVICE_CLASS_VIDEO_STREAM *video_stream,
                             ULONG alternate_setting)
{
  /* USER CODE BEGIN USBD_VIDEO_StreamChange */

  /* 호스트가 영상을 닫음(대체 설정 0) */
  if (alternate_setting == 0U)
  {
    uvc_state = UVC_PLAY_STATUS_STOP;
    video_stream_reset();
    return;
  }

  /* 영상 시작: 가장 최근 프레임부터 보낸다 */
  video_stream_reset();
  uvc_state = UVC_PLAY_STATUS_STREAMING;

  for (ULONG i = 0U; i < UVC_PREFILL_PAYLOADS; i++)
  {
    video_write_payload(video_stream);
  }

  /* Start sending valid payloads in the Video class */
  ux_device_class_video_transmission_start(video_stream);

  /* USER CODE END USBD_VIDEO_StreamChange */

  return;
}

/**
  * @brief  USBD_VIDEO_StreamPayloadDone
  *         This function is invoked when stream data payload received.
  * @param  video_stream: Pointer to video class stream instance.
  * @param  length: transfer length.
  * @retval none
  */
VOID USBD_VIDEO_StreamPayloadDone(UX_DEVICE_CLASS_VIDEO_STREAM *video_stream,
                                  ULONG length)
{
  /* USER CODE BEGIN USBD_VIDEO_StreamPayloadDone */
  UX_PARAMETER_NOT_USED(length);

  uvc_lat_on_payload_done();

  /* 페이로드 하나가 나갈 때마다 하나를 채운다(보낼 프레임이 없으면 헤더만) */
  if (uvc_state == UVC_PLAY_STATUS_STREAMING)
  {
    video_write_payload(video_stream);
  }
  /* USER CODE END USBD_VIDEO_StreamPayloadDone */

  return;
}

/**
  * @brief  USBD_VIDEO_StreamRequest
  *         This function is invoked to manage the UVC class requests.
  * @param  video_stream: Pointer to video class stream instance.
  * @param  transfer: Pointer to the transfer request.
  * @retval status
  */
UINT USBD_VIDEO_StreamRequest(UX_DEVICE_CLASS_VIDEO_STREAM *video_stream,
                              UX_SLAVE_TRANSFER *transfer)
{
   UINT status  = UX_SUCCESS;

  /* USER CODE BEGIN USBD_VIDEO_StreamRequest */
  UCHAR *data, bRequest;
  USHORT wValue_CS, wLength;

  /* Decode setup packet */
  bRequest = transfer -> ux_slave_transfer_request_setup[UX_SETUP_REQUEST];
  wValue_CS = transfer -> ux_slave_transfer_request_setup[UX_SETUP_VALUE + 1];
  wLength = ux_utility_short_get(transfer -> ux_slave_transfer_request_setup + UX_SETUP_LENGTH);
  data = transfer -> ux_slave_transfer_request_data_pointer;

  /* Check CS */
  switch(wValue_CS)
  {
    case UX_DEVICE_CLASS_VIDEO_VS_PROBE_CONTROL:

      switch(bRequest)
      {
        case UX_DEVICE_CLASS_VIDEO_SET_CUR:

          status = UX_SUCCESS;

          break;

        case UX_DEVICE_CLASS_VIDEO_GET_DEF:
        case UX_DEVICE_CLASS_VIDEO_GET_CUR:
        case UX_DEVICE_CLASS_VIDEO_GET_MIN:
        case UX_DEVICE_CLASS_VIDEO_GET_MAX:

          /* Update bPreferedVersion, bMinVersion and bMaxVersion which must be set only by Device */
          video_Probe_Control.bPreferedVersion = 0x00U;
          video_Probe_Control.bMinVersion = 0x00U;
          video_Probe_Control.bMaxVersion = 0x00U;
          video_Probe_Control.dwMaxVideoFrameSize = UVC_MAX_FRAME_SIZE;
          video_Probe_Control.dwClockFrequency = 0x02DC6C00U;

          /* Check device speed */
          if(_ux_system_slave->ux_system_slave_speed == UX_FULL_SPEED_DEVICE)
          {
            video_Probe_Control.dwFrameInterval = (UVC_INTERVAL(UVC_CAM_FPS_FS));
            video_Probe_Control.dwMaxPayloadTransferSize = USBD_VIDEO_EPIN_FS_MPS;
          }
          else
          {
            video_Probe_Control.dwFrameInterval = (UVC_INTERVAL(UVC_CAM_FPS_HS));
            video_Probe_Control.dwMaxPayloadTransferSize = USBD_VIDEO_HS_PAYLOAD_SIZE;
          }

          /* Copy data for transfer */
          ux_utility_memory_copy(data, (VOID *)&video_Probe_Control, sizeof(USBD_VideoControlTypeDef));

          /* Transfer request */
          status = ux_device_stack_transfer_request(transfer, UX_MIN(wLength, sizeof(USBD_VideoControlTypeDef)),
                                                    UX_MIN(wLength, sizeof(USBD_VideoControlTypeDef)));

          break;

        default:
          break;
      }
      break;

    case UX_DEVICE_CLASS_VIDEO_VS_COMMIT_CONTROL:

      /* Check request length */
      if (wLength < 26)
      {
        break;
      }

      switch(bRequest)
      {
        case UX_DEVICE_CLASS_VIDEO_SET_CUR:

          status = UX_SUCCESS;

          break;

        case UX_DEVICE_CLASS_VIDEO_GET_CUR:

          /* Check device speed */
          if(_ux_system_slave->ux_system_slave_speed == UX_FULL_SPEED_DEVICE)
          {
            video_Commit_Control.dwFrameInterval = (UVC_INTERVAL(UVC_CAM_FPS_FS));
            video_Commit_Control.dwMaxPayloadTransferSize = USBD_VIDEO_EPIN_FS_MPS;
          }
          else
          {
            video_Commit_Control.dwFrameInterval = (UVC_INTERVAL(UVC_CAM_FPS_HS));
            /* 예제는 USBD_VIDEO_EPIN_HS_BINTERVAL(=1)을 넣고 있었다. 패킷 크기(MPS)가 맞다. */
            video_Commit_Control.dwMaxPayloadTransferSize = USBD_VIDEO_HS_PAYLOAD_SIZE;
          }

          /* Copy data for transfer */
          ux_utility_memory_copy(data, (VOID *)&video_Commit_Control, 26);

          /* Send transfer request */
          status = ux_device_stack_transfer_request(transfer,
                                                    UX_MIN(wLength, sizeof(USBD_VideoControlTypeDef)),
                                                    UX_MIN(wLength, sizeof(USBD_VideoControlTypeDef)));

          break;

        default:
          break;

      }

      break;

      default:
        status = UX_ERROR;
        break;
  }

  /* USER CODE END USBD_VIDEO_StreamRequest */

  return status;
}

/**
  * @brief  USBD_VIDEO_StreamGetMaxPayloadBufferSize
  *         Get video stream max payload buffer size.
  * @param  none
  * @retval max payload
  */
ULONG USBD_VIDEO_StreamGetMaxPayloadBufferSize(VOID)
{
  ULONG max_playload = 0U;

  /* USER CODE BEGIN USBD_VIDEO_StreamGetMaxPayloadBufferSize */

  max_playload = USBD_VIDEO_HS_PAYLOAD_SIZE;

  /* USER CODE END USBD_VIDEO_StreamGetMaxPayloadBufferSize */

  return max_playload;
}

/* USER CODE BEGIN 1 */

/**
  * @brief  보내던 프레임을 돌려주고 스트림 상태를 처음으로 돌린다.
  */
static VOID video_stream_reset(VOID)
{
  if (uvc_frame != UX_NULL)
  {
    vfq_release(uvc_frame);
    uvc_frame = UX_NULL;
  }
  uvc_frame_off = 0U;
  uvc_pay_written = 0U;
  uvc_pay_done = 0U;
  uvc_lat_pend.valid = 0U;
}

/**
  * @brief  video_write_payload
  *         페이로드 하나(최대 MPS)를 만든다. 프레임 큐에서 가장 최근 JPEG을 빌려
  *         (MPS - 2) 바이트씩 잘라 보내고, 마지막 조각에 EOF를 붙인다.
  *         보낼 프레임이 없으면 헤더만 보낸다(호스트는 빈 페이로드를 무시한다).
  *         USBX 영상 스레드 또는 USB 인터럽트(스트림 시작) 문맥에서 불린다.
  * @param  stream : Video class stream instance.
  * @retval none
  */
static VOID video_write_payload(UX_DEVICE_CLASS_VIDEO_STREAM *stream)
{
  ULONG buffer_length;
  UCHAR *buffer;
  ULONG mps;
  ULONG chunk = 0U;
  UCHAR info;

  if (uvc_state != UVC_PLAY_STATUS_STREAMING)
  {
    return;
  }

  if (ux_device_class_video_write_payload_get(stream, &buffer, &buffer_length) != UX_SUCCESS)
  {
    uvc_payload_get_fail++;
    return;
  }

  /* 한 페이로드 최대 = 패킷 크기 x 마이크로프레임당 패킷 수(wMaxPacketSize bit 12:11 + 1) */
  {
    ULONG wmps = stream->ux_device_class_video_stream_endpoint->ux_slave_endpoint_descriptor.wMaxPacketSize;
    mps = (wmps & 0x7FFU) * (((wmps >> 11) & 0x3U) + 1U);
  }
  if (mps > buffer_length)
  {
    mps = buffer_length;
  }

  /* 새 프레임 시작 */
  if (uvc_frame == UX_NULL)
  {
    uvc_frame = vfq_acquire_latest(uvc_last_seq);
    if (uvc_frame != UX_NULL)
    {
      uvc_frame_off = 0U;
      uvc_fid ^= UVC_HDR_FID;
      uvc_usb_start = vtime_now();
    }
  }

  info = (UCHAR)(UVC_HDR_EOH | uvc_fid);

  if (uvc_frame != UX_NULL)
  {
    chunk = uvc_frame->size - uvc_frame_off;
    /* 조각 크기를 4의 배수로(3068 B) 해서 다음 조각의 원본 주소도 4바이트 정렬로 유지한다 */
    if (chunk > ((mps - UVC_HDR_LEN) & ~3U))
    {
      chunk = (mps - UVC_HDR_LEN) & ~3U;
    }

    /* PSRAM(비캐시) → USBX 페이로드 버퍼. 워드 단위 복사(fast_copy.h) */
    fast_memcpy(buffer + UVC_HDR_LEN, uvc_frame->data + uvc_frame_off, chunk);
    uvc_frame_off += chunk;

    if (uvc_frame_off >= uvc_frame->size)
    {
      info |= UVC_HDR_EOF;
      uvc_last_seq = uvc_frame->seq;
      /* 이 페이로드(EOF)가 전송 완료되면 지연을 기록한다 */
      uvc_lat_pend.idx         = uvc_pay_written + 1U;
      uvc_lat_pend.t_cap_start = uvc_frame->t_cap_start;
      uvc_lat_pend.t_cap_end   = uvc_frame->t_cap_end;
      uvc_lat_pend.t_enc_end   = uvc_frame->t_enc_end;
      uvc_lat_pend.t_usb_start = uvc_usb_start;
      uvc_lat_pend.valid       = 1U;
      vfq_release(uvc_frame);
      uvc_frame = UX_NULL;
      uvc_frames_sent++;
    }
  }
  else
  {
    uvc_empty_payloads++;
  }

  buffer[0] = (UCHAR)UVC_HDR_LEN;
  buffer[1] = info;

  ux_device_class_video_write_payload_commit(stream, UVC_HDR_LEN + chunk);
  uvc_pay_written++;
}

/**
  * @brief  페이로드 전송 완료마다 불린다. 기다리던 EOF 페이로드면 프레임 지연을 누적한다.
  */
static VOID uvc_lat_on_payload_done(VOID)
{
  uint32_t now;
  uint32_t us[UVC_LAT_NUM];
  uint32_t i;
  UX_INTERRUPT_SAVE_AREA

  uvc_pay_done++;
  if ((uvc_lat_pend.valid == 0U) || ((int32_t)(uvc_pay_done - uvc_lat_pend.idx) < 0))
  {
    return;
  }
  uvc_lat_pend.valid = 0U;
  if ((uvc_lat_pend.t_cap_start == 0U) || (uvc_lat_pend.t_cap_end == 0U))
  {
    return;   /* 캡처 시각이 기록되지 않은 프레임 */
  }

  now   = vtime_now();
  us[0] = vtime_us(uvc_lat_pend.t_cap_end   - uvc_lat_pend.t_cap_start);
  us[1] = vtime_us(uvc_lat_pend.t_enc_end   - uvc_lat_pend.t_cap_end);
  us[2] = vtime_us(uvc_lat_pend.t_usb_start - uvc_lat_pend.t_enc_end);
  us[3] = vtime_us(now                      - uvc_lat_pend.t_usb_start);
  us[4] = vtime_us(now                      - uvc_lat_pend.t_cap_start);

  UX_DISABLE
  uvc_lat.n++;
  for (i = 0U; i < UVC_LAT_NUM; i++)
  {
    uvc_lat.sum_us[i] += us[i];
    if (us[i] > uvc_lat.max_us[i])
    {
      uvc_lat.max_us[i] = us[i];
    }
  }
  UX_RESTORE
}

/**
  * @brief  video_app이 5초마다 부르는 통계 훅(video_app.c의 약한 정의를 덮어쓴다).
  *         sent: 호스트로 다 보낸 프레임 수/초, isoinc: USB가 제때 못 보낸 패킷 누적,
  *         buferr: USBX 페이로드 버퍼 부족 누적, getfail: 페이로드 버퍼 얻기 실패 누적.
  */
void video_stats_hook(uint32_t period_ms)
{
  static uint32_t prev_sent = 0U;
  uint32_t sent = uvc_frames_sent;
  uint32_t d = sent - prev_sent;
  ULONG buferr = 0U;

  prev_sent = sent;
  if (stream_write != UX_NULL)
  {
    buferr = stream_write->ux_device_class_video_stream_buffer_error_count;
  }
  if (period_ms == 0U)
  {
    period_ms = 1U;
  }

  printf("UVC: %s, sent %lu.%lu fps, empty %lu, isoinc %lu, buferr %lu, getfail %lu\r\n",
         (uvc_state == UVC_PLAY_STATUS_STREAMING) ? "streaming" : "stopped",
         (unsigned long)(d * 1000U / period_ms), (unsigned long)((d * 10000U / period_ms) % 10U),
         (unsigned long)uvc_empty_payloads, (unsigned long)ux_dcd_stm32_iso_in_incomplete_count,
         (unsigned long)buferr, (unsigned long)uvc_payload_get_fail);

  /* 지연(5초 평균/최대, ms 소수 첫째 자리). 출력 후 누적값을 비운다. */
  {
    static const char *const name[UVC_LAT_NUM] = { "cap", "enc", "wait", "usb", "total" };
    uint32_t n;
    uint32_t sum[UVC_LAT_NUM];
    uint32_t max[UVC_LAT_NUM];
    uint32_t i;
    UX_INTERRUPT_SAVE_AREA

    UX_DISABLE
    n = uvc_lat.n;
    for (i = 0U; i < UVC_LAT_NUM; i++)
    {
      sum[i] = uvc_lat.sum_us[i];
      max[i] = uvc_lat.max_us[i];
      uvc_lat.sum_us[i] = 0U;
      uvc_lat.max_us[i] = 0U;
    }
    uvc_lat.n = 0U;
    UX_RESTORE

    if (n > 0U)
    {
      printf("LAT ms (avg/max, n %lu):", (unsigned long)n);
      for (i = 0U; i < UVC_LAT_NUM; i++)
      {
        uint32_t avg = sum[i] / n;
        printf(" %s %lu.%lu/%lu.%lu", name[i],
               (unsigned long)(avg / 1000U), (unsigned long)((avg / 100U) % 10U),
               (unsigned long)(max[i] / 1000U), (unsigned long)((max[i] / 100U) % 10U));
      }
      printf("\r\n");
    }
  }
}

/* USER CODE END 1 */
