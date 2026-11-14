/**
  ******************************************************************************
  * @file    cli.c
  * @brief   대화형 CLI(cli.h, PRD 7장).
  *
  *          줄 편집 → 토큰 나누기 → 내장 명령(help/version/status/reboot/enc idr) 또는
  *          설정 항목 테이블(settings.c)의 `<group> <name> [value]`.
  *          터미널 표시용 문자열은 영문(Tera Term 기본 설정에서 한글이 깨지지 않게).
  ******************************************************************************
  */
#include "cli.h"
#include "settings.h"
#include "app_version.h"
#include "video_app.h"
#include "video_cfg.h"
#include "vframe_queue.h"
#include "ux_device_video.h"
#include "main.h"
#include "tx_api.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define CLI_OUT_SIZE    512U
#define CLI_MAX_ARGS    6U

static cli_write_fn_t cli_write = NULL;

static char     cli_line[CLI_LINE_MAX + 1U];
static uint32_t cli_len = 0U;
static uint8_t  cli_last_cr = 0U;    /* 바로 앞 글자가 CR이었는지(CRLF의 LF 무시) */
static uint8_t  cli_esc = 0U;        /* ESC 시퀀스(화살표 키 등) 건너뛰기 상태 */
static volatile uint8_t cli_reset_req = 0U;  /* 배너를 보냈으니 입력 중이던 줄을 버림 */

static uint8_t  cli_out[CLI_OUT_SIZE];
static uint32_t cli_out_len = 0U;

/* ---------------------------------------------------------------------------
 * 출력
 * ------------------------------------------------------------------------- */
static void out_flush(void)
{
  if ((cli_out_len != 0U) && (cli_write != NULL))
  {
    cli_write(cli_out, cli_out_len);
  }
  cli_out_len = 0U;
}

static void out_bytes(const char *s, uint32_t n)
{
  while (n != 0U)
  {
    uint32_t room = CLI_OUT_SIZE - cli_out_len;
    uint32_t k = (n < room) ? n : room;

    memcpy(&cli_out[cli_out_len], s, k);
    cli_out_len += k;
    s += k;
    n -= k;
    if (cli_out_len == CLI_OUT_SIZE)
    {
      out_flush();
    }
  }
}

static void out_str(const char *s)
{
  out_bytes(s, (uint32_t)strlen(s));
}

static void out_printf(const char *fmt, ...)
{
  char buf[160];
  va_list ap;
  int n;

  va_start(ap, fmt);
  n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (n > 0)
  {
    out_bytes(buf, ((uint32_t)n < sizeof(buf)) ? (uint32_t)n : (uint32_t)(sizeof(buf) - 1U));
  }
}

static void out_ok(void)
{
  out_str("OK\r\n");
}

static void out_error(const char *fmt, ...)
{
  char buf[128];
  va_list ap;

  va_start(ap, fmt);
  (void)vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  out_str("ERROR: ");
  out_str(buf);
  out_str("\r\n");
}

/* x10 값을 "30.0"처럼 */
static void fmt_x10(char *buf, uint32_t size, uint32_t v)
{
  (void)snprintf(buf, size, "%lu.%lu", (unsigned long)(v / 10U), (unsigned long)(v % 10U));
}

/* 설정 값(decimals 자리 정수)을 문자열로 */
static void fmt_value(char *buf, uint32_t size, const setting_t *s, int32_t v)
{
  if (s->type == SETTING_BOOL)
  {
    (void)snprintf(buf, size, "%s", (v != 0) ? "on" : "off");
  }
  else if (s->decimals == 0U)
  {
    (void)snprintf(buf, size, "%ld", (long)v);
  }
  else
  {
    /* 소수 자리는 1~3자리만 쓴다(게인 0.1 dB) */
    static const uint32_t pow10[4] = { 1U, 10U, 100U, 1000U };
    uint32_t d = (s->decimals > 3U) ? 3U : s->decimals;
    uint32_t a = (uint32_t)((v < 0) ? -v : v);
    char     frac[4];

    (void)snprintf(frac, sizeof(frac), "%03lu", (unsigned long)((a % pow10[d]) % 1000U));
    (void)snprintf(buf, size, "%s%lu.%s", (v < 0) ? "-" : "", (unsigned long)(a / pow10[d]),
                   &frac[3U - d]);
  }
}

/* ---------------------------------------------------------------------------
 * 입력 해석
 * ------------------------------------------------------------------------- */
static char lower(char c)
{
  return ((c >= 'A') && (c <= 'Z')) ? (char)(c + ('a' - 'A')) : c;
}

static int str_eq(const char *a, const char *b)
{
  while ((*a != '\0') && (*b != '\0'))
  {
    if (lower(*a++) != lower(*b++))
    {
      return 0;
    }
  }
  return (*a == '\0') && (*b == '\0');
}

/* "123", "-5", "12.5"(decimals 자리까지) → 정수. 성공 1 */
static int parse_value(const setting_t *s, const char *str, int32_t *out)
{
  int32_t  v = 0;
  int32_t  sign = 1;
  uint32_t frac_digits = 0U;
  uint8_t  in_frac = 0U;
  uint8_t  any = 0U;

  if (s->type == SETTING_BOOL)
  {
    if (str_eq(str, "on") || str_eq(str, "1"))
    {
      *out = 1;
      return 1;
    }
    if (str_eq(str, "off") || str_eq(str, "0"))
    {
      *out = 0;
      return 1;
    }
    return 0;
  }

  if (*str == '-')
  {
    sign = -1;
    str++;
  }
  for (; *str != '\0'; str++)
  {
    if ((*str == '.') && (in_frac == 0U) && (s->decimals != 0U))
    {
      in_frac = 1U;
      continue;
    }
    if ((*str < '0') || (*str > '9'))
    {
      return 0;
    }
    if (in_frac != 0U)
    {
      if (frac_digits >= s->decimals)
      {
        continue;   /* 남는 소수 자리는 버린다 */
      }
      frac_digits++;
    }
    if (v > 100000000)
    {
      return 0;     /* 너무 큼 */
    }
    v = (v * 10) + (*str - '0');
    any = 1U;
  }
  if (any == 0U)
  {
    return 0;
  }
  while (frac_digits < s->decimals)
  {
    v *= 10;
    frac_digits++;
  }
  *out = v * sign;
  return 1;
}

/* ---------------------------------------------------------------------------
 * 명령
 * ------------------------------------------------------------------------- */
static void cmd_help_group(const char *group)
{
  uint32_t i;
  uint8_t  found = 0U;

  for (i = 0U; i < settings_count(); i++)
  {
    const setting_t *s = settings_at(i);
    char lo[16];
    char hi[16];
    char arg[48];

    if (!str_eq(s->group, group))
    {
      continue;
    }
    found = 1U;
    if (s->type == SETTING_BOOL)
    {
      (void)snprintf(arg, sizeof(arg), "[on|off]");
    }
    else
    {
      fmt_value(lo, sizeof(lo), s, s->min);
      fmt_value(hi, sizeof(hi), s, s->max);
      (void)snprintf(arg, sizeof(arg), "[%s-%s%s%s]", lo, hi, (s->unit[0] != '\0') ? " " : "", s->unit);
    }
    out_printf("  %s %-7s %-20s %s\r\n", s->group, s->name, arg, s->help);
  }
  if (str_eq(group, "enc"))
  {
    out_printf("  enc %-7s %-20s %s\r\n", "idr", "", "force next frame to IDR");
    found = 1U;
  }
  if (found == 0U)
  {
    out_error("unknown group '%s' (type help)", group);
    return;
  }
  out_str("  (no value: show current value)\r\n");
  out_ok();
}

static void cmd_help(void)
{
  uint32_t i;
  const char *prev = "";

  out_str("commands:\r\n");
  out_str("  help [group]   show commands of a group\r\n");
  out_str("  version        firmware version\r\n");
  out_str("  status         stream / encoder / camera status\r\n");
  out_str("  reboot [m]     reset, m = wdg (default) | sys | warm\r\n");
  out_str("groups:");
  for (i = 0U; i < settings_count(); i++)
  {
    const setting_t *s = settings_at(i);
    if (!str_eq(s->group, prev))
    {
      out_printf(" %s", s->group);
      prev = s->group;
    }
  }
  out_str("   (type: help <group>)\r\n");
  out_ok();
}

static void cmd_version(void)
{
  out_printf("%s firmware %s\r\n", APP_FW_NAME, APP_FW_VERSION);
  out_printf("build    : %s %s\r\n", __DATE__, __TIME__);
  out_ok();
}

static void cmd_status(void)
{
  const video_cfg_t   *cfg = video_cfg_get();
  const video_stats_t *vs = video_app_stats();
  const video_live_t  *vl = video_app_live();
  uvc_status_t us;
  char a[12];
  char b[12];
  char c[12];
  uint8_t ae = 0U;
  uint8_t awb = 0U;
  int32_t exp_us = 0;
  int32_t gain_mdb = 0;

  uvc_status_get(&us);

  out_printf("stream   : %s  H264 %lux%lu@%lu\r\n", (us.streaming != 0U) ? "on " : "off",
             (unsigned long)cfg->width, (unsigned long)cfg->height, (unsigned long)cfg->fps);
  if (vs->init_status != 0)
  {
    out_printf("camera   : not running (init %ld)\r\n", (long)vs->init_status);
  }
  if (vl->valid != 0U)
  {
    fmt_x10(a, sizeof(a), vl->cap_fps_x10);
    fmt_x10(b, sizeof(b), vl->enc_fps_x10);
    fmt_x10(c, sizeof(c), us.sent_fps_x10);
    out_printf("fps      : cap %s  enc %s  usb %s  (5 s avg)\r\n", a, b, c);
    fmt_x10(a, sizeof(a), vl->bitrate_kbps / 100U);
    out_printf("bitrate  : %s Mbps  (target %lu kbps, gop %lu)\r\n", a,
               (unsigned long)video_get_bitrate_kbps(), (unsigned long)video_get_gop());
  }
  else
  {
    out_printf("fps      : -  (first 5 s)\r\n");
    out_printf("bitrate  : -  (target %lu kbps, gop %lu)\r\n",
               (unsigned long)video_get_bitrate_kbps(), (unsigned long)video_get_gop());
  }
  if (us.lat_valid != 0U)
  {
    out_printf("latency  : %lu.%lu ms avg, %lu.%lu ms max  (capture -> USB done)\r\n",
               (unsigned long)(us.lat_avg_us / 1000U), (unsigned long)((us.lat_avg_us / 100U) % 10U),
               (unsigned long)(us.lat_max_us / 1000U), (unsigned long)((us.lat_max_us / 100U) % 10U));
  }
  out_printf("drop     : late %lu  noslot %lu  queue %lu  wait-idr %lu  desync %lu  encerr %lu\r\n",
             (unsigned long)vs->cap_skipped, (unsigned long)vs->no_slot, (unsigned long)vfq_dropped(),
             (unsigned long)us.skipped, (unsigned long)vs->desync, (unsigned long)vs->enc_errors);
  out_printf("usb loss : isoinc %lu  buferr %lu\r\n", (unsigned long)us.isoinc, (unsigned long)us.buferr);
  if ((video_cam_get_ae(&ae) == VIDEO_OK) && (video_cam_get_awb(&awb) == VIDEO_OK) &&
      (video_cam_get_exposure(&exp_us) == VIDEO_OK) && (video_cam_get_gain(&gain_mdb) == VIDEO_OK))
  {
    out_printf("camera   : ae %s  awb %s  exp %ld us  gain %ld.%ld dB\r\n",
               (ae != 0U) ? "on" : "off", (awb != 0U) ? "on" : "off", (long)exp_us,
               (long)(gain_mdb / 1000), (long)((gain_mdb / 100) % 10));
  }
  out_ok();
}

/* reboot [wdg|sys|warm] (M6 진단: 리셋 방식별로 부트 ROM이 다시 부팅하는지 본다)
   wdg (기본): IWDG 리셋. 시스템 전체 리셋. LSI 32 kHz, /4, 2틱 ≈ 0.25 ms.
   sys       : SYSCFG CM55RSTCR CORE_RESET_TYPE = 1 + NVIC_SystemReset().
   warm      : NVIC_SystemReset()만(N6 기본값은 코어만 다시 시작하는 warm reset). */
static void cmd_reboot(const char *mode)
{
  uint32_t m;

  if ((mode == NULL) || str_eq(mode, "wdg"))
  {
    m = 0U;
  }
  else if (str_eq(mode, "sys"))
  {
    m = 1U;
  }
  else if (str_eq(mode, "warm"))
  {
    m = 2U;
  }
  else
  {
    out_error("mode must be wdg, sys or warm");
    return;
  }

  out_printf("rebooting (%s)...\r\n", (m == 0U) ? "wdg" : ((m == 1U) ? "sys" : "warm"));
  out_ok();
  out_flush();
  tx_thread_sleep((100U * TX_TIMER_TICKS_PER_SECOND / 1000U) + 1U);
  __disable_irq();
  /* POR로만 초기화되는 PSRAM 패드 전압 범위를 되돌린다(이 뒤로 PSRAM 접근 금지, main.c) */
  board_prepare_reset();

  if (m == 0U)
  {
    IWDG->KR  = 0xCCCCU;   /* 시작(LSI도 함께 켜진다) */
    IWDG->KR  = 0x5555U;   /* PR·RLR 쓰기 허용 */
    IWDG->PR  = 0U;
    IWDG->RLR = 1U;
    while (IWDG->SR != 0U)
    {
    }
    IWDG->KR  = 0xAAAAU;   /* 새 값으로 다시 적재 */
    while (1)
    {
    }
  }
  if (m == 1U)
  {
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    HAL_SYSCFG_EnablePowerOnReset();
    __DSB();
  }
  NVIC_SystemReset();
}

static void cmd_setting(const setting_t *s, const char *value)
{
  int32_t v;
  int32_t ret;
  char    buf[16];
  char    lo[16];
  char    hi[16];

  if (value != NULL)
  {
    if (parse_value(s, value, &v) == 0)
    {
      if (s->type == SETTING_BOOL)
      {
        out_error("%s %s: use on or off", s->group, s->name);
      }
      else
      {
        out_error("invalid number '%s'", value);
      }
      return;
    }
    ret = settings_set(s, v);
    if (ret == SETTING_ERR_RANGE)
    {
      fmt_value(lo, sizeof(lo), s, s->min);
      fmt_value(hi, sizeof(hi), s, s->max);
      out_error("%s range is %s-%s%s%s", s->name, lo, hi, (s->unit[0] != '\0') ? " " : "", s->unit);
      return;
    }
    if (ret != SETTING_OK)
    {
      out_error("%s %s: %s", s->group, s->name, settings_strerror(ret));
      return;
    }
  }

  ret = s->get(&v);
  if (ret != SETTING_OK)
  {
    out_error("%s %s: %s", s->group, s->name, settings_strerror(ret));
    return;
  }
  fmt_value(buf, sizeof(buf), s, v);
  out_printf("%s %s : %s%s%s\r\n", s->group, s->name, buf, (s->unit[0] != '\0') ? " " : "", s->unit);
  out_ok();
}

static void cli_execute(char *line)
{
  char    *argv[CLI_MAX_ARGS];
  uint32_t argc = 0U;
  char    *p = line;

  /* 띄어쓰기(스페이스·탭)로 나누기 */
  while (*p != '\0')
  {
    while ((*p == ' ') || (*p == '\t'))
    {
      *p++ = '\0';
    }
    if (*p == '\0')
    {
      break;
    }
    if (argc == CLI_MAX_ARGS)
    {
      out_error("too many arguments");
      return;
    }
    argv[argc++] = p;
    while ((*p != '\0') && (*p != ' ') && (*p != '\t'))
    {
      p++;
    }
  }
  if (argc == 0U)
  {
    return;   /* 빈 줄: 프롬프트만 */
  }

  if (str_eq(argv[0], "help") || str_eq(argv[0], "?"))
  {
    if (argc == 1U)
    {
      cmd_help();
    }
    else
    {
      cmd_help_group(argv[1]);
    }
    return;
  }
  if (str_eq(argv[0], "version") && (argc == 1U))
  {
    cmd_version();
    return;
  }
  if (str_eq(argv[0], "status") && (argc == 1U))
  {
    cmd_status();
    return;
  }
  if (str_eq(argv[0], "reboot") && (argc <= 2U))
  {
    cmd_reboot((argc == 2U) ? argv[1] : NULL);
    return;
  }
  if ((argc == 2U) && str_eq(argv[0], "enc") && str_eq(argv[1], "idr"))
  {
    video_request_keyframe();
    out_str("idr requested\r\n");
    out_ok();
    return;
  }

  if (argc >= 2U)
  {
    const setting_t *s = settings_find(argv[0], argv[1]);
    if (s != NULL)
    {
      if (argc > 3U)
      {
        out_error("too many arguments");
      }
      else
      {
        cmd_setting(s, (argc == 3U) ? argv[2] : NULL);
      }
      return;
    }
  }
  out_error("unknown command '%s%s%s' (type help)", argv[0], (argc >= 2U) ? " " : "",
            (argc >= 2U) ? argv[1] : "");
}

/* ---------------------------------------------------------------------------
 * 공개 함수
 * ------------------------------------------------------------------------- */
void cli_init(cli_write_fn_t write_fn)
{
  cli_write = write_fn;
  cli_len = 0U;
  cli_last_cr = 0U;
  cli_esc = 0U;
  cli_out_len = 0U;
}

void cli_banner(void)
{
  /* 배너 스레드에서 불린다: CLI 스레드의 출력 버퍼는 건드리지 않고 따로 만들어 보낸다.
     입력 중이던 줄은 CLI 스레드가 다음 입력 때 버린다. */
  static char banner[160];
  int n;

  cli_reset_req = 1U;
  n = snprintf(banner, sizeof(banner), "\r\n%s firmware %s (%s %s)\r\ntype 'help' for commands\r\n%s",
               APP_FW_NAME, APP_FW_VERSION, __DATE__, __TIME__, CLI_PROMPT);
  if ((n > 0) && (cli_write != NULL))
  {
    cli_write((const uint8_t *)banner, ((uint32_t)n < sizeof(banner)) ? (uint32_t)n : (uint32_t)(sizeof(banner) - 1U));
  }
}

void cli_input(const uint8_t *data, uint32_t len)
{
  uint32_t i;

  if (cli_reset_req != 0U)
  {
    cli_reset_req = 0U;
    cli_len = 0U;
    cli_esc = 0U;
    cli_last_cr = 0U;
  }

  for (i = 0U; i < len; i++)
  {
    char ch = (char)data[i];

    /* ESC [ ... 끝 글자(영문자 또는 ~)까지 무시: 화살표 이력은 P1 */
    if (cli_esc != 0U)
    {
      if ((cli_esc == 1U) && (ch == '['))
      {
        cli_esc = 2U;
      }
      else if ((cli_esc == 1U) || ((ch >= '@') && (ch <= '~')))
      {
        cli_esc = 0U;
      }
      continue;
    }

    if ((ch == '\r') || (ch == '\n'))
    {
      if ((ch == '\n') && (cli_last_cr != 0U))
      {
        cli_last_cr = 0U;   /* CRLF의 LF */
        continue;
      }
      cli_last_cr = (uint8_t)(ch == '\r');
      out_str("\r\n");
      cli_line[cli_len] = '\0';
      cli_execute(cli_line);
      cli_len = 0U;
      out_str(CLI_PROMPT);
      continue;
    }
    cli_last_cr = 0U;

    switch (ch)
    {
      case 0x1B:            /* ESC */
        cli_esc = 1U;
        break;
      case 0x03:            /* Ctrl+C: 줄 취소 */
        out_str("^C\r\n" CLI_PROMPT);
        cli_len = 0U;
        break;
      case 0x08:            /* Backspace */
      case 0x7F:            /* DEL(Tera Term 기본 Backspace) */
        if (cli_len != 0U)
        {
          cli_len--;
          out_str("\b \b");
        }
        break;
      default:
        if ((ch >= 0x20) && (ch < 0x7F))
        {
          if (cli_len < CLI_LINE_MAX)
          {
            cli_line[cli_len++] = ch;
            out_bytes(&ch, 1U);
          }
          else
          {
            out_str("\a");   /* 줄이 꽉 참 */
          }
        }
        break;
    }
  }
  out_flush();
}
