/**
  ******************************************************************************
  * @file    vtime.h
  * @brief   지연 측정용 고해상도 시각(CPU 사이클 카운터 DWT->CYCCNT).
  *
  *          32비트 카운터라 CPU 800 MHz에서 약 5.3초마다 한 바퀴 돈다.
  *          두 시각의 차(뺄셈)는 5초 이내 간격이면 한 바퀴를 넘어가도 맞다.
  ******************************************************************************
  */
#ifndef VTIME_H
#define VTIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32n6xx.h"

/* 사이클 카운터 켜기(디버거 없이 실행해도 동작하도록 직접 켠다). 한 번만 부르면 된다. */
static inline void vtime_init(void)
{
  DCB->DEMCR |= DCB_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

static inline uint32_t vtime_now(void)
{
  return DWT->CYCCNT;
}

/* 사이클 수 → 마이크로초 */
static inline uint32_t vtime_us(uint32_t cycles)
{
  uint32_t mhz = SystemCoreClock / 1000000U;
  return (mhz != 0U) ? (cycles / mhz) : 0U;
}

#ifdef __cplusplus
}
#endif

#endif /* VTIME_H */
