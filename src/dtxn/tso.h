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
class TSO {
public:
 static timestamp_t get_ts() { return rdtsc(); }

private:
};


// class TSO {
//     public:
//         static timestamp_t get_ts() {
//             static int fd = -1;
//             static bool ptp_available = true; // 标记PTP是否可用
            
//             if (ptp_available && fd < 0) {
//                 fd = open("/dev/ptp0", O_RDWR);
//                 if (fd < 0) {
//                     perror("Failed to open /dev/ptp0");
//                     ptp_available = false;
//                 }
//             }
            
//             if (ptp_available) {
//                 struct ptp_clock_time ptp_time = {0};
//                 if (ioctl(fd, PTP_CLOCK_GETTIME, &ptp_time) == 0) {
//                     return static_cast<timestamp_t>(ptp_time.sec) * 1000000000 + ptp_time.nsec;
//                 }
//                 perror("PTP ioctl failed");
//                 ptp_available = false; // 禁用后续PTP尝试
//                 close(fd);
//                 fd = -1;
//             }
            
//             // Fallback to monotonic clock
//             struct timespec ts;
//             clock_gettime(CLOCK_MONOTONIC, &ts);
//             return static_cast<timestamp_t>(ts.tv_sec) * 1000000000 + ts.tv_nsec;
//         }
//     };

// #include <iostream>
// #include <cstdio>
// #include <cstring>
// #include <cstdlib>
// #include <unistd.h>
// #include <sys/types.h>
// #include <sys/wait.h>
// #include <ctime>

// class TSO {
// public:
//     static timestamp_t get_ts() {
//         static bool ptp_available = true; // 标记PTP是否可用

//         if (ptp_available) {
//             // 使用 pmc 命令获取 PTP 时间
//             FILE *fp = popen("pmc -u -b 0 \"GET TIME_STATUS_NP\"", "r");
//             if (!fp) {
//                 perror("Failed to run pmc");
//                 ptp_available = false;
//                 return fallback_to_monotonic();
//             }

//             char buffer[128];
//             while (fgets(buffer, sizeof(buffer), fp) != NULL) {
//                 // 解析输出以获取秒和纳秒
//                 if (strstr(buffer, "time")) {
//                     // 假设输出格式为 "time: <sec>.<nsec>"
//                     long sec, nsec;
//                     if (sscanf(buffer, "time: %ld.%ld", &sec, &nsec) == 2) {
//                         pclose(fp);
//                         return static_cast<timestamp_t>(sec) * 1000000000 + nsec;
//                     }
//                 }
//             }

//             pclose(fp);
//             ptp_available = false; // 禁用后续PTP尝试
//         }

//         // Fallback to monotonic clock
//         return fallback_to_monotonic();
//     }

// private:
//     static timestamp_t fallback_to_monotonic() {
//         struct timespec ts;
//         clock_gettime(CLOCK_MONOTONIC, &ts);
//         return static_cast<timestamp_t>(ts.tv_sec) * 1000000000 + ts.tv_nsec;
//     }
// };