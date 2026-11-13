/**
  ******************************************************************************
  * @file    fast_copy.c
  * @brief   워드 단위 메모리 복사(fast_copy.h 참고).
  ******************************************************************************
  */
#include "fast_copy.h"
#include "stm32n6xx.h"   /* __UNALIGNED_UINT32_WRITE */

void fast_memcpy(void *dst, const void *src, uint32_t len)
{
  uint8_t       *d = (uint8_t *)dst;
  const uint8_t *s = (const uint8_t *)src;
  const uint32_t *sw;

  /* 원본을 4바이트 정렬로 맞춘다(느린 쪽 = 원본 PSRAM 읽기를 워드로) */
  while ((len != 0U) && (((uint32_t)s & 3U) != 0U))
  {
    *d++ = *s++;
    len--;
  }

  sw = (const uint32_t *)s;

  if (((uint32_t)d & 3U) == 0U)
  {
    uint32_t *dw = (uint32_t *)d;

    while (len >= 32U)
    {
      uint32_t a = sw[0], b = sw[1], c = sw[2], e = sw[3];
      uint32_t f = sw[4], g = sw[5], h = sw[6], i = sw[7];
      dw[0] = a; dw[1] = b; dw[2] = c; dw[3] = e;
      dw[4] = f; dw[5] = g; dw[6] = h; dw[7] = i;
      sw += 8;
      dw += 8;
      len -= 32U;
    }
    while (len >= 4U)
    {
      *dw++ = *sw++;
      len -= 4U;
    }
    d = (uint8_t *)dw;
  }
  else
  {
    while (len >= 32U)
    {
      uint32_t a = sw[0], b = sw[1], c = sw[2], e = sw[3];
      uint32_t f = sw[4], g = sw[5], h = sw[6], i = sw[7];
      __UNALIGNED_UINT32_WRITE(d + 0,  a);
      __UNALIGNED_UINT32_WRITE(d + 4,  b);
      __UNALIGNED_UINT32_WRITE(d + 8,  c);
      __UNALIGNED_UINT32_WRITE(d + 12, e);
      __UNALIGNED_UINT32_WRITE(d + 16, f);
      __UNALIGNED_UINT32_WRITE(d + 20, g);
      __UNALIGNED_UINT32_WRITE(d + 24, h);
      __UNALIGNED_UINT32_WRITE(d + 28, i);
      sw += 8;
      d  += 32;
      len -= 32U;
    }
    while (len >= 4U)
    {
      __UNALIGNED_UINT32_WRITE(d, *sw++);
      d  += 4;
      len -= 4U;
    }
  }

  s = (const uint8_t *)sw;
  while (len != 0U)
  {
    *d++ = *s++;
    len--;
  }
}
