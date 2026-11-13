/**
  ******************************************************************************
  * @file    venc_ewl.c
  * @brief   VENC 인코더 라이브러리(EWL)의 작업 메모리.
  *
  *          ewl_impl.c는 인코더 인스턴스를 만들 때(EWLInit) 이 콜백으로 메모리를 받아
  *          ThreadX 바이트 풀을 만들고, 인스턴스를 지우면(EWLRelease) 풀을 지운다.
  *          JPEG와 H.264가 같은 풀을 번갈아 쓴다(동시에 두 인스턴스를 만들지 않는다).
  *          H.264 720p는 참조·재구성 프레임 등으로 수 MB가 필요하다.
  *          (x-cube-n6-ai-h264-usb-uvc 720p: 4 MB, CubeN6 VENC_USB: 1080p 기준 8 MB)
  ******************************************************************************
  */
#include <stddef.h>
#include <stdint.h>
#include "video_cfg.h"

#define VENC_EWL_POOL_SIZE   (6U * 1024U * 1024U)
static uint8_t venc_ewl_pool[VENC_EWL_POOL_SIZE] IN_PSRAM;

/* ewl_impl.c의 약한 정의를 덮어쓴다 */
void EWLPoolChoiceCb(uint8_t **pool_ptr, size_t *size)
{
  *pool_ptr = venc_ewl_pool;
  *size     = VENC_EWL_POOL_SIZE;
}

void EWLPoolReleaseCb(uint8_t **pool_ptr)
{
  (void)pool_ptr;
}
