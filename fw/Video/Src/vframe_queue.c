/**
  ******************************************************************************
  * @file    vframe_queue.c
  * @brief   인코딩된 프레임 큐. 설명은 vframe_queue.h 참고.
  ******************************************************************************
  */
#include "vframe_queue.h"
#include "video_cfg.h"
#include "stm32n6xx.h"
#include <stddef.h>

#define ST_FREE     0U
#define ST_WRITING  1U
#define ST_READY    2U

static uint8_t vfq_mem[VFQ_SLOT_NUM][VFQ_SLOT_SIZE] IN_PSRAM;

static vfq_frame_t vfq_slot[VFQ_SLOT_NUM];
static uint32_t    vfq_next_seq;
static uint32_t    vfq_drop_count;

static inline uint32_t vfq_lock(void)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  return primask;
}

static inline void vfq_unlock(uint32_t primask)
{
  __set_PRIMASK(primask);
}

void vfq_init(void)
{
  for (uint32_t i = 0U; i < VFQ_SLOT_NUM; i++)
  {
    vfq_slot[i].data     = vfq_mem[i];
    vfq_slot[i].capacity = VFQ_SLOT_SIZE;
    vfq_slot[i].size     = 0U;
    vfq_slot[i].seq      = 0U;
    vfq_slot[i].ts_ms    = 0U;
    vfq_slot[i].format   = VFQ_FMT_H264;
    vfq_slot[i].key      = 0U;
    vfq_slot[i].state    = ST_FREE;
    vfq_slot[i].refcnt   = 0U;
  }
  vfq_next_seq   = 1U;
  vfq_drop_count = 0U;
}

vfq_frame_t *vfq_producer_get(void)
{
  vfq_frame_t *pick = NULL;
  vfq_frame_t *oldest = NULL;
  uint32_t newest_seq = 0U;
  uint32_t key = vfq_lock();

  for (uint32_t i = 0U; i < VFQ_SLOT_NUM; i++)
  {
    if ((vfq_slot[i].state == ST_READY) && (vfq_slot[i].seq > newest_seq))
    {
      newest_seq = vfq_slot[i].seq;
    }
  }

  for (uint32_t i = 0U; i < VFQ_SLOT_NUM; i++)
  {
    vfq_frame_t *f = &vfq_slot[i];
    if (f->state == ST_FREE)
    {
      pick = f;
      break;
    }
    /* 빌려 간 소비자가 없고 가장 최근 프레임도 아닌 것 중 가장 오래된 것 */
    if ((f->state == ST_READY) && (f->refcnt == 0U) && (f->seq != newest_seq))
    {
      if ((oldest == NULL) || (f->seq < oldest->seq))
      {
        oldest = f;
      }
    }
  }

  if ((pick == NULL) && (oldest != NULL))
  {
    pick = oldest;
    vfq_drop_count++;
  }

  if (pick != NULL)
  {
    pick->state = ST_WRITING;
    pick->size  = 0U;
  }

  vfq_unlock(key);
  return pick;
}

void vfq_producer_commit(vfq_frame_t *f)
{
  uint32_t key = vfq_lock();
  f->seq   = vfq_next_seq++;
  f->state = ST_READY;
  vfq_unlock(key);
}

void vfq_producer_abort(vfq_frame_t *f)
{
  uint32_t key = vfq_lock();
  f->state = ST_FREE;
  f->size  = 0U;
  vfq_unlock(key);
}

vfq_frame_t *vfq_acquire_latest(uint32_t last_seq)
{
  vfq_frame_t *best = NULL;
  uint32_t key = vfq_lock();

  for (uint32_t i = 0U; i < VFQ_SLOT_NUM; i++)
  {
    vfq_frame_t *f = &vfq_slot[i];
    if ((f->state == ST_READY) && (f->seq > last_seq))
    {
      if ((best == NULL) || (f->seq > best->seq))
      {
        best = f;
      }
    }
  }
  if (best != NULL)
  {
    best->refcnt++;
  }

  vfq_unlock(key);
  return best;
}

void vfq_release(vfq_frame_t *f)
{
  if (f == NULL)
  {
    return;
  }
  uint32_t key = vfq_lock();
  if (f->refcnt > 0U)
  {
    f->refcnt--;
  }
  vfq_unlock(key);
}

uint32_t vfq_dropped(void)
{
  return vfq_drop_count;
}
