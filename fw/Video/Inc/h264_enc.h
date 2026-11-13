/**
  ******************************************************************************
  * @file    h264_enc.h
  * @brief   VENC H.264 모드 래퍼(1-B).
  *
  *          출력은 Annex-B 바이트 스트림(00 00 00 01 시작 코드).
  *          IDR 프레임 앞에는 항상 SPS/PPS를 붙인다. 중간에 붙는 소비자(UVC 스트림 시작,
  *          2단계 녹화 파일 분할)도 IDR 하나만 받으면 바로 디코딩을 시작할 수 있다.
  ******************************************************************************
  */
#ifndef H264_ENC_H
#define H264_ENC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct
{
  uint32_t width;         /* 16의 배수 권장 */
  uint32_t height;
  uint32_t fps;
  uint32_t bitrate_bps;   /* 목표 비트레이트(CBR) */
  uint32_t gop;           /* IDR 주기(프레임 수) */
} h264_enc_cfg_t;

/* 입력은 YUV422 인터리브(YUYV, pitch = width*2). 0이면 성공, 음수는 VENC 오류 코드 */
int32_t h264_enc_init(const h264_enc_cfg_t *cfg);

/* 한 프레임 인코딩. 반환: 출력 바이트 수(>0) 또는 음수 오류.
   out은 8바이트 정렬이어야 한다. *is_idr에 IDR 여부를 돌려준다. */
int32_t h264_enc_frame(const uint8_t *yuyv, uint8_t *out, uint32_t out_size, uint8_t *is_idr);

/* 다음 프레임을 IDR로(스레드·ISR 어디서나 호출 가능) */
void    h264_enc_request_idr(void);

void    h264_enc_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* H264_ENC_H */
