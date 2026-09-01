#ifndef DATA_SOURCE_H
#define DATA_SOURCE_H

#include <stdint.h>

/* 数据源接口:UI 只认这个结构,不关心数据来自 sim 还是板子 */
typedef struct {
    float    cpu_usage;   /* CPU 占用率 0-100 % */
    float    cpu_temp;    /* SoC 温度 °C */
    float    cpu_power;   /* 整机 CPU 功耗 W(估算值,UI 标注 EST) */
    float    cpu_freq;    /* 当前主频 MHz(估算模型用) */
    float    mem_usage;   /* 内存使用率 0-100 % */
    float    load_avg;    /* 1 分钟平均负载(1 位小数) */
    uint32_t uptime_sec;  /* 系统运行时长,秒 */
} cpu_info_t;

void data_source_init(void);
void data_source_get(cpu_info_t * out);

#endif /* DATA_SOURCE_H */
