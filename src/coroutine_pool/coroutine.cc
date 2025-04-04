#include "coroutine.h"

#include "coroutine_pool.h"
#include "coroutine_pool/scheduler.h"
#include "util/common.h"
#include "util/logging.h"

//无限循环，执行流程：设置为可运行状态，执行当前任务，设置为空闲状态，让出执行权(yield)
void Coroutine::routine(coro_yield_t &main_context) {
  yield_ = &main_context;
  while (true) {
    state_ = CoroutineState::RUNNABLE;
    // do some work
    task_();
    // exit
    state_ = CoroutineState::IDLE;
    (*yield_)();
  }
}

//将控制权返回给调度器
void Coroutine::yield() { (*yield_)(); }

void Coroutine::co_wait(int events) {
  if (events == 0) return;
  //fetch_add原子地增加等待计数：fetch_add返回增加前的旧值
  if (waiting_events_.fetch_add(events) + events > 0) {
    state_ = CoroutineState::WAITING;
    yield();
  }
}

extern thread_local Scheduler *scheduler;
//唤醒机制，和等待机制配合，原子性确保线程安全，适用于高并发的协程调度
void Coroutine::wakeup_once() {
  // auto now_waiting_num = --waiting_events_;
  // if(now_waiting_num < 0 && state_ == CoroutineState::WAITING){
  //   LOG_DEBUG("should not reach here");
  // }
  if ((--waiting_events_) == 0) {
    LOG_ASSERT(scheduler != nullptr, "invalid scheduler");
    sched_->addWakupCoroutine(this);
  }
}
