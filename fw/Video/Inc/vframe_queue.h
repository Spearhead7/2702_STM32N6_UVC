/**
  ******************************************************************************
  * @file    vframe_queue.h
  * @brief   인코딩된 프레임 큐(생산자 1, 소비자 여럿).
  *
  *          인코더가 프레임을 넣고, UVC(지금)와 SD 녹화(2단계)가 각자 꺼내 쓴다.
  *          소비자는 '가장 최근 프레임'을 참조 카운트로 빌려 쓰고 다 쓰면 돌려준다.
  *          빈 슬롯이 없으면 생산자는 아무도 쓰지 않는 가장 오래된 프레임을 덮어쓴다
  *          (느린 소비자는 프레임을 건너뛰게 되고, 인코더는 멈추지 않는다).
  ******************************************************************************
  */
#ifndef VFRAME_QUEUE_H
#define VFRAME_QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define VFQ_SLOT_NUM        4U
#define VFQ_SLOT_SIZE       (512U * 1024U)   /* 한 프레임 최대 크기(바이트) */

/* 프레임 형식. M4-7에서 MJPEG(0)를 지워 H.264만 남았다. 2단계 녹화 등 다른 형식을 위해 필드는 둔다. */
typedef enum
{
  VFQ_FMT_H264  = 1,
} vfq_format_t;

typedef struct
{
  uint8_t      *data;       /* 프레임 데이터(PSRAM) */
  uint32_t      capacity;   /* data 크기 */
  uint32_t      size;       /* 유효 바이트 수 */
  uint32_t      seq;        /* 1부터 증가하는 프레임 번호 */
  uint32_t      ts_ms;      /* 캡처 완료 시각(HAL tick, ms) */
  vfq_format_t  format;
  uint8_t       key;        /* 키프레임(H.264 IDR) */
  /* 지연 측정용 시각(vtime_now() 사이클). 0이면 기록 안 됨 */
  uint32_t      t_cap_start; /* 캡처 시작(DCMIPP VSYNC) */
  uint32_t      t_cap_end;   /* 캡처 완료(DCMIPP 프레임 이벤트) */
  uint32_t      t_enc_end;   /* 인코딩 완료(큐에 넣기 직전) */
  /* 내부용 */
  uint8_t       state;
  uint16_t      refcnt;
} vfq_frame_t;

void         vfq_init(void);

/* 생산자: 쓸 슬롯을 얻고(NULL이면 모두 사용 중), 다 쓰면 commit, 실패하면 abort */
vfq_frame_t *vfq_producer_get(void);
void         vfq_producer_commit(vfq_frame_t *f);
void         vfq_producer_abort(vfq_frame_t *f);

/* 소비자: last_seq보다 새 프레임 중 가장 최근 것을 빌린다. 없으면 NULL.
   ISR/스레드 어디서나 호출 가능(짧은 인터럽트 금지 구간 사용). */
vfq_frame_t *vfq_acquire_latest(uint32_t last_seq);
void         vfq_release(vfq_frame_t *f);

/* 통계 */
uint32_t     vfq_dropped(void);   /* 빈 슬롯이 없어 지난 프레임을 덮어쓴 횟수 */

#ifdef __cplusplus
}
#endif

#endif /* VFRAME_QUEUE_H */
