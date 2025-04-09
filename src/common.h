#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <atomic>

using timestamp_t = uint64_t;
using data_t = char;

#define KB(x) ((x)*1024ul)
#define MB(x) ((x)*1024 * 1024ul)
#define GB(x) ((x)*1024 * 1024 * 1024ul)
#define OFFSET(TYPE, MEMBER) ((unsigned long)(&(((TYPE *)0)->MEMBER)))

constexpr timestamp_t LATEST = ~0ull;
constexpr uint16_t kColdWatermark = 3;
constexpr uint64_t kHotWatermark = 10;
constexpr size_t kMaxObjectSize = 1024;

// transaction execution mode
enum class Mode : uint8_t { COLD, HOT };

enum class DbStatus : uint8_t {
  OK = 0,
  NOT_EXIST,
  OBSOLETE,
  LOCKED,
  DUPLICATE,
  ALLOC_FAILED,
  UNEXPECTED_ERROR,
};

static inline std::string Status2Str(DbStatus s) {
  switch (s) {
    case DbStatus::OK:
      return "OK";
    case DbStatus::NOT_EXIST:
      return "NOT_EXIST";
    case DbStatus::LOCKED:
      return "LOCKED";
    case DbStatus::OBSOLETE:
      return "OBSOLETE";
    case DbStatus::ALLOC_FAILED:
      return "ALLOC_FAILED";
    case DbStatus::UNEXPECTED_ERROR:
      return "UNEXPECTED_ERROR";
    case DbStatus::DUPLICATE:
      return "DUPLICATE";
  }
  return "<Unkonwn>";
}

constexpr uint64_t RTT = 6;  // 偷个懒，先固定RTT为6us，实际上需要实时计算滑动平均值

// 动态水位控制器
class DynamicWatermark {
  private:
      std::atomic<uint32_t> cold_watermark_;
      std::atomic<uint32_t> hot_watermark_;
      std::atomic<uint64_t> total_wait_time_;
      std::atomic<uint32_t> sample_count_;
      
  public:
      DynamicWatermark(uint32_t initial_cold = 3, uint32_t initial_hot = 10) 
          : cold_watermark_(initial_cold), 
            hot_watermark_(initial_hot),
            total_wait_time_(0),
            sample_count_(0) {}
      
      void update(uint64_t wait_time) {
          total_wait_time_.fetch_add(wait_time, std::memory_order_relaxed);
          uint32_t count = sample_count_.fetch_add(1, std::memory_order_relaxed) + 1;
          
          // 每100个样本更新一次水位
          if (count % 100 == 0) {
              uint64_t avg_wait = total_wait_time_.load(std::memory_order_relaxed) / count;
              
              // 动态调整公式 
              uint32_t new_cold = 3 + avg_wait / 1000;  // 每1us平均等待增加0.001
              uint32_t new_hot = 10 + avg_wait / 500;   // 每1us平均等待增加0.002
              
              cold_watermark_.store(std::min(new_cold, 50U), std::memory_order_relaxed);
              hot_watermark_.store(std::min(new_hot, 200U), std::memory_order_relaxed);
              
              // 重置统计
              total_wait_time_.store(0, std::memory_order_relaxed);
              sample_count_.store(0, std::memory_order_relaxed);
          }
      }
      
      uint32_t get_cold() const { return cold_watermark_.load(std::memory_order_relaxed); }
      uint32_t get_hot() const { return hot_watermark_.load(std::memory_order_relaxed); }
  };
  

// 全局动态水位实例
extern DynamicWatermark g_watermark;