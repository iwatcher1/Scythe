#pragma once

#include <cstdint>

#include "common.h"
#include "coroutine_pool/coroutine.h"

struct LockReply;

struct TakeoutLock {
  //票号机制：采用类似银行叫号系统的设计
//lower: 类似"当前最大号"
//upper: 类似"现在服务到X号"
  uint64_t lower{0};
  uint64_t upper{0};
// 队列中最早事务的时间戳
  timestamp_t queued_ts{0};

  LockReply Lock(timestamp_t ts, Mode mode);
//计算等待事务数
  uint64_t queued_num() const { return lower - upper; }
//计算是否叫到自己
  bool ready() const { return lower == upper; }
};

struct LockReply {
  // 是否成功排队
  bool is_queued{false};
  // 当前锁状态
  TakeoutLock lock{};
  // 远程锁地址
  uint64_t lock_addr{0};
};

class TxnObj;
struct TakeoutLockProxy : public std::enable_shared_from_this<TakeoutLockProxy> {
  struct Compare {
   public:
    bool operator()(const std::shared_ptr<TakeoutLockProxy> &a, const std::shared_ptr<TakeoutLockProxy> &b) {
      return a->us_since_poll + a->hangout > b->us_since_poll + b->hangout;
    }
  };
  void poll_lock();
  void unlock();
  uint64_t next_poll_time() const { return us_since_poll + hangout; }
  std::shared_ptr<TakeoutLockProxy> getShared() { return shared_from_this(); }

  TakeoutLock tl;
  uint64_t us_since_poll;  // 上一次轮询时的时间
  uint64_t hangout;
  uint64_t lock_addr;
  uint32_t rkey;
  
  TxnObj *obj;
  Coroutine *txn_coro;
  TakeoutLockProxy(Coroutine *coro, TxnObj *obj) : txn_coro(coro), obj(obj) {}
};

using TLP = std::shared_ptr<TakeoutLockProxy>;
