/**
  ******************************************************************************
  * @file    video_app.c
  * @brief   카메라(IMX335) → DCMIPP PIPE1(ISP, 16:9 크롭, 축소, YUV422) → VENC JPEG
  *          → 프레임 큐.
  *
  *          카메라 설정은 STM32CubeN6 V1.4.1 Applications/VENC/VENC_USB
  *          (dcmipp_app.c, venc_app.c)을 따랐다. 다른 점:
  *          - 센서 4:3 화면을 그대로 줄이면 16:9 출력이 찌그러지므로 먼저 크롭한다.
  *          - 출력은 USB에 직접 넘기지 않고 vframe_queue에 넣는다.
  *          - 캡처 버퍼 3개를 돌려 써서, 인코딩 중인 버퍼를 DCMIPP가 덮어쓰지 않게 한다.
  *          DCMIPP의 PIPE0, PIPE2(2단계 AI용·화면용)는 건드리지 않는다.
  ******************************************************************************
  */
#include "video_app.h"
#include "video_cfg.h"
#include "vframe_queue.h"
#include "jpeg_enc.h"
#include "stm32n6570_discovery_camera.h"
#include "stm32n6xx_ll_venc.h"
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * 설정
 * ------------------------------------------------------------------------- */
static const video_cfg_t video_cfg =
{
  .width  = VIDEO_DEFAULT_WIDTH,
  .height = VIDEO_DEFAULT_HEIGHT,
  .fps    = VIDEO_DEFAULT_FPS,
  .jpeg_q = VIDEO_DEFAULT_JPEG_Q,
};

const video_cfg_t *video_cfg_get(void)
{
  return &video_cfg;
}

/* ---------------------------------------------------------------------------
 * 스레드, 버퍼
 * ------------------------------------------------------------------------- */
#define VIDEO_THREAD_STACK_SIZE   (8U * 1024U)   /* Debug(-O0)와 ISP 알고리즘 여유 */
#define VIDEO_THREAD_PRIO         15U            /* USBX(10)보다 낮고 CDC(20)보다 높다 */
#define VIDEO_STATS_PERIOD_MS     5000U
#define VIDEO_FRAME_TIMEOUT_MS    500U

#define FLAG_FRAME                0x01U

#define CAP_BUF_NUM               3U
#define CAP_BUF_SIZE              (VIDEO_MAX_WIDTH * VIDEO_MAX_HEIGHT * 2U)  /* YUV422 */
#define CAP_NONE                  0xFFU

static uint8_t cap_buf[CAP_BUF_NUM][CAP_BUF_SIZE] IN_PSRAM;

static TX_THREAD            video_thread;
static ULONG                video_thread_stack[VIDEO_THREAD_STACK_SIZE / sizeof(ULONG)];
static TX_EVENT_FLAGS_GROUP video_flags;

/* ISR ↔ 스레드 공유 상태 */
static volatile uint8_t  cap_write_idx  = 0U;        /* DCMIPP가 쓰고 있는 버퍼 */
static volatile uint8_t  cap_ready_idx  = CAP_NONE;  /* 다 찍혀서 인코딩을 기다리는 버퍼 */
static volatile uint8_t  cap_enc_idx    = CAP_NONE;  /* 인코딩 중인 버퍼 */
static volatile uint32_t cap_ready_tick = 0U;

static video_stats_t stats;

static void video_thread_entry(ULONG arg);

/* ---------------------------------------------------------------------------
 * 공개 함수
 * ------------------------------------------------------------------------- */
UINT video_app_init(void)
{
  UINT ret;

  memset(&stats, 0, sizeof(stats));
  stats.init_status = 1;   /* 아직 시작 전 */

  ret = tx_event_flags_create(&video_flags, "video flags");
  if (ret != TX_SUCCESS)
  {
    return ret;
  }

  return tx_thread_create(&video_thread, "video", video_thread_entry, 0U,
                          video_thread_stack, sizeof(video_thread_stack),
                          VIDEO_THREAD_PRIO, VIDEO_THREAD_PRIO,
                          TX_NO_TIME_SLICE, TX_AUTO_START);
}

const video_stats_t *video_app_stats(void)
{
  return &stats;
}

/* ---------------------------------------------------------------------------
 * DCMIPP 설정 (BSP의 약한 정의를 덮어쓴다. BSP_CAMERA_Init() 안에서 불린다)
 * ------------------------------------------------------------------------- */
static HAL_StatusTypeDef dcmipp_crop_downsize(DCMIPP_HandleTypeDef *hdcmipp,
                                              uint32_t out_w, uint32_t out_h)
{
  DCMIPP_CropConfTypeDef crop = {0};
  DCMIPP_DownsizeTypeDef ds   = {0};
  uint32_t crop_w;
  uint32_t crop_h;

  /* 출력 화면비에 맞춰 센서 가운데를 잘라낸다(1280x720 → 2592x1458). */
  if ((out_w * VIDEO_SENSOR_HEIGHT) >= (out_h * VIDEO_SENSOR_WIDTH))
  {
    crop_w = VIDEO_SENSOR_WIDTH;
    crop_h = (VIDEO_SENSOR_WIDTH * out_h / out_w) & ~1U;
  }
  else
  {
    crop_h = VIDEO_SENSOR_HEIGHT;
    crop_w = (VIDEO_SENSOR_HEIGHT * out_w / out_h) & ~1U;
  }

  crop.HStart   = ((VIDEO_SENSOR_WIDTH - crop_w) / 2U) & ~1U;
  crop.VStart   = ((VIDEO_SENSOR_HEIGHT - crop_h) / 2U) & ~1U;
  crop.HSize    = crop_w;
  crop.VSize    = crop_h;
  crop.PipeArea = DCMIPP_POSITIVE_AREA;

  if (HAL_DCMIPP_PIPE_SetCropConfig(hdcmipp, DCMIPP_PIPE1, &crop) != HAL_OK)
  {
    return HAL_ERROR;
  }
  if (HAL_DCMIPP_PIPE_EnableCrop(hdcmipp, DCMIPP_PIPE1) != HAL_OK)
  {
    return HAL_ERROR;
  }

  /* 축소 비율 계산은 RM0486 표 354(VENC_USB dcmipp_downsize()와 같음) */
  ds.HRatio     = (crop_w * 8192U) / out_w;
  ds.VRatio     = (crop_h * 8192U) / out_h;
  ds.HSize      = out_w;
  ds.VSize      = out_h;
  ds.HDivFactor = (1024U * 8192U - 1U) / ds.HRatio;
  ds.VDivFactor = (1024U * 8192U - 1U) / ds.VRatio;

  if (HAL_DCMIPP_PIPE_SetDownsizeConfig(hdcmipp, DCMIPP_PIPE1, &ds) != HAL_OK)
  {
    return HAL_ERROR;
  }
  return HAL_DCMIPP_PIPE_EnableDownsize(hdcmipp, DCMIPP_PIPE1);
}

HAL_StatusTypeDef MX_DCMIPP_Init(DCMIPP_HandleTypeDef *hdcmipp)
{
  DCMIPP_PipeConfTypeDef      pipe_conf     = {0};
  DCMIPP_CSI_PIPE_ConfTypeDef csi_pipe_conf = {0};
  DCMIPP_CSI_ConfTypeDef      csi_conf      = {0};
  const video_cfg_t *cfg = video_cfg_get();

  if (HAL_DCMIPP_Init(hdcmipp) != HAL_OK)
  {
    return HAL_ERROR;
  }

  /* CSI-2: 2 lane, 1600 Mbps, 가상 채널 0, RAW10 */
  csi_conf.DataLaneMapping = DCMIPP_CSI_PHYSICAL_DATA_LANES;
  csi_conf.NumberOfLanes   = DCMIPP_CSI_TWO_DATA_LANES;
  csi_conf.PHYBitrate      = DCMIPP_CSI_PHY_BT_1600;
  if (HAL_DCMIPP_CSI_SetConfig(hdcmipp, &csi_conf) != HAL_OK)
  {
    return HAL_ERROR;
  }
  if (HAL_DCMIPP_CSI_SetVCConfig(hdcmipp, DCMIPP_VIRTUAL_CHANNEL0, DCMIPP_CSI_DT_BPP10) != HAL_OK)
  {
    return HAL_ERROR;
  }

  csi_pipe_conf.DataTypeMode = DCMIPP_DTMODE_DTIDA;
  csi_pipe_conf.DataTypeIDA  = DCMIPP_DT_RAW10;
  csi_pipe_conf.DataTypeIDB  = DCMIPP_DT_RAW10;   /* 사용 안 함 */
  if (HAL_DCMIPP_CSI_PIPE_SetConfig(hdcmipp, DCMIPP_PIPE1, &csi_pipe_conf) != HAL_OK)
  {
    return HAL_ERROR;
  }

  /* PIPE1 출력: YUV422 인터리브(YUYV), 한 줄 = 폭*2 바이트 */
  pipe_conf.FrameRate         = DCMIPP_FRAME_RATE_ALL;   /* fps는 센서에서 정한다 */
  pipe_conf.PixelPackerFormat = DCMIPP_PIXEL_PACKER_FORMAT_YUV422_1;
  pipe_conf.PixelPipePitch    = cfg->width * 2U;
  if (HAL_DCMIPP_PIPE_SetConfig(hdcmipp, DCMIPP_PIPE1, &pipe_conf) != HAL_OK)
  {
    return HAL_ERROR;
  }
  if (HAL_DCMIPP_PIPE_EnableRedBlueSwap(hdcmipp, DCMIPP_PIPE1) != HAL_OK)
  {
    return HAL_ERROR;
  }

  /* RGB → YUV(BT.601, 제한 범위). 값은 VENC_USB 그대로. */
#define N10(val) ((((uint32_t)(val)) ^ 0x7FFU) + 1U)
  DCMIPP_ColorConversionConfTypeDef color_conf =
  {
    .ClampOutputSamples = ENABLE,
    .OutputSamplesType  = DCMIPP_CLAMP_YUV,
    .RR = N10(26),  .RG = N10(87),  .RB = 112,     .RA = 128,
    .GR = 47,       .GG = 157,      .GB = 16,      .GA = 16,
    .BR = 112,      .BG = N10(102), .BB = N10(10), .BA = 128,
  };
#undef N10
  if (HAL_DCMIPP_PIPE_SetYUVConversionConfig(hdcmipp, DCMIPP_PIPE1, &color_conf) != HAL_OK)
  {
    return HAL_ERROR;
  }
  if (HAL_DCMIPP_PIPE_EnableYUVConversion(hdcmipp, DCMIPP_PIPE1) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if (dcmipp_crop_downsize(hdcmipp, cfg->width, cfg->height) != HAL_OK)
  {
    return HAL_ERROR;
  }

  /* 프레임 단위로만 쓰므로 라인 이벤트는 끈다(VENC_USB 프레임 모드와 같음). */
  MODIFY_REG(DCMIPP->P1PPCR, DCMIPP_P1PPCR_LINEMULT_Msk, DCMIPP_MULTILINE_128_LINES);
  return HAL_DCMIPP_PIPE_DisableLineEvent(hdcmipp, DCMIPP_PIPE1);
}

/* ---------------------------------------------------------------------------
 * DCMIPP 콜백(인터럽트 문맥)
 * ------------------------------------------------------------------------- */
void BSP_CAMERA_FrameEventCallback(uint32_t instance)
{
  uint8_t done;
  uint8_t next;

  if (instance != DCMIPP_PIPE1)
  {
    return;
  }

  done = cap_write_idx;
  next = (uint8_t)((done + 1U) % CAP_BUF_NUM);
  if (next == cap_enc_idx)
  {
    next = (uint8_t)((next + 1U) % CAP_BUF_NUM);
  }

  /* 다음 프레임을 받을 주소(섀도 레지스터, 다음 프레임 시작 때 반영) */
  (void)HAL_DCMIPP_PIPE_SetMemoryAddress(&hcamera_dcmipp, DCMIPP_PIPE1, DCMIPP_MEMORY_ADDRESS_0,
                                         (uint32_t)cap_buf[next]);
  cap_write_idx = next;

  if (cap_ready_idx != CAP_NONE)
  {
    stats.cap_skipped++;   /* 인코더가 이전 프레임을 아직 안 가져감 */
  }
  cap_ready_idx  = done;
  cap_ready_tick = HAL_GetTick();
  stats.cap_frames++;

  (void)tx_event_flags_set(&video_flags, FLAG_FRAME, TX_OR);
}

void BSP_CAMERA_PipeErrorCallback(uint32_t instance)
{
  (void)instance;
  stats.pipe_errors++;
}

/* ---------------------------------------------------------------------------
 * 영상 스레드
 * ------------------------------------------------------------------------- */
static void video_print_stats(uint32_t period_ms)
{
  static uint32_t prev_cap = 0U;
  static uint32_t prev_enc = 0U;
  uint32_t cap = stats.cap_frames;
  uint32_t enc = stats.enc_frames;
  uint32_t d_cap = cap - prev_cap;
  uint32_t d_enc = enc - prev_enc;

  printf("VIDEO: cap %lu.%lu fps, enc %lu.%lu fps, jpeg last %lu KB max %lu KB, "
         "enc %lu ms (max %lu), skip %lu, noslot %lu, recycle %lu, err %lu, pipeerr %lu\r\n",
         (unsigned long)(d_cap * 1000U / period_ms), (unsigned long)((d_cap * 10000U / period_ms) % 10U),
         (unsigned long)(d_enc * 1000U / period_ms), (unsigned long)((d_enc * 10000U / period_ms) % 10U),
         (unsigned long)(stats.last_size / 1024U), (unsigned long)(stats.max_size / 1024U),
         (unsigned long)stats.last_enc_ms, (unsigned long)stats.max_enc_ms,
         (unsigned long)stats.cap_skipped, (unsigned long)stats.no_slot,
         (unsigned long)vfq_dropped(), (unsigned long)stats.enc_errors,
         (unsigned long)stats.pipe_errors);

  prev_cap = cap;
  prev_enc = enc;
}

static void video_fail(int32_t step, const char *what)
{
  stats.init_status = step;
  printf("VIDEO: init failed at step %ld (%s)\r\n", (long)step, what);
  for (;;)
  {
    (void)tx_thread_sleep(TX_WAIT_FOREVER);
  }
}

static void video_thread_entry(ULONG arg)
{
  const video_cfg_t *cfg = video_cfg_get();
  uint32_t last_stats_tick;
  int32_t  ret;

  (void)arg;

  printf("VIDEO: %lux%lu %lu fps, JPEG q%lu\r\n", (unsigned long)cfg->width,
         (unsigned long)cfg->height, (unsigned long)cfg->fps, (unsigned long)cfg->jpeg_q);

  vfq_init();

  ret = BSP_CAMERA_Init(0, CAMERA_R2592x1944, CAMERA_PF_RAW_RGGB10);
  if (ret != BSP_ERROR_NONE)
  {
    printf("VIDEO: BSP_CAMERA_Init ret %ld\r\n", (long)ret);
    video_fail(-1, "camera init (MB1854B connected?)");
  }

  if (IMX335_SetFramerate(Camera_CompObj, (int32_t)cfg->fps) != IMX335_OK)
  {
    video_fail(-2, "sensor framerate");
  }

  LL_VENC_Init();

  ret = jpeg_enc_init(cfg->width, cfg->height, cfg->jpeg_q);
  if (ret != 0)
  {
    printf("VIDEO: jpeg_enc_init ret %ld\r\n", (long)ret);
    video_fail(-3, "VENC JPEG init");
  }

  cap_write_idx = 0U;
  cap_ready_idx = CAP_NONE;
  cap_enc_idx   = CAP_NONE;
  if (BSP_CAMERA_Start(0, cap_buf[0], CAMERA_MODE_CONTINUOUS) != BSP_ERROR_NONE)
  {
    video_fail(-4, "camera start");
  }

  stats.init_status = 0;
  printf("VIDEO: camera started\r\n");
  last_stats_tick = HAL_GetTick();

  for (;;)
  {
    ULONG flags = 0U;
    uint8_t idx;
    uint32_t tick;
    uint32_t t0;
    vfq_frame_t *f;

    /* ISP 자동 노출·화이트밸런스(프레임마다 한 번) */
    (void)BSP_CAMERA_BackgroundProcess();

    if (tx_event_flags_get(&video_flags, FLAG_FRAME, TX_OR_CLEAR, &flags,
                           VIDEO_FRAME_TIMEOUT_MS * TX_TIMER_TICKS_PER_SECOND / 1000U) == TX_SUCCESS)
    {
      /* 다 찍힌 버퍼를 가져온다 */
      __disable_irq();
      idx = cap_ready_idx;
      cap_ready_idx = CAP_NONE;
      cap_enc_idx = idx;
      tick = cap_ready_tick;
      __enable_irq();

      if (idx != CAP_NONE)
      {
        f = vfq_producer_get();
        if (f == NULL)
        {
          stats.no_slot++;
        }
        else
        {
          t0 = HAL_GetTick();
          ret = jpeg_enc_frame(cap_buf[idx], f->data, f->capacity);
          stats.last_enc_ms = HAL_GetTick() - t0;
          if (stats.last_enc_ms > stats.max_enc_ms)
          {
            stats.max_enc_ms = stats.last_enc_ms;
          }

          if (ret > 0)
          {
            f->size   = (uint32_t)ret;
            f->ts_ms  = tick;
            f->format = VFQ_FMT_MJPEG;
            f->key    = 1U;
            vfq_producer_commit(f);
            stats.enc_frames++;
            stats.last_size = (uint32_t)ret;
            if ((uint32_t)ret > stats.max_size)
            {
              stats.max_size = (uint32_t)ret;
            }
          }
          else
          {
            vfq_producer_abort(f);
            if (stats.enc_errors == 0U)
            {
              printf("VIDEO: jpeg encode error %ld\r\n", (long)ret);
            }
            stats.enc_errors++;
          }
        }

        cap_enc_idx = CAP_NONE;
      }
    }
    else
    {
      static uint32_t timeout_reported = 0U;
      if (timeout_reported < 3U)
      {
        printf("VIDEO: no frame from camera for %lu ms\r\n", (unsigned long)VIDEO_FRAME_TIMEOUT_MS);
        timeout_reported++;
      }
    }

    if ((HAL_GetTick() - last_stats_tick) >= VIDEO_STATS_PERIOD_MS)
    {
      uint32_t now = HAL_GetTick();
      video_print_stats(now - last_stats_tick);
      last_stats_tick = now;
    }
  }
}
