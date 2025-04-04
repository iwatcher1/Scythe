#include "scheduler.h"

#include <atomic>

#include "coroutine_pool/coroutine.h"
#include "rrpc/rrpc.h"
#include "util/common.h"
#include "util/logging.h"
#include "util/timer.h"

thread_local Scheduler *scheduler = nullptr;

std::atomic_int id_generator{0};

Scheduler::Scheduler(int coroutine_num) : coro_num_(coroutine_num) {
  // init coroutine
  coros_.reserve(coroutine_num);
  for (int i = 0; i < coroutine_num; i++) {
    coros_.emplace_back(new Coroutine(id_generator.fetch_add(1), this));
    idle_list_.push_back(coros_.back().get());
  }
  scheduler = this;
  wakeup_buf_ = new Coroutine *[coroutine_num];
};

Scheduler::~Scheduler() { delete[] wakeup_buf_; }

void Scheduler::scheduling() {
  scheduler = this;
  while (!stop) {
    // 处理定时事件
    event_loop();
    if (unlikely(runnable_list_.empty())) {
    // 无任务时处理网络IO
      PollRockets();
    }
    // 处理被唤醒的协程
    wakeup();
    // 分配新任务给空闲协程
    dispatch();
    if (unlikely(runnable_list_.empty())) {
    // 无任务可执行时的处理
      Issue();
      continue;
    }
    // 执行一个可运行协程
    current_ = runnable_list_.front();
    runnable_list_.pop_front();
    current_->func_();

    // 根据协程状态重新分类
    switch (current_->state_) {
      case CoroutineState::IDLE:
        idle_list_.push_back(current_);
        break;
      case CoroutineState::RUNNABLE:
        runnable_list_.push_back(current_);
        break;
      case CoroutineState::WAITING:
        waiting_list_.push_front(current_);
        current_->iter = waiting_list_.begin();
        break;
    }
    current_ = nullptr;
  }
};

void Scheduler::dispatch() {
  // 从队列批量获取任务(批量处理提高效率)
  //检查本地缓冲区是否耗尽，如果缓冲区为空，从并发队列批量获取新任务
  if (unlikely(task_pos_ == task_cnt_)) {
    task_cnt_ = queue_.try_dequeue_bulk(task_buf_, kTaskBufLen);
    task_pos_ = 0;
  }

  // 将任务分配给空闲协程
  auto iter = idle_list_.begin();
  while (iter != idle_list_.end() && task_pos_ != task_cnt_) {
    auto coro = *iter;
    coro->task_ = std::move(task_buf_[task_pos_]);
    coro->state_ = CoroutineState::RUNNABLE;
    task_pos_++;
    idle_list_.erase(iter++);
    runnable_list_.push_back(coro);
  }
};

void Scheduler::wakeup() {
  // 批量获取待唤醒协程
  auto cnt = wakeup_list_.try_dequeue_bulk(wakeup_buf_, coro_num_);
  for (int i = 0; i < cnt; i++) {
    auto coro = wakeup_buf_[i];
    // 状态校验
    LOG_ASSERT(coro->waiting_events_ == 0, "coro->waiting_events %d != 0", coro->waiting_events_.load());
    LOG_ASSERT(coro->state_ == CoroutineState::WAITING, "state %d", (int)coro->state_);
    LOG_ASSERT(coro->iter != waiting_list_.end(), "invalid iter");
    
    // 从等待列表移到可运行列表
    waiting_list_.erase(coro->iter);
    runnable_list_.push_back(coro);
    coro->iter = waiting_list_.end();
    coro->state_ = CoroutineState::RUNNABLE;
  }
};

void Scheduler::event_loop() {
  auto current_ts = RdtscTimer::instance().us();
  while (!events_.empty() && events_.top()->next_poll_time() <= current_ts) {
    auto ev = events_.top();
    events_.pop();
    ev->poll_lock();
  }
}

namespace this_coroutine {

Coroutine *current() {
  if (likely(scheduler != nullptr)) return scheduler->current_;
  return nullptr;
}

void yield() {
  if (likely(scheduler != nullptr)) current()->yield();
}

void co_wait(int events) {
  if (likely(scheduler != nullptr)) current()->co_wait(events);
}

bool is_coro_env() { return scheduler != nullptr; };

void scheduler_delegate(TLP tlp) { scheduler->events_.push(tlp); };
}  // namespace this_coroutine