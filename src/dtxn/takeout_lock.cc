#include "takeout_lock.h"

#include <algorithm>

#include "common.h"
#include "coroutine_pool/scheduler.h"
#include "proto/rdma.h"
#include "rrpc/rrpc.h"
#include "util/logging.h"
#include "tso.h"

DynamicWatermark g_watermark;
// 包含上下文数据的结构体
struct CallbackContext {
  void* original_ctx;
  uint64_t start_time;

  
  CallbackContext(void* ctx, uint64_t time) 
  : original_ctx(ctx), start_time(time) {}
};

//"票号+时间戳"混合排队机制
LockReply TakeoutLock::Lock(timestamp_t ts, Mode mode) {
  timestamp_t old_queued_ts;
  TakeoutLock tmp;
  //记录开始时间
  const uint64_t start_time = TSO::get_ts();
  //获取当前动态水位值
  const uint32_t current_cold = g_watermark.get_cold();
  const uint32_t current_hot = g_watermark.get_hot();
 //一次计算即可
  auto CurrentQueuedTxnNum = queued_num();
  do {
     // 每次循环重新计算当前排队数: lower - upper
    //auto CurrentQueuedTxnNum = queued_num();
    // try to take a ticket
    old_queued_ts = queued_ts;
    // 时间戳检查 新事务的时间戳必须 ≥ 队列中最早事务的时间戳, 防止时间戳较小的事务插队
    //冷热模式检查
    if ((ts < old_queued_ts) || (mode == Mode::COLD && CurrentQueuedTxnNum > kColdWatermark) ||
        (CurrentQueuedTxnNum > kHotWatermark)) {
      return {false, *this, reinterpret_cast<uint64_t>(this)};
    }
    tmp = *this;
    //原子排队操作
  } while (!__atomic_compare_exchange_n(&queued_ts, &old_queued_ts, ts, true, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE));
  // update lock state，即为票号加1操作
  __atomic_fetch_add(&lower, 1, __ATOMIC_RELAXED);
 //记录等待时间用于动态调整
 if(mode == Mode::COLD){
    g_watermark.update(TSO::get_ts()-start_time);
 }

  return {true, tmp, reinterpret_cast<uint64_t>(this)};
}


static void poll_lock_wrapper(void* ctx, uint64_t start_time) {
  g_watermark.update(TSO::get_ts()-start_time);
  poll_lock_cb(ctx);
}


//锁状态轮询
void TakeoutLockProxy::poll_lock() {
  const uint64_t start_time = TSO::get_ts();

  auto rkt = GetRocket(0);
  auto *ctx = new PollLockCtx{getShared()};
  auto cb_ctx = new CallbackContext(ctx, start_time);
  auto rc =
            rkt->remote_read(&tl.upper, sizeof(uint64_t), 
            lock_addr + OFFSET(TakeoutLock, upper), 
            rkey,
            [](void* ctx) {
              auto cb = static_cast<CallbackContext*>(ctx);
              poll_lock_wrapper(cb->original_ctx, cb->start_time);
              delete cb;
        },
           cb_ ctx);
  ENSURE(rc == RDMA_CM_ERROR_CODE::CM_SUCCESS, "RDMA read failed, %d", (int)rc);
};

void TakeoutLockProxy::unlock() {
  auto rkt = GetRocket(0);
  // RDMA Fatch And Add
  auto *ctx = new UnlockCtx{this_coroutine::current()};
  auto rc =
      rkt->remote_fetch_add(nullptr, sizeof(uint64_t), lock_addr + OFFSET(TakeoutLock, upper), rkey, 1, unlock_cb, ctx);
  ENSURE(rc == RDMA_CM_ERROR_CODE::CM_SUCCESS, "RDMA fetch add failed, %d", (int)rc);
};

