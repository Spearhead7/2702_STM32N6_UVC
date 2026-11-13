/**
  ******************************************************************************
  * @file    video_cfg.h
  * @brief   영상 경로 설정값(해상도, fps, JPEG 품질)과 메모리 배치 매크로.
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

/* 버퍼 크기 계산용 최대 해상도 */
#define VIDEO_MAX_WIDTH         1920U
#define VIDEO_MAX_HEIGHT        1080U

/* 카메라(IMX335) 원본 크기 */
#define VIDEO_SENSOR_WIDTH      2592U
#define VIDEO_SENSOR_HEIGHT     1944U

/* 기본값: 1-A 단계 1280x720 30fps MJPEG */
#define VIDEO_DEFAULT_WIDTH     1280U
#define VIDEO_DEFAULT_HEIGHT    720U
#define VIDEO_DEFAULT_FPS       30U
/* VENC JPEG 양자화 수준 0(최저 화질)~9(최고 화질).
   USB isochronous 1024 B x 8000/s = 약 8 MB/s 안에 30fps가 들어가야 하므로
   한 장이 약 250 KB를 넘지 않는 값으로 둔다. */
#define VIDEO_DEFAULT_JPEG_Q    6U

typedef struct
{
  uint32_t width;     /* 출력 폭(16의 배수)  */
  uint32_t height;    /* 출력 높이(8의 배수) */
  uint32_t fps;       /* 카메라 프레임레이트 */
  uint32_t jpeg_q;    /* JPEG 양자화 수준 0~9 */
} video_cfg_t;

/* 현재 설정(읽기 전용). 카메라 시작 전에 정해지고, 지금은 실행 중 변경을 지원하지 않는다. */
const video_cfg_t *video_cfg_get(void);

#ifdef __cplusplus
}
#endif

#endif /* VIDEO_CFG_H */
