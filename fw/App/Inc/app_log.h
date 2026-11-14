/**
  ******************************************************************************
  * @file    app_log.h
  * @brief   USART1 디버그 로그 레벨(CLI `log level`).
  *
  *          0: 주기 통계 끔, 1: VIDEO 줄, 2: + UVC 줄, 3: + UVCPAY·LAT 줄(기본).
  *          연결/해제·오류 같은 사건 로그는 레벨과 상관없이 나온다.
  ******************************************************************************
  */
#ifndef APP_LOG_H
#define APP_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define APP_LOG_LEVEL_MAX       3U
#define APP_LOG_LEVEL_DEFAULT   3U

extern volatile uint8_t app_log_level;

#ifdef __cplusplus
}
#endif

#endif /* APP_LOG_H */
