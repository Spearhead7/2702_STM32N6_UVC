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
  uint32_t cap_skipped;    /* 인코더를 제때 못 걸어(다음 프레임 줄이 이미 들어옴) 건너뛴 프레임 수 */
  uint32_t desync;         /* DCMIPP–VENC 동기 어긋남(VENC 리셋) 횟수 */
  uint32_t no_slot;        /* 큐 슬롯이 없어 버린 프레임 수 */
  uint32_t pipe_errors;    /* DCMIPP 파이프 오류(오버런 등) */
  uint32_t last_size;      /* 마지막 프레임 크기(바이트) */
  uint32_t max_size;       /* 최대 프레임 크기(바이트) */
  uint32_t idr_frames;     /* H.264 IDR 프레임 수 */
  uint32_t last_idr_size;  /* 마지막 IDR 크기(바이트) */
  uint32_t sum_size;       /* 통계 주기 동안 프레임 크기 합(평균 계산용, 출력 후 0) */
  uint32_t last_enc_ms;    /* 마지막 인코딩 호출 시간(핸드셰이크라 캡처 시간 포함, 약 30 ms) */
  uint32_t max_enc_ms;     /* 최대 인코딩 호출 시간 */
  int32_t  init_status;    /* 0: 정상 동작 중, 음수: 초기화 실패 단계 */
} video_stats_t;

const video_stats_t *video_app_stats(void);

/* 다음 프레임을 키프레임(H.264 IDR)으로. UVC가 스트림을 열 때 부른다. */
void video_request_keyframe(void);

/* 통계 주기(5초) 평균. status 명령용. valid가 0이면 아직 한 주기도 지나지 않음 */
typedef struct
{
  uint32_t valid;
  uint32_t cap_fps_x10;    /* 캡처 fps × 10 */
  uint32_t enc_fps_x10;    /* 인코딩 fps × 10 */
  uint32_t bitrate_kbps;   /* 실제 출력 비트레이트 */
} video_live_t;

const video_live_t *video_app_live(void);

/* ---- 실행 중 제어(설정 항목 테이블에서 부른다). 어느 스레드에서나 호출 가능 ---- */
#define VIDEO_OK              0
#define VIDEO_ERR_NOTREADY  (-1)   /* 카메라 시작 전 */
#define VIDEO_ERR_AUTO      (-2)   /* AE가 켜져 있어 수동 노출·게인을 쓸 수 없음 */
#define VIDEO_ERR_HW        (-3)

int32_t  video_cam_get_ae(uint8_t *on);
int32_t  video_cam_set_ae(uint8_t on);
int32_t  video_cam_get_awb(uint8_t *on);
int32_t  video_cam_set_awb(uint8_t on);       /* off: 지금 추정한 색온도로 고정 */
int32_t  video_cam_get_exposure(int32_t *us);
int32_t  video_cam_set_exposure(int32_t us);
int32_t  video_cam_get_gain(int32_t *mdb);
int32_t  video_cam_set_gain(int32_t mdb);

/* 인코더 설정. 영상 스레드가 프레임 사이에 인코더를 다시 만들어 반영한다(VENC는 HRD를 켠 채
   실행 중 율 제어 변경을 받지 않는다). 반영 순간 프레임 1~2장이 빠지고 다음 프레임은 IDR. */
uint32_t video_get_bitrate_kbps(void);
void     video_set_bitrate_kbps(uint32_t kbps);
uint32_t video_get_gop(void);
void     video_set_gop(uint32_t gop);

#ifdef __cplusplus
}
#endif

#endif /* VIDEO_APP_H */
