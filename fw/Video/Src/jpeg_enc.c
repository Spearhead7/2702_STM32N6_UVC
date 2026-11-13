/**
  ******************************************************************************
  * @file    jpeg_enc.c
  * @brief   VENC JPEG 모드 래퍼.
  *          사용법은 STM32CubeN6 V1.4.1 Applications/VENC/VENC_JPEG_Encoding
  *          (FSBL/Src/imageEncode.c)을 따랐다.
  ******************************************************************************
  */
/* <string.h>(stddef.h)를 먼저 넣어야 VideoEncoder basetype.h가 size_t를 다시 정의하지 않는다 */
#include <string.h>
#include "jpeg_enc.h"
#include "video_cfg.h"
#include "jpegencapi.h"
#include "ewl.h"
#include "ewl_impl.h"

/* EWL 작업 메모리는 venc_ewl.c(JPEG·H.264 공용) */
static JpegEncInst jpeg_inst = NULL;
static JpegEncCfg  jpeg_cfg;

int32_t jpeg_enc_init(uint32_t width, uint32_t height, uint32_t qlevel)
{
  JpegEncRet ret;

  if (jpeg_inst != NULL)
  {
    jpeg_enc_deinit();
  }

  memset(&jpeg_cfg, 0, sizeof(jpeg_cfg));
  jpeg_cfg.qLevel          = (qlevel > 9U) ? 9U : qlevel;
  jpeg_cfg.inputWidth      = width;
  jpeg_cfg.inputHeight     = height;
  jpeg_cfg.codingWidth     = width;
  jpeg_cfg.codingHeight    = height;
  jpeg_cfg.xOffset         = 0U;
  jpeg_cfg.yOffset         = 0U;
  jpeg_cfg.restartInterval = 0U;
  jpeg_cfg.frameType       = JPEGENC_YUV422_INTERLEAVED_YUYV;
  jpeg_cfg.rotation        = JPEGENC_ROTATE_0;
  jpeg_cfg.codingType      = JPEGENC_WHOLE_FRAME;
  jpeg_cfg.codingMode      = JPEGENC_420_MODE;
  jpeg_cfg.unitsType       = JPEGENC_DOTS_PER_INCH;
  jpeg_cfg.markerType      = JPEGENC_SINGLE_MARKER;
  jpeg_cfg.xDensity        = 72U;
  jpeg_cfg.yDensity        = 72U;

  ret = JpegEncInit(&jpeg_cfg, &jpeg_inst);
  if (ret != JPEGENC_OK)
  {
    jpeg_inst = NULL;
    return (int32_t)ret;
  }

  ret = JpegEncSetPictureSize(jpeg_inst, &jpeg_cfg);
  if (ret != JPEGENC_OK)
  {
    jpeg_enc_deinit();
    return (int32_t)ret;
  }

  return 0;
}

int32_t jpeg_enc_frame(const uint8_t *yuyv, uint8_t *out, uint32_t out_size)
{
  JpegEncIn  in;
  JpegEncOut enc_out;
  JpegEncRet ret;

  if (jpeg_inst == NULL)
  {
    return (int32_t)JPEGENC_INSTANCE_ERROR;
  }

  memset(&in, 0, sizeof(in));
  memset(&enc_out, 0, sizeof(enc_out));

  in.frameHeader = 1U;
  in.busLum      = (size_t)yuyv;   /* 인터리브 포맷은 busLum만 쓴다 */
  in.busCb       = (size_t)yuyv;
  in.busCr       = (size_t)yuyv;
  in.pLum        = yuyv;
  in.pOutBuf     = out;
  in.busOutBuf   = (size_t)out;
  in.outBufSize  = out_size;

  do
  {
    ret = JpegEncEncode(jpeg_inst, &in, &enc_out, NULL, NULL);
  } while (ret == JPEGENC_RESTART_INTERVAL);

  if (ret != JPEGENC_FRAME_READY)
  {
    return (ret < 0) ? (int32_t)ret : (int32_t)JPEGENC_ERROR;
  }

  return (int32_t)enc_out.jfifSize;
}

void jpeg_enc_deinit(void)
{
  if (jpeg_inst != NULL)
  {
    (void)JpegEncRelease(jpeg_inst);
    jpeg_inst = NULL;
  }
}
