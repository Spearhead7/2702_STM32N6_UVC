/**
  ******************************************************************************
  * @file    h264_enc.h
  * @brief   VENC H.264 모드 래퍼(1-B).
  *
  *          출력은 Annex-B 바이트 스트림(00 00 00 01 시작 코드).
  *          IDR 프레임 앞에는 항상 SPS/PPS를 붙인다. 중간에 붙는 소비자(UVC 스트림 시작,
  *          2단계 녹화 파일 분할)도 IDR 하나만 받으면 바로 디코딩을 시작할 수 있다.
  *
  *          입력은 DCMIPP–VENC 하드웨어 핸드셰이크(M4-6)로 받는다. DCMIPP가 NV12
  *          (YUV420 semi-planar)를 작은 줄 버퍼에 돌려 쓰고, 32줄마다 VENC에 신호를 준다.
  *          VENC는 그 신호에 맞춰 매크로블록 줄을 바로 인코딩하므로 캡처와 인코딩이 겹친다.
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
  uint32_t width;         /* 16의 배수 */
  uint32_t height;
  uint32_t fps;
  uint32_t bitrate_bps;   /* 목표 비트레이트(CBR) */
  uint32_t gop;           /* IDR 주기(프레임 수) */
  uint32_t line_buf_mb;   /* 하드웨어 핸드셰이크 줄 버퍼 깊이(매크로블록 줄 = 16줄 단위) */
} h264_enc_cfg_t;

/* 0이면 성공, 음수는 VENC 오류 코드 */
int32_t h264_enc_init(const h264_enc_cfg_t *cfg);

/* 한 프레임 인코딩. 다음 프레임 캡처가 시작되기 전(프레임 사이 공백)에 불러야 하고,
   그 프레임 캡처가 끝나 마지막 매크로블록 줄을 인코딩할 때까지 기다렸다 돌아온다.
   luma/chroma: 줄 버퍼의 Y, UV 시작 주소. out은 8바이트 정렬.
   반환: 출력 바이트 수(>0) 또는 음수 오류. *is_idr에 IDR 여부를 돌려준다. */
int32_t h264_enc_frame(const uint8_t *luma, const uint8_t *chroma,
                       uint8_t *out, uint32_t out_size, uint8_t *is_idr);

/* h264_enc_frame() 오류가 DCMIPP–VENC 동기 어긋남(VENC 리셋 필요)인지 */
uint8_t h264_enc_is_desync(int32_t ret);

/* 다음 프레임을 IDR로(스레드·ISR 어디서나 호출 가능) */
void    h264_enc_request_idr(void);

void    h264_enc_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* H264_ENC_H */
