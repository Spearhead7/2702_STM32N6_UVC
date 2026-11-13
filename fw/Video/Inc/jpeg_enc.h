/**
  ******************************************************************************
  * @file    jpeg_enc.h
  * @brief   N6 하드웨어 인코더(VENC) JPEG 모드 래퍼.
  *          입력: YUV422 YUYV(인터리브), 출력: JFIF(JPEG) 한 장.
  *          1-B 단계에서 H.264 래퍼(h264_enc)가 같은 자리에 들어간다.
  ******************************************************************************
  */
#ifndef JPEG_ENC_H
#define JPEG_ENC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* 0 성공, 음수 실패. LL_VENC_Init() 이후에 호출한다. */
int32_t jpeg_enc_init(uint32_t width, uint32_t height, uint32_t qlevel);

/* yuyv(폭*2 바이트 간격) 한 장을 out에 JPEG로 인코딩한다.
   반환: JPEG 바이트 수(>0), 실패 시 음수(JpegEncRet 값). VENC 완료까지 블록된다. */
int32_t jpeg_enc_frame(const uint8_t *yuyv, uint8_t *out, uint32_t out_size);

void    jpeg_enc_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* JPEG_ENC_H */
