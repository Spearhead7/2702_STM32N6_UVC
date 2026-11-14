/**
  ******************************************************************************
  * @file    settings.c
  * @brief   설정 항목 테이블(settings.h 참고).
  *
  *          항목을 늘릴 때는 get/set 함수를 만들고 아래 settings_table에 한 줄 추가한다.
  *          CLI 명령(`<group> <name> [value]`)과 `help`는 자동으로 생긴다.
  ******************************************************************************
  */
#include "settings.h"
#include "app_log.h"
#include "video_app.h"
#include <stddef.h>

volatile uint8_t app_log_level = APP_LOG_LEVEL_DEFAULT;

/* video_app 오류 코드 → 설정 오류 코드 */
static int32_t map_video_err(int32_t ret)
{
  switch (ret)
  {
    case VIDEO_OK:           return SETTING_OK;
    case VIDEO_ERR_NOTREADY: return SETTING_ERR_NOTREADY;
    case VIDEO_ERR_AUTO:     return SETTING_ERR_AUTO;
    default:                 return SETTING_ERR_HW;
  }
}

/* ---- cam ---------------------------------------------------------------- */
static int32_t get_cam_ae(int32_t *v)
{
  uint8_t on = 0U;
  int32_t ret = video_cam_get_ae(&on);
  *v = (int32_t)on;
  return map_video_err(ret);
}

static int32_t set_cam_ae(int32_t v)
{
  return map_video_err(video_cam_set_ae((uint8_t)(v != 0)));
}

static int32_t get_cam_awb(int32_t *v)
{
  uint8_t on = 0U;
  int32_t ret = video_cam_get_awb(&on);
  *v = (int32_t)on;
  return map_video_err(ret);
}

static int32_t set_cam_awb(int32_t v)
{
  return map_video_err(video_cam_set_awb((uint8_t)(v != 0)));
}

static int32_t get_cam_exp(int32_t *v)
{
  return map_video_err(video_cam_get_exposure(v));
}

static int32_t set_cam_exp(int32_t v)
{
  return map_video_err(video_cam_set_exposure(v));
}

/* 게인: 센서는 mdB, 표 값은 0.1 dB 단위 */
static int32_t get_cam_gain(int32_t *v)
{
  int32_t mdb = 0;
  int32_t ret = video_cam_get_gain(&mdb);
  *v = mdb / 100;
  return map_video_err(ret);
}

static int32_t set_cam_gain(int32_t v)
{
  return map_video_err(video_cam_set_gain(v * 100));
}

/* ---- enc ---------------------------------------------------------------- */
static int32_t get_enc_bitrate(int32_t *v)
{
  *v = (int32_t)video_get_bitrate_kbps();
  return SETTING_OK;
}

static int32_t set_enc_bitrate(int32_t v)
{
  video_set_bitrate_kbps((uint32_t)v);
  return SETTING_OK;
}

static int32_t get_enc_gop(int32_t *v)
{
  *v = (int32_t)video_get_gop();
  return SETTING_OK;
}

static int32_t set_enc_gop(int32_t v)
{
  video_set_gop((uint32_t)v);
  return SETTING_OK;
}

/* ---- log ---------------------------------------------------------------- */
static int32_t get_log_level(int32_t *v)
{
  *v = (int32_t)app_log_level;
  return SETTING_OK;
}

static int32_t set_log_level(int32_t v)
{
  app_log_level = (uint8_t)v;
  return SETTING_OK;
}

/* ---- 테이블 -------------------------------------------------------------- */
static const setting_t settings_table[] =
{
  { "cam", "ae",      SETTING_BOOL, 0,    1,     0, "",
    "auto exposure on/off",                          get_cam_ae,      set_cam_ae },
  { "cam", "exp",     SETTING_INT,  10,   33000, 0, "us",
    "manual exposure time (needs cam ae off)",       get_cam_exp,     set_cam_exp },
  { "cam", "gain",    SETTING_INT,  0,    720,   1, "dB",
    "manual sensor gain (needs cam ae off)",         get_cam_gain,    set_cam_gain },
  { "cam", "awb",     SETTING_BOOL, 0,    1,     0, "",
    "auto white balance on/off (off keeps current)", get_cam_awb,     set_cam_awb },
  { "enc", "bitrate", SETTING_INT,  1000, 8000,  0, "kbps",
    "H.264 CBR target bitrate (encoder restarts)",   get_enc_bitrate, set_enc_bitrate },
  { "enc", "gop",     SETTING_INT,  1,    300,   0, "frames",
    "IDR period (encoder restarts)",                 get_enc_gop,     set_enc_gop },
  { "log", "level",   SETTING_INT,  0,    (int32_t)APP_LOG_LEVEL_MAX, 0, "",
    "USART1 periodic stats: 0 off,1 VIDEO,2 +UVC,3 +LAT", get_log_level, set_log_level },
};

#define SETTINGS_NUM   (sizeof(settings_table) / sizeof(settings_table[0]))

static int name_eq(const char *a, const char *b)
{
  while ((*a != '\0') && (*b != '\0'))
  {
    char ca = *a++;
    char cb = *b++;
    if ((ca >= 'A') && (ca <= 'Z')) { ca = (char)(ca + ('a' - 'A')); }
    if ((cb >= 'A') && (cb <= 'Z')) { cb = (char)(cb + ('a' - 'A')); }
    if (ca != cb)
    {
      return 0;
    }
  }
  return (*a == '\0') && (*b == '\0');
}

uint32_t settings_count(void)
{
  return (uint32_t)SETTINGS_NUM;
}

const setting_t *settings_at(uint32_t index)
{
  return (index < SETTINGS_NUM) ? &settings_table[index] : NULL;
}

const setting_t *settings_find(const char *group, const char *name)
{
  uint32_t i;

  for (i = 0U; i < SETTINGS_NUM; i++)
  {
    if (name_eq(settings_table[i].group, group) && name_eq(settings_table[i].name, name))
    {
      return &settings_table[i];
    }
  }
  return NULL;
}

int32_t settings_set(const setting_t *s, int32_t value)
{
  if ((value < s->min) || (value > s->max))
  {
    return SETTING_ERR_RANGE;
  }
  return s->set(value);
}

const char *settings_strerror(int32_t err)
{
  switch (err)
  {
    case SETTING_ERR_RANGE:    return "out of range";
    case SETTING_ERR_NOTREADY: return "camera not running";
    case SETTING_ERR_AUTO:     return "auto mode is on (turn it off first)";
    case SETTING_ERR_HW:       return "sensor/ISP error";
    default:                   return "failed";
  }
}
