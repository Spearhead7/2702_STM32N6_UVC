/**
  ******************************************************************************
  * @file    cli.h
  * @brief   대화형 CLI(PRD 7장). 입출력 장치(CDC)와 무관하다.
  *
  *          - 프롬프트 `n6uvc> `, 받은 글자 에코, Enter(CR 또는 LF, CRLF)로 실행,
  *            Backspace/DEL로 지우기, Ctrl+C로 줄 취소. 줄 길이 최대 128자.
  *          - 명령: help, version, status, reboot, enc idr, 그리고 설정 항목 테이블
  *            (settings.h)의 `<group> <name> [value]`.
  *          - 결과 마지막 줄은 항상 `OK` 또는 `ERROR: <이유>`.
  *          cli_input은 CDC CLI 스레드에서만, cli_banner는 배너 스레드에서 부른다.
  ******************************************************************************
  */
#ifndef CLI_H
#define CLI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* 출력 함수: 다 보낼 때까지(또는 타임아웃까지) 블로킹해도 된다 */
typedef void (*cli_write_fn_t)(const uint8_t *data, uint32_t len);

#define CLI_LINE_MAX   128U
#define CLI_PROMPT     "n6uvc> "

void cli_init(cli_write_fn_t write_fn);
/* 터미널이 포트를 열었을 때(DTR=1): 배너와 프롬프트를 바로 보낸다.
   입력 중이던 줄은 다음 cli_input에서 버린다. cli_input과 다른 스레드에서 불러도 된다. */
void cli_banner(void);
/* 받은 바이트 처리(에코·편집·실행). 처리 끝에 출력을 내보낸다. */
void cli_input(const uint8_t *data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* CLI_H */
