/**
  ******************************************************************************
  * @file    h264_enc.c
  * @brief   VENC H.264 모드 래퍼(h264_enc.h 참고).
  *
  *          설정은 STM32CubeN6 V1.4.1 Applications/VENC/VENC_USB(venc_h264_config.c,
  *          venc_app.c)와 x-cube-n6-ai-h264-usb-uvc v2.2.1(Src/app_enc.c)을 참고했다.
  *          - Baseline(CAVLC), Level 3.1, I/P만(B 프레임 없음), 참조 프레임 1장
  *          - CBR: pictureRc + mbRc + HRD(순간 비트레이트 제한), 화면 스킵 없음
  *          - IDR 주기 gop, 요청 시 즉시 IDR. IDR마다 인코더가 SPS/PPS를 붙인다(idrHeader = 1)
  *          - DCMIPP–VENC 하드웨어 핸드셰이크(M4-6): 입력 NV12, 줄 버퍼(inputLineBuf*)를
  *            loopback으로 돌려 쓴다. VENC_USB venc_h264_config_720p_Slice.h와 같은 설정.
  *            줄 버퍼를 한 장만 쓰므로 viewMode는 SINGLE_BUFFER.
  ******************************************************************************
  */
/* <string.h>(stddef.h)를 먼저 넣어야 VideoEncoder basetype.h가 size_t를 다시 정의하지 않는다 */
#include <string.h>
#include "h264_enc.h"
#include "h264encapi.h"

/* H264EncStrmStart 출력(SPS/PPS)을 받을 자리. idrHeader = 1이라 IDR마다 인코더가 다시 넣으므로
   이 내용은 쓰지 않는다(M4-6: 예전에는 직접 한 번 더 붙여 중복됐다). */
#define H264_HDR_MAX   128U

static H264EncInst       h264_inst = NULL;
static h264_enc_cfg_t    h264_cfg;
static uint32_t          h264_frame_in_gop = 0U;     /* GOP 안에서 몇 번째 프레임인지 */
static volatile uint8_t  h264_idr_req = 1U;
static uint8_t           h264_hdr[H264_HDR_MAX] __attribute__((aligned(8)));
static uint8_t           h264_first = 1U;            /* 초기화 후 첫 프레임(timeIncrement 0) */

int32_t h264_enc_init(const h264_enc_cfg_t *cfg)
{
  H264EncConfig           config;
  H264EncPreProcessingCfg pre;
  H264EncCodingCtrl       coding;
  H264EncRateCtrl         rate;
  H264EncIn               in;
  H264EncOut              out;
  H264EncRet              ret;

  if (h264_inst != NULL)
  {
    h264_enc_deinit();
  }
  h264_cfg = *cfg;

  memset(&config, 0, sizeof(config));
  config.streamType     = H264ENC_BYTE_STREAM;
  config.viewMode       = H264ENC_BASE_VIEW_SINGLE_BUFFER;   /* VENC_USB 슬라이스 모드와 같음 */
  config.level          = H264ENC_LEVEL_3_1;
  config.width          = cfg->width;
  config.height         = cfg->height;
  config.frameRateNum   = cfg->fps;
  config.frameRateDenom = 1U;
  config.refFrameAmount = 1U;
  ret = H264EncInit(&config, &h264_inst);
  if (ret != H264ENC_OK)
  {
    h264_inst = NULL;
    return (int32_t)ret;
  }

  /* 입력 형식: DCMIPP PIPE1 출력 NV12(Y 평면 + UV 인터리브 평면) */
  ret = H264EncGetPreProcessing(h264_inst, &pre);
  if (ret == H264ENC_OK)
  {
    pre.origWidth  = cfg->width;
    pre.origHeight = cfg->height;
    pre.xOffset    = 0U;
    pre.yOffset    = 0U;
    pre.inputType  = H264ENC_YUV420_SEMIPLANAR;
    pre.rotation   = H264ENC_ROTATE_0;
    ret = H264EncSetPreProcessing(h264_inst, &pre);
  }

  /* 부호화: Baseline(CAVLC), 한 화면 한 슬라이스, IDR마다 SPS/PPS */
  if (ret == H264ENC_OK)
  {
    ret = H264EncGetCodingCtrl(h264_inst, &coding);
  }
  if (ret == H264ENC_OK)
  {
    coding.sliceSize        = 0U;
    coding.seiMessages      = 0U;
    coding.idrHeader        = 1U;
    coding.videoFullRange   = 0U;
    coding.enableCabac      = 0U;   /* Baseline */
    coding.transform8x8Mode = 0U;
    /* 하드웨어 핸드셰이크: DCMIPP 라인 이벤트로 VENC가 진행, 줄 버퍼는 depth MB 줄씩 loopback */
    coding.inputLineBufEn         = 1U;
    coding.inputLineBufLoopBackEn = 1U;
    coding.inputLineBufDepth      = cfg->line_buf_mb;
    coding.inputLineBufHwModeEn   = 1U;
    ret = H264EncSetCodingCtrl(h264_inst, &coding);
  }

  /* 율 제어: CBR */
  if (ret == H264ENC_OK)
  {
    ret = H264EncGetRateCtrl(h264_inst, &rate);
  }
  if (ret == H264ENC_OK)
  {
    rate.pictureRc    = 1U;
    rate.mbRc         = 1U;
    rate.pictureSkip  = 0U;
    rate.qpHdr        = 26;
    rate.qpMin        = 10U;
    rate.qpMax        = 51U;
    rate.bitPerSecond = cfg->bitrate_bps;
    rate.hrd          = 1U;
    rate.hrdCpbSize   = cfg->bitrate_bps;   /* 1초분 버퍼: 저지연 쪽으로 작게 */
    rate.gopLen       = cfg->gop;
    rate.intraQpDelta = 0;
    rate.fixedIntraQp = 0U;
    ret = H264EncSetRateCtrl(h264_inst, &rate);
  }

  /* 스트림 시작(필수 호출). 출력 SPS/PPS는 버린다 */
  if (ret == H264ENC_OK)
  {
    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.pOutBuf    = (u32 *)h264_hdr;
    in.busOutBuf  = (size_t)h264_hdr;
    in.outBufSize = H264_HDR_MAX;
    ret = H264EncStrmStart(h264_inst, &in, &out);
  }

  if (ret != H264ENC_OK)
  {
    h264_enc_deinit();
    return (int32_t)ret;
  }

  h264_frame_in_gop = 0U;
  h264_idr_req = 1U;
  h264_first = 1U;
  return 0;
}

int32_t h264_enc_frame(const uint8_t *luma, const uint8_t *chroma,
                       uint8_t *out, uint32_t out_size, uint8_t *is_idr)
{
  H264EncIn  in;
  H264EncOut enc_out;
  H264EncRet ret;
  uint8_t    idr;

  *is_idr = 0U;
  if (h264_inst == NULL)
  {
    return (int32_t)H264ENC_INSTANCE_ERROR;
  }

  idr = (uint8_t)((h264_idr_req != 0U) || (h264_frame_in_gop == 0U) || (h264_frame_in_gop >= h264_cfg.gop));
  if (idr != 0U)
  {
    h264_idr_req = 0U;
    h264_frame_in_gop = 0U;
  }

  memset(&in, 0, sizeof(in));
  memset(&enc_out, 0, sizeof(enc_out));
  in.busLuma       = (ptr_t)luma;
  in.busChromaU    = (ptr_t)chroma;   /* semi-planar: UV 평면(VENC_USB처럼 U, V 둘 다 같은 주소) */
  in.busChromaV    = (ptr_t)chroma;
  in.pOutBuf       = (u32 *)out;
  in.busOutBuf     = (size_t)out;
  in.outBufSize    = out_size;
  in.codingType    = (idr != 0U) ? H264ENC_INTRA_FRAME : H264ENC_PREDICTED_FRAME;
  in.timeIncrement = (h264_first != 0U) ? 0U : 1U;
  in.ipf           = H264ENC_REFERENCE_AND_REFRESH;
  in.ltrf          = H264ENC_NO_REFERENCE_NO_REFRESH;
  in.lineBufWrCnt  = 0U;
  in.sendAUD       = 0U;

  ret = H264EncStrmEncode(h264_inst, &in, &enc_out, NULL, NULL, NULL);
  if (ret != H264ENC_FRAME_READY)
  {
    h264_idr_req = 1U;   /* 실패 뒤에는 IDR부터 다시 */
    return (ret < 0) ? (int32_t)ret : (int32_t)H264ENC_ERROR;
  }

  h264_frame_in_gop++;
  h264_first = 0U;
  *is_idr = (enc_out.codingType == H264ENC_INTRA_FRAME) ? 1U : 0U;
  return (int32_t)enc_out.streamSize;
}

uint8_t h264_enc_is_desync(int32_t ret)
{
  return (uint8_t)(ret == (int32_t)H264ENC_FUSE_ERROR);
}

void h264_enc_request_idr(void)
{
  h264_idr_req = 1U;
}

void h264_enc_deinit(void)
{
  if (h264_inst != NULL)
  {
    (void)H264EncRelease(h264_inst);
    h264_inst = NULL;
  }
}
