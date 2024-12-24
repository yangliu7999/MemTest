// 使用方式：第一个参数为总的内存占用大小，第二个参数为并发访问的线程数量

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <signal.h>


long SIZE;
long NUM_THREADS;
long rand_seq;
#define ACCESS_INTERVAL_NS 10000 // 0.01ms,10μs
#define RUN_TIME_SEC 60 // 5分钟

char *mem;
const char *START_SIGNAL_FILE = "start_signal"; // 同步文件

void *access_memory(void *arg) {
    // 使用微秒部分作为种子
    struct timeval tv;
    
    long thread_id = (long)arg; //线程ID
    long interval = SIZE / NUM_THREADS; // 每个线程访问的内存块大小
    long start_index = thread_id * interval; // 线程的起始访问位置
    // long end_index = (thread_id + 1) * interval; // 线程的结束访问位置
    long hot_region_size = interval / 4;    // 热区大小 (25%的区域)
    long cold_region_size = interval - hot_region_size; // 冷区大小 (75%的区域)

    struct timespec start, end;

    const int MODE_SWITCH_SEC = 5; // 每 5 秒切换访问模式

    time_t end_time = time(NULL) + RUN_TIME_SEC; // 计算结束时间
    time_t next_switch_time = time(NULL) + MODE_SWITCH_SEC;
    int access_mode = 0; // 0,1,2 表示随机访问，3 表示顺序访问
    long seq_offset = 0;

    srand(tv.tv_usec);

    while (time(NULL) < end_time) {
        // 随机访问内存中的某个位置
        long i; 

        if (time(NULL) >= next_switch_time) {
            access_mode++; // 切换访问模式
            access_mode %= (rand_seq + 1);
            next_switch_time += MODE_SWITCH_SEC;
        }
        
        if (access_mode == rand_seq) {
            // 随机访问
            if (rand() % 100 < 80) {
                // 80% 的概率访问热区
                i = start_index + (rand() % hot_region_size);
            } else {
                // 20% 的概率访问冷区
                i = start_index + hot_region_size + (rand() % cold_region_size);
            }
        } else {
            // 顺序访问
            i = start_index + (seq_offset % interval);
            seq_offset++;
            
            if (seq_offset >= interval) {
                seq_offset = 0;
            }
        }

        clock_gettime(CLOCK_MONOTONIC, &start); // 记录开始时间
        char value = mem[i]; // 访问内存
        clock_gettime(CLOCK_MONOTONIC, &end); // 记录结束时间

        // 计算时间差
        long elapsed_ns = (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);

        printf("Time %ld, value %d\n", elapsed_ns, value);

        // 计算剩余时间并调整休眠时间
        if (elapsed_ns < ACCESS_INTERVAL_NS) {
            struct timespec sleep_time;
            sleep_time.tv_sec = 0;
            sleep_time.tv_nsec = ACCESS_INTERVAL_NS - elapsed_ns;
            nanosleep(&sleep_time, NULL);
        }
    }
    return NULL;
}

// 等待同步信号
void wait_for_start_signal() {
    char signal = '0';

    while (signal != '1') {
        int fd = open(START_SIGNAL_FILE, O_RDONLY);
        if (fd != -1) {
            read(fd, &signal, 1);
            close(fd);
        }
        usleep(10000); // 每 10ms 检查一次信号
    }
    printf("Received start signal, starting threads...\n");
}

int main(int argc, char *argv[]) {
    // 申请 SIZE GB内存
    SIZE = atol(argv[1]); // 从命令行参数获取内存大小 GB
    SIZE = SIZE * 1024 * 1024 * 1024;

    NUM_THREADS = atol(argv[2]); // 从命令行参数获取线程数量

    if(NUM_THREADS < 1)
        NUM_THREADS = 1;

// 从命令行参数获取顺序访问比例
    rand_seq = atol(argv[3]); 

    mem = malloc(SIZE);

    if (mem == NULL) {
        perror("malloc");
        return 1;
    }

    // 填充内存
    for (long i = 0; i < SIZE; i++) {
        mem[i] = (char)(i % 256); // 填充数据
    }

    printf("Memory fill complete. Waiting for start signal...\n");

    // 等待开始信号
    wait_for_start_signal();

    pthread_t threads[NUM_THREADS];

    // 创建线程
    for (long i = 0; i < NUM_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, access_memory, (void *)i) != 0) {
            perror("pthread_create");
            free(mem);
            return 1;
        }
    }

    // 等待线程结束
    for (long i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // 释放内存
    free(mem);

    return 0;
}
