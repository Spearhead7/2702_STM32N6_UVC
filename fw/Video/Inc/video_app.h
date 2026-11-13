/**
  ******************************************************************************
  * @file    video_app.h
  * @brief   카메라(IMX335) → DCMIPP/ISP → VENC → 프레임 큐 영상 경로.
  *
  *          USB와 무관하게 부팅 후 계속 돈다. 결과는 vframe_queue로만 내보낸다.
  ******************************************************************************
  */
#ifndef VIDEO_APP_H
#define VIDEO_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "tx_api.h"

/* App_ThreadX_Init()에서 호출. 영상 스레드를 만든다. */
UINT video_app_init(void);

typedef struct
{
  uint32_t cap_frames;     /* DCMIPP 프레임 완료 수 */
  uint32_t enc_frames;     /* 인코딩 성공 수 */
  uint32_t enc_errors;     /* 인코딩 실패 수 */
  uint32_t cap_skipped;    /* 인코더가 못 따라가 건너뛴 캡처 프레임 수 */
  uint32_t no_slot;        /* 큐 슬롯이 없어 버린 프레임 수 */
  uint32_t pipe_errors;    /* DCMIPP 파이프 오류(오버런 등) */
  uint32_t last_size;      /* 마지막 프레임 크기(바이트) */
  uint32_t max_size;       /* 최대 프레임 크기(바이트) */
  uint32_t idr_frames;     /* H.264 IDR 프레임 수 */
  uint32_t last_idr_size;  /* 마지막 IDR 크기(바이트) */
  uint32_t sum_size;       /* 통계 주기 동안 프레임 크기 합(평균 계산용, 출력 후 0) */
  uint32_t format;         /* 지금 인코딩 중인 형식 VFQ_FMT_* */
  uint32_t last_enc_ms;    /* 마지막 인코딩 시간 */
  uint32_t max_enc_ms;     /* 최대 인코딩 시간 */
  int32_t  init_status;    /* 0: 정상 동작 중, 음수: 초기화 실패 단계 */
} video_stats_t;

const video_stats_t *video_app_stats(void);

/* 인코딩 형식 바꾸기 요청(VFQ_FMT_*). 영상 스레드가 다음 프레임 전에 인코더를 다시 만든다.
   UVC가 스트림을 열 때 호스트가 고른 형식으로 부른다. */
void video_set_format(uint32_t format);

/* 다음 프레임을 키프레임(H.264 IDR)으로. MJPEG는 모든 프레임이 키프레임이라 무시된다. */
void video_request_keyframe(void);

#ifdef __cplusplus
}
#endif

#endif /* VIDEO_APP_H */
