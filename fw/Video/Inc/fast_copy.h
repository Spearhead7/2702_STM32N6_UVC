/**
  ******************************************************************************
  * @file    fast_copy.h
  * @brief   워드 단위 메모리 복사.
  *
  *          링크되는 newlib-nano의 memcpy는 크기 우선이라 1바이트씩 복사한다.
  *          비캐시 PSRAM에서 읽을 때는 접근 하나하나가 버스 트랜잭션이라 매우 느리다.
  *          원본 쪽을 4바이트 정렬로 맞춘 뒤 워드(8개씩 묶어)로 읽고,
  *          대상이 정렬되지 않았으면 비정렬 쓰기(Cortex-M55, Normal 메모리 허용)를 쓴다.
  *          영역이 겹치면 안 된다.
  ******************************************************************************
  */
#ifndef FAST_COPY_H
#define FAST_COPY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void fast_memcpy(void *dst, const void *src, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* FAST_COPY_H */
