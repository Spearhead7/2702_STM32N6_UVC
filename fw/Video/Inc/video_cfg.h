/**
  ******************************************************************************
  * @file    video_cfg.h
  * @brief   영상 경로 설정값(해상도, fps, H.264)과 메모리 배치 매크로.
  *
  *          해상도·fps를 코드에 박지 않고 이 설정 구조체 하나로 모은다.
  *          M5의 CLI, 4단계 메뉴는 이 값을 읽고 바꾸는 '설정 항목 테이블'을 따로 둔다.
  *          버퍼 크기는 최대값(VIDEO_MAX_*) 기준으로 잡아, 2단계 1080p 녹화에도 그대로 쓴다.
  ******************************************************************************
  */
#ifndef VIDEO_CFG_H
#define VIDEO_CFG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* 외부 PSRAM에 두는 변수(링커 .psram_bss, NOLOAD, MPU 비캐시) */
#define IN_PSRAM  __attribute__((section(".psram_bss"), aligned(32)))

/* 내부 SRAM(AXISRAM2 아래쪽 512K, 링커 .linebuf_bss, NOLOAD)에 두는 변수.
   DCMIPP–VENC 핸드셰이크 줄 버퍼용. CPU는 읽지 않으므로 캐시 관리가 필요 없다. */
#define IN_LINEBUF  __attribute__((section(".linebuf_bss"), aligned(32)))

/* 버퍼 크기 계산용 최대 해상도 */
#define VIDEO_MAX_WIDTH         1920U
#define VIDEO_MAX_HEIGHT        1080U

/* 카메라(IMX335) 원본 크기 */
#define VIDEO_SENSOR_WIDTH      2592U
#define VIDEO_SENSOR_HEIGHT     1944U

/* 기본값: 1단계 1280x720 30fps H.264 */
#define VIDEO_DEFAULT_WIDTH     1280U
#define VIDEO_DEFAULT_HEIGHT    720U
#define VIDEO_DEFAULT_FPS       30U
/* H.264: CBR 비트레이트(bps), IDR 주기(프레임) */
#define VIDEO_DEFAULT_BITRATE   4000000U
#define VIDEO_DEFAULT_GOP       30U

/* DCMIPP–VENC 하드웨어 핸드셰이크(M4-6). VENC_USB 720p_Slice와 같은 값.
   VENC 줄 버퍼 깊이 2 MB 줄(32줄): DCMIPP는 32줄마다 VENC에 신호를 주고,
   줄 버퍼는 그 두 배(64줄)마다 처음으로 돌아간다(wrap). */
#define VIDEO_LINEBUF_MB        2U
#define VIDEO_LINEBUF_LINES     (VIDEO_LINEBUF_MB * 16U * 2U)   /* wrap 줄 수 = 64 */

typedef struct
{
  uint32_t width;     /* 출력 폭(16의 배수)  */
  uint32_t height;    /* 출력 높이(8의 배수) */
  uint32_t fps;       /* 카메라 프레임레이트 */
  uint32_t bitrate;   /* H.264 목표 비트레이트(bps) */
  uint32_t gop;       /* H.264 IDR 주기(프레임) */
} video_cfg_t;

/* 현재 설정(읽기 전용). 해상도·fps는 카메라 시작 전에 정해진다.
   비트레이트·GOP는 video_set_bitrate_kbps()/video_set_gop()로 실행 중에 바꿀 수 있다(M5). */
const video_cfg_t *video_cfg_get(void);

#ifdef __cplusplus
}
#endif

#endif /* VIDEO_CFG_H */
