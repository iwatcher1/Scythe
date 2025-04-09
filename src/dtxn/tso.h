#pragma once
#include "common.h"
#include "util/timer.h"
#include <sys/ioctl.h>
#include <linux/ptp_clock.h>  // PTP硬件时钟支持
#include <fcntl.h>            // 文件控制
#include <unistd.h>           // close()


// 还没实现TSO，这里用rdtsc代替
//class TSO {
// public:
//  static timestamp_t get_ts() { return rdtsc(); }

//private:
//};


class TSO {
    public:
     static timestamp_t get_ts() {
       struct timespec ts;
       int fd = open("/dev/ptp0", O_RDWR);  // 打开DPU的PTP设备
       if (fd < 0) {
         perror("Failed to open /dev/ptp0");
         return 0; 
       }
       ioctl(fd, PTP_CLOCK_GETTIME, &ts);   // 获取硬件时间
       close(fd);
       return static_cast<timestamp_t>(ts.tv_sec) * 1000000000 + ts.tv_nsec;  // 转换为纳秒
     }
   };