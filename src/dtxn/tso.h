#pragma once
#include "common.h"
#include "util/timer.h"
#include <sys/ioctl.h>
#include <linux/ptp_clock.h>  // PTP硬件时钟支持
#include <fcntl.h>            // 文件控制
#include <unistd.h>           // close()

// PTP时钟支持
#ifndef PTP_CLOCK_GETTIME
#define PTP_CLOCK_GETTIME _IOR('P', 0, struct ptp_clock_time)
#endif

// 还没实现TSO，这里用rdtsc代替
//class TSO {
// public:
//  static timestamp_t get_ts() { return rdtsc(); }

//private:
//};


class TSO {
    public:
        static timestamp_t get_ts() {
            static int fd = -1;
            if (fd < 0) {
                fd = open("/dev/ptp0", O_RDWR);
            }
            
            if (fd >= 0) {
                struct ptp_clock_time ptp_time;
                if (ioctl(fd, PTP_CLOCK_GETTIME, &ptp_time) == 0) {
                    return static_cast<timestamp_t>(ptp_time.sec)*1000000000 + ptp_time.nsec;
                }
            }
            
            // 回退到系统时钟
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            return static_cast<timestamp_t>(ts.tv_sec)*1000000000 + ts.tv_nsec;
        }
    };