/**
  ******************************************************************************
  * @file    settings.h
  * @brief   설정 항목 테이블(PRD 7장).
  *
  *          CLI는 이 테이블을 보고 명령·도움말·범위 검사를 만든다. 4단계 마우스 메뉴도
  *          같은 테이블을 쓰도록, 테이블은 CDC·문자열 처리와 무관하게 값 읽기/쓰기만 한다.
  *          값은 모두 정수다. 소수가 필요한 항목은 decimals 자리만큼 10의 거듭제곱을 곱해 둔다
  *          (예: 게인 12.5 dB → 125, decimals 1).
  *          설정은 전원을 끄면 사라진다(1단계는 저장하지 않음).
  ******************************************************************************
  */
#ifndef SETTINGS_H
#define SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum
{
  SETTING_BOOL = 0,   /* 0/1, 입력·표시는 off/on */
  SETTING_INT  = 1
} setting_type_t;

/* get/set 반환값 */
#define SETTING_OK            0
#define SETTING_ERR_RANGE    (-1)   /* 범위 밖(테이블 min/max로 CLI가 먼저 거른다) */
#define SETTING_ERR_NOTREADY (-2)   /* 카메라·인코더가 아직 동작 전 */
#define SETTING_ERR_AUTO     (-3)   /* 자동 모드(AE 등)가 켜져 있어 수동 값을 쓸 수 없음 */
#define SETTING_ERR_HW       (-4)   /* 센서·ISP 오류 */

typedef struct
{
  const char     *group;      /* cam, enc, log */
  const char     *name;       /* ae, exp, ... */
  setting_type_t  type;
  int32_t         min;        /* 정수 단위(decimals 반영) */
  int32_t         max;
  uint8_t         decimals;
  const char     *unit;       /* 표시 단위(없으면 "") */
  const char     *help;       /* 한 줄 설명(영문, 터미널 표시용) */
  int32_t       (*get)(int32_t *value);
  int32_t       (*set)(int32_t value);
} setting_t;

uint32_t         settings_count(void);
const setting_t *settings_at(uint32_t index);
/* group/name은 대소문자 무시. 없으면 NULL */
const setting_t *settings_find(const char *group, const char *name);
/* 범위 검사 후 set 호출 */
int32_t          settings_set(const setting_t *s, int32_t value);
const char      *settings_strerror(int32_t err);

#ifdef __cplusplus
}
#endif

#endif /* SETTINGS_H */
