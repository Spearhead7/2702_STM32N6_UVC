/**
  ******************************************************************************
  * @file    video_app.c
  * @brief   카메라(IMX335) → DCMIPP PIPE1(ISP, 16:9 크롭, 축소, NV12)
  *          → (하드웨어 핸드셰이크) → VENC H.264 → 프레임 큐.
  *
  *          카메라 설정은 STM32CubeN6 V1.4.1 Applications/VENC/VENC_USB
  *          (dcmipp_app.c, venc_app.c, venc_h264_config_720p_Slice.h)를 따랐다. 다른 점:
  *          - 센서 4:3 화면을 그대로 줄이면 16:9 출력이 찌그러지므로 먼저 크롭한다.
  *          - 출력은 USB에 직접 넘기지 않고 vframe_queue에 넣는다.
  *          - ISP 자동 노출·화이트밸런스(BSP_CAMERA_BackgroundProcess)는 따로 낮은 우선순위
  *            스레드에서 돌린다. 인코더를 프레임 사이 공백에 제때 거는 것이 우선이다.
  *
  *          하드웨어 핸드셰이크(M4-6):
  *          - DCMIPP는 프레임 전체가 아니라 64줄짜리 줄 버퍼(내부 SRAM)에 돌려 쓰고,
  *            32줄마다 VENC에 하드웨어 신호를 준다. VENC는 그때마다 매크로블록 줄 2개를
  *            인코딩한다. 캡처와 인코딩이 겹치므로 '캡처 완료 → 인코딩 완료'가 거의 0이 된다.
  *          - 인코더는 프레임 사이 공백(프레임 끝 → 다음 프레임 첫 32줄)에 걸어야 한다.
  *            프레임 끝 이벤트를 받으면 바로 h264_enc_frame()을 부르고, 그 안에서 다음
  *            프레임 캡처가 끝날 때까지 기다린다. 걸기 전에 다음 프레임 줄 이벤트가 이미
  *            왔으면(늦음) 그 프레임은 건너뛴다(VENC_USB IsVideoOverflow()와 같음).
  *          - 동기가 어긋나면(H264ENC_FUSE_ERROR) DCMIPP를 멈추고 VENC를 리셋한다.
  *          DCMIPP의 PIPE0, PIPE2(2단계 AI용·화면용)는 건드리지 않는다.
  ******************************************************************************
  */
#include "video_app.h"
#include "video_cfg.h"
#include "vframe_queue.h"
#include "h264_enc.h"
#include "vtime.h"
#include "stm32n6570_discovery_camera.h"
#include "stm32n6xx_ll_venc.h"
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * 설정
 * ------------------------------------------------------------------------- */
static const video_cfg_t video_cfg =
{
  .width   = VIDEO_DEFAULT_WIDTH,
  .height  = VIDEO_DEFAULT_HEIGHT,
  .fps     = VIDEO_DEFAULT_FPS,
  .bitrate = VIDEO_DEFAULT_BITRATE,
  .gop     = VIDEO_DEFAULT_GOP,
};

void video_request_keyframe(void)
{
  h264_enc_request_idr();
}

const video_cfg_t *video_cfg_get(void)
{
  return &video_cfg;
}

/* ---------------------------------------------------------------------------
 * 스레드, 버퍼
 * ------------------------------------------------------------------------- */
#define VIDEO_THREAD_STACK_SIZE   (6U * 1024U)
/* 영상 스레드: 프레임 끝 이벤트를 받자마자 인코더를 걸어야 하므로 UVC 채우기 스레드(21) 바로 아래.
   인코딩 중에는 VENC 인터럽트를 기다리며 잠들어 있어 다른 스레드를 막지 않는다. */
#define VIDEO_THREAD_PRIO         22U
/* ISP(AE/AWB) 스레드: 영상 스레드보다 낮게. 영상 스레드가 인코딩을 기다리는 동안 돈다. */
#define ISP_THREAD_STACK_SIZE     (8U * 1024U)   /* Debug(-O0)와 ISP 알고리즘 여유 */
#define ISP_THREAD_PRIO           23U
#define VIDEO_STATS_PERIOD_MS     5000U
/* 통계 출력 스레드. printf(USART1 115200, 1글자씩 블로킹)는 5초마다 약 30 ms 걸리므로
   가장 낮은 우선순위로 뺀다. */
#define VSTATS_THREAD_STACK_SIZE  (3U * 1024U)
#define VSTATS_THREAD_PRIO        30U
#define VIDEO_FRAME_TIMEOUT_MS    500U

#define FLAG_FRAME                0x01U   /* 프레임 끝 → 영상 스레드(인코더 걸기) */
#define FLAG_ISP                  0x02U   /* 프레임 끝 → ISP 스레드 */

/* 핸드셰이크 줄 버퍼(NV12): Y 64줄 + UV 32줄. 최대 폭 기준(1920 → 180 KB). */
#define LINEBUF_Y_SIZE            (VIDEO_MAX_WIDTH * VIDEO_LINEBUF_LINES)
#define LINEBUF_SIZE              (LINEBUF_Y_SIZE + (LINEBUF_Y_SIZE / 2U))

static uint8_t linebuf[LINEBUF_SIZE] IN_LINEBUF;

static TX_THREAD            video_thread;
static ULONG                video_thread_stack[VIDEO_THREAD_STACK_SIZE / sizeof(ULONG)];
static TX_THREAD            isp_thread;
static ULONG                isp_thread_stack[ISP_THREAD_STACK_SIZE / sizeof(ULONG)];
static TX_THREAD            vstats_thread;
static ULONG                vstats_thread_stack[VSTATS_THREAD_STACK_SIZE / sizeof(ULONG)];
static TX_EVENT_FLAGS_GROUP video_flags;

/* ISR ↔ 스레드 공유 상태 */
static volatile uint32_t line_events     = 0U;   /* 마지막 프레임 끝 이후 DCMIPP 라인 이벤트 수 */
static volatile uint32_t cap_ready_tick  = 0U;
/* 지연 측정(vtime 사이클): 지금 찍히는 프레임의 시작, 마지막으로 끝난 프레임의 시작·끝 */
static volatile uint32_t cap_cur_start   = 0U;
static volatile uint32_t cap_ready_start = 0U;
static volatile uint32_t cap_ready_end   = 0U;

static video_stats_t stats;

static void video_thread_entry(ULONG arg);
static void isp_thread_entry(ULONG arg);
static void vstats_thread_entry(ULONG arg);

/* 줄 버퍼 주소: Y 평면 다음에 UV 평면(VENC_USB dcmipp_get_address()와 같은 배치) */
static uint8_t *linebuf_y(void)
{
  return linebuf;
}

static uint8_t *linebuf_uv(void)
{
  return &linebuf[video_cfg_get()->width * VIDEO_LINEBUF_LINES];
}

/* ---------------------------------------------------------------------------
 * 공개 함수
 * ------------------------------------------------------------------------- */
UINT video_app_init(void)
{
  UINT ret;

  memset(&stats, 0, sizeof(stats));
  vtime_init();
  stats.init_status = 1;   /* 아직 시작 전 */

  ret = tx_event_flags_create(&video_flags, "video flags");
  if (ret != TX_SUCCESS)
  {
    return ret;
  }

  ret = tx_thread_create(&video_thread, "video", video_thread_entry, 0U,
                         video_thread_stack, sizeof(video_thread_stack),
                         VIDEO_THREAD_PRIO, VIDEO_THREAD_PRIO,
                         TX_NO_TIME_SLICE, TX_AUTO_START);
  if (ret != TX_SUCCESS)
  {
    return ret;
  }

  ret = tx_thread_create(&isp_thread, "isp", isp_thread_entry, 0U,
                         isp_thread_stack, sizeof(isp_thread_stack),
                         ISP_THREAD_PRIO, ISP_THREAD_PRIO,
                         TX_NO_TIME_SLICE, TX_AUTO_START);
  if (ret != TX_SUCCESS)
  {
    return ret;
  }

  return tx_thread_create(&vstats_thread, "vstats", vstats_thread_entry, 0U,
                          vstats_thread_stack, sizeof(vstats_thread_stack),
                          VSTATS_THREAD_PRIO, VSTATS_THREAD_PRIO,
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

  /* PIPE1 출력: NV12(YUV420 semi-planar). Y 한 줄 = 폭 바이트(VENC_USB 720p_Slice와 같음) */
  pipe_conf.FrameRate         = DCMIPP_FRAME_RATE_ALL;   /* fps는 센서에서 정한다 */
  pipe_conf.PixelPackerFormat = DCMIPP_PIXEL_PACKER_FORMAT_YUV420_2;
  pipe_conf.PixelPipePitch    = cfg->width;
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

  /* 하드웨어 핸드셰이크(VENC_USB dcmipp_enable_HW_Handshake()와 같음):
     64줄마다 줄 버퍼 처음으로 돌아가고, 32줄마다 라인 이벤트(= VENC 하드웨어 신호). */
  if (HAL_DCMIPP_PIPE_SetLineWrappingConfig(hdcmipp, DCMIPP_PIPE1, DCMIPP_WRAP_ADDRESS_64_LINES) != HAL_OK)
  {
    return HAL_ERROR;
  }
  if (HAL_DCMIPP_PIPE_EnableLineWrapping(hdcmipp, DCMIPP_PIPE1) != HAL_OK)
  {
    return HAL_ERROR;
  }
  MODIFY_REG(DCMIPP->P1PPCR, DCMIPP_P1PPCR_LINEMULT_Msk, DCMIPP_MULTILINE_32_LINES);
  return HAL_OK;
}

/* ---------------------------------------------------------------------------
 * DCMIPP 콜백(인터럽트 문맥)
 * ------------------------------------------------------------------------- */
/* 프레임 시작(VSYNC). 지연 측정용 시각만 기록한다.
   BSP(VENC_USB 판)는 파이프와 관계없이 instance 0으로 부른다. 지금은 PIPE1만 돌리므로 그대로 쓴다. */
void BSP_CAMERA_VsyncEventCallback(uint32_t instance)
{
  (void)instance;
  cap_cur_start = vtime_now();
}

/* 32줄마다(라인 이벤트 인터럽트). 인코더를 늦게 걸었는지 판단하는 데만 쓴다. */
void BSP_CAMERA_LineEventCallback(uint32_t instance)
{
  (void)instance;
  line_events++;
}

void BSP_CAMERA_FrameEventCallback(uint32_t instance)
{
  if (instance != DCMIPP_PIPE1)
  {
    return;
  }

  line_events     = 0U;
  cap_ready_tick  = HAL_GetTick();
  cap_ready_end   = vtime_now();
  cap_ready_start = cap_cur_start;
  stats.cap_frames++;

  (void)tx_event_flags_set(&video_flags, FLAG_FRAME | FLAG_ISP, TX_OR);
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

  uint32_t sum = stats.sum_size;
  stats.sum_size = 0U;

  printf("VIDEO: H264 cap %lu.%lu fps, enc %lu.%lu fps, avg %lu KB last %lu KB max %lu KB idr %lu KB, "
         "call %lu ms (max %lu), late %lu, desync %lu, noslot %lu, recycle %lu, err %lu, pipeerr %lu\r\n",
         (unsigned long)(d_cap * 1000U / period_ms), (unsigned long)((d_cap * 10000U / period_ms) % 10U),
         (unsigned long)(d_enc * 1000U / period_ms), (unsigned long)((d_enc * 10000U / period_ms) % 10U),
         (unsigned long)((d_enc != 0U) ? (sum / d_enc / 1024U) : 0U),
         (unsigned long)(stats.last_size / 1024U), (unsigned long)(stats.max_size / 1024U),
         (unsigned long)(stats.last_idr_size / 1024U),
         (unsigned long)stats.last_enc_ms, (unsigned long)stats.max_enc_ms,
         (unsigned long)stats.cap_skipped, (unsigned long)stats.desync, (unsigned long)stats.no_slot,
         (unsigned long)vfq_dropped(), (unsigned long)stats.enc_errors,
         (unsigned long)stats.pipe_errors);

  prev_cap = cap;
  prev_enc = enc;
}

/* 통계 출력 뒤에 불린다. 영상 소비자(UVC 등)가 자기 통계를 덧붙일 때 재정의한다.
   video_app은 소비자를 모르므로 약한 정의로 둔다. */
__attribute__((weak)) void video_stats_hook(uint32_t period_ms)
{
  (void)period_ms;
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

static int32_t video_encoder_init(void)
{
  const video_cfg_t *cfg = video_cfg_get();
  h264_enc_cfg_t hc;

  hc.width       = cfg->width;
  hc.height      = cfg->height;
  hc.fps         = cfg->fps;
  hc.bitrate_bps = cfg->bitrate;
  hc.gop         = cfg->gop;
  hc.line_buf_mb = VIDEO_LINEBUF_MB;
  return h264_enc_init(&hc);
}

/* DCMIPP–VENC 동기가 어긋났을 때(VENC_USB encoder_reset()와 같음).
   DCMIPP를 멈추고 프레임이 끝나길 기다린 뒤 VENC를 하드웨어 리셋하고 인코더를 새로 만든다. */
static void video_encoder_reset(void)
{
  stats.desync++;
  (void)HAL_DCMIPP_PIPE_Suspend(&hcamera_dcmipp, DCMIPP_PIPE1);
  tx_thread_sleep((15U * TX_TIMER_TICKS_PER_SECOND / 1000U) + 1U);

  h264_enc_deinit();
  __HAL_RCC_VENC_FORCE_RESET();
  tx_thread_sleep((1U * TX_TIMER_TICKS_PER_SECOND / 1000U) + 1U);
  __HAL_RCC_VENC_RELEASE_RESET();
  tx_thread_sleep((1U * TX_TIMER_TICKS_PER_SECOND / 1000U) + 1U);
  if (video_encoder_init() != 0)
  {
    video_fail(-5, "VENC re-init after desync");
  }

  (void)tx_event_flags_set(&video_flags, ~((ULONG)FLAG_FRAME), TX_AND);   /* 남은 FLAG_FRAME 지우기(다음 프레임 끝부터) */
  (void)HAL_DCMIPP_PIPE_Resume(&hcamera_dcmipp, DCMIPP_PIPE1);
}

/* 프레임 끝 이벤트를 받은 직후에 부른다. 다음 프레임을 인코딩해 큐에 넣는다. */
static void video_encode_next_frame(void)
{
  vfq_frame_t *f;
  uint32_t armed_frames;
  uint32_t t0;
  uint32_t t_start;
  uint32_t t_end;
  uint32_t tick;
  uint8_t  key = 0U;
  int32_t  ret;

  /* 다음 프레임 줄이 이미 들어오기 시작했으면 VENC가 첫 줄 신호를 놓친다 → 이 프레임은 건너뛴다 */
  if (line_events != 0U)
  {
    stats.cap_skipped++;
    return;
  }

  f = vfq_producer_get();
  if (f == NULL)
  {
    stats.no_slot++;
    return;
  }

  armed_frames = stats.cap_frames;
  t0 = HAL_GetTick();
  ret = h264_enc_frame(linebuf_y(), linebuf_uv(), f->data, f->capacity, &key);
  stats.last_enc_ms = HAL_GetTick() - t0;
  if (stats.last_enc_ms > stats.max_enc_ms)
  {
    stats.max_enc_ms = stats.last_enc_ms;
  }

  /* 방금 인코딩한 프레임의 캡처 시각. 보통은 그 프레임의 끝 이벤트가 이미 왔다.
     VENC가 끝 이벤트보다 먼저 끝났으면 지금 찍히는 프레임 시작과 지금 시각을 쓴다. */
  __disable_irq();
  if (stats.cap_frames != armed_frames)
  {
    t_start = cap_ready_start;
    t_end   = cap_ready_end;
    tick    = cap_ready_tick;
  }
  else
  {
    t_start = cap_cur_start;
    t_end   = vtime_now();
    tick    = HAL_GetTick();
  }
  __enable_irq();

  if (ret > 0)
  {
    f->size   = (uint32_t)ret;
    f->ts_ms  = tick;
    f->format = VFQ_FMT_H264;
    f->key    = key;
    f->t_cap_start = t_start;
    f->t_cap_end   = t_end;
    f->t_enc_end   = vtime_now();
    vfq_producer_commit(f);
    stats.enc_frames++;
    stats.last_size = (uint32_t)ret;
    stats.sum_size += (uint32_t)ret;
    if (key != 0U)
    {
      stats.idr_frames++;
      stats.last_idr_size = (uint32_t)ret;
    }
    if ((uint32_t)ret > stats.max_size)
    {
      stats.max_size = (uint32_t)ret;
    }
    return;
  }

  vfq_producer_abort(f);
  if (stats.enc_errors < 3U)
  {
    printf("VIDEO: h264 encode error %ld\r\n", (long)ret);
  }
  stats.enc_errors++;
  if (h264_enc_is_desync(ret) != 0U)
  {
    video_encoder_reset();
  }
}

static void video_thread_entry(ULONG arg)
{
  const video_cfg_t *cfg = video_cfg_get();
  DCMIPP_SemiPlanarDstAddressTypeDef dst;
  int32_t ret;

  (void)arg;

  printf("VIDEO: %lux%lu %lu fps, H264 %lu kbps gop %lu, HW handshake NV12 linebuf %lu lines\r\n",
         (unsigned long)cfg->width, (unsigned long)cfg->height, (unsigned long)cfg->fps,
         (unsigned long)(cfg->bitrate / 1000U), (unsigned long)cfg->gop,
         (unsigned long)VIDEO_LINEBUF_LINES);

  vfq_init();

  /* 카메라 초기화. USB-C에 휴대폰을 꽂은 채 부팅하면 PD 하드 리셋 등과 겹쳐 센서 I2C 확인
     (IMX335_Probe)이 가끔 실패한다(ret -7). BSP_CAMERA_Init은 매번 카메라 전원을 껐다 켜므로
     DCMIPP만 해제하고 몇 번 다시 시도한다. */
  for (uint32_t attempt = 1U; attempt <= 3U; attempt++)
  {
    ret = BSP_CAMERA_Init(0, CAMERA_R2592x1944, CAMERA_PF_RAW_RGGB10);
    if (ret == BSP_ERROR_NONE)
    {
      break;
    }
    printf("VIDEO: BSP_CAMERA_Init ret %ld (try %lu)\r\n", (long)ret, (unsigned long)attempt);
    (void)HAL_DCMIPP_DeInit(&hcamera_dcmipp);
    tx_thread_sleep((300U * TX_TIMER_TICKS_PER_SECOND / 1000U) + 1U);
  }
  if (ret != BSP_ERROR_NONE)
  {
    video_fail(-1, "camera init (MB1854B connected?)");
  }

  if (IMX335_SetFramerate(Camera_CompObj, (int32_t)cfg->fps) != IMX335_OK)
  {
    video_fail(-2, "sensor framerate");
  }

  LL_VENC_Init();
  ret = video_encoder_init();
  if (ret != 0)
  {
    printf("VIDEO: h264 encoder init ret %ld\r\n", (long)ret);
    video_fail(-3, "VENC init");
  }

  /* 줄 버퍼로 캡처 시작. 첫 프레임은 인코더를 걸기 전이라 버려진다. */
  dst.YAddress  = (uint32_t)linebuf_y();
  dst.UVAddress = (uint32_t)linebuf_uv();
  if (BSP_CAMERA_SemiPlanarStart(0, &dst, CAMERA_MODE_CONTINUOUS) != BSP_ERROR_NONE)
  {
    video_fail(-4, "camera start");
  }
  __HAL_DCMIPP_ENABLE_IT(&hcamera_dcmipp, DCMIPP_IT_PIPE1_LINE);   /* 늦음 판단용 라인 이벤트 */

  stats.init_status = 0;
  printf("VIDEO: camera started\r\n");

  for (;;)
  {
    ULONG flags = 0U;

    if (tx_event_flags_get(&video_flags, FLAG_FRAME, TX_OR_CLEAR, &flags,
                           VIDEO_FRAME_TIMEOUT_MS * TX_TIMER_TICKS_PER_SECOND / 1000U) == TX_SUCCESS)
    {
      video_encode_next_frame();
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
  }
}

/* ISP 자동 노출·화이트밸런스(프레임마다 한 번). 영상 스레드가 인코딩을 기다리는 동안 돈다. */
static void isp_thread_entry(ULONG arg)
{
  (void)arg;

  for (;;)
  {
    ULONG flags = 0U;

    (void)tx_event_flags_get(&video_flags, FLAG_ISP, TX_OR_CLEAR, &flags, TX_WAIT_FOREVER);
    if (stats.init_status == 0)
    {
      (void)BSP_CAMERA_BackgroundProcess();
    }
  }
}

/* 5초마다 VIDEO:/UVC:/LAT 통계 출력(가장 낮은 우선순위) */
static void vstats_thread_entry(ULONG arg)
{
  uint32_t last = HAL_GetTick();
  (void)arg;

  for (;;)
  {
    uint32_t now;

    tx_thread_sleep(VIDEO_STATS_PERIOD_MS * TX_TIMER_TICKS_PER_SECOND / 1000U);
    if (stats.init_status != 0)
    {
      last = HAL_GetTick();
      continue;   /* 카메라 시작 전(또는 실패) */
    }
    now = HAL_GetTick();
    video_print_stats(now - last);
    video_stats_hook(now - last);
    last = now;
  }
}
