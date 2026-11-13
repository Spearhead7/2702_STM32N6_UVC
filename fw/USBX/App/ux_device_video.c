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
            video_Probe_Control.dwMaxPayloadTransferSize = USBD_VIDEO_EPIN_HS_MPS;
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
            video_Commit_Control.dwMaxPayloadTransferSize = USBD_VIDEO_EPIN_HS_MPS;
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

  max_playload = USBD_VIDEO_EPIN_HS_MPS;

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
    return;
  }

  mps = stream->ux_device_class_video_stream_endpoint->ux_slave_endpoint_descriptor.wMaxPacketSize & 0x7FFU;
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
    }
  }

  info = (UCHAR)(UVC_HDR_EOH | uvc_fid);

  if (uvc_frame != UX_NULL)
  {
    chunk = uvc_frame->size - uvc_frame_off;
    if (chunk > (mps - UVC_HDR_LEN))
    {
      chunk = mps - UVC_HDR_LEN;
    }

    ux_utility_memory_copy(buffer + UVC_HDR_LEN, uvc_frame->data + uvc_frame_off, chunk);
    uvc_frame_off += chunk;

    if (uvc_frame_off >= uvc_frame->size)
    {
      info |= UVC_HDR_EOF;
      uvc_last_seq = uvc_frame->seq;
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
}

/* USER CODE END 1 */
