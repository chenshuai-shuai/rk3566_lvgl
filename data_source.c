/* ===== 数据源:同一接口,双平台实现 =====
 *
 * Windows sim : 模拟数据(正弦 + 噪声),便于开发时看动态效果
 * 板子(Linux): /proc/stat CPU 占用率(两次采样差值)
 *              /sys/class/thermal/thermal_zone0/temp SoC 温度
 *              /sys/.../cpufreq/scaling_cur_freq 实时主频
 *
 * 功耗为估算值(EST):P = P_idle + k * usage * (f/f_max)
 * 系数取典型值(非实测),后续可外接 INA219 电流计校准。
 */
#include "data_source.h"
#include <stdio.h>

#if defined(_WIN32) || defined(_MSC_VER)

/* ===================== sim:Windows 本机真实 CPU 数据 =====================
 * 占用率: GetSystemTimes() 两次采样差值(内核 API,与 /proc/stat 算法同构)
 *         注意:kernel 时间【包含】idle 时间(MSDN 文档),busy = total - idle
 * 温度:   Windows 无公开 CPU 温度 API(WMI 台式机为空,探测确认) → 估算(EST)
 * 功耗:   RAPL 在 MSR 中,需驱动 → 估算(EST),与板子口径一致
 */
#include <windows.h>

static ULONGLONG prev_idle   = 0;
static ULONGLONG prev_kernel = 0;
static ULONGLONG prev_user   = 0;
static float     smooth_temp = 0.0f;

static ULONGLONG ft_to_u64(const FILETIME * t)
{
    return ((ULONGLONG)t->dwHighDateTime << 32) | t->dwLowDateTime;
}

void data_source_init(void)
{
    FILETIME idle, kernel, user;
    GetSystemTimes(&idle, &kernel, &user);
    prev_idle   = ft_to_u64(&idle);
    prev_kernel = ft_to_u64(&kernel);
    prev_user   = ft_to_u64(&user);
    smooth_temp = 45.0f;
}

void data_source_get(cpu_info_t * out)
{
    FILETIME idle, kernel, user;
    GetSystemTimes(&idle, &kernel, &user);

    ULONGLONG u_idle   = ft_to_u64(&idle);
    ULONGLONG u_kernel = ft_to_u64(&kernel);
    ULONGLONG u_user   = ft_to_u64(&user);

    /* 两次采样差值:总时间 = kernel+user(kernel 内含 idle),busy = total - idle */
    ULONGLONG d_total = (u_kernel + u_user) - (prev_kernel + prev_user);
    ULONGLONG d_idle  = u_idle - prev_idle;

    float usage = 0.0f;
    if (d_total > 0) {
        usage = 100.0f * (1.0f - (float)d_idle / (float)d_total);
        if (usage < 0)   usage = 0;
        if (usage > 100) usage = 100;
    }
    prev_idle   = u_idle;
    prev_kernel = u_kernel;
    prev_user   = u_user;

    /* 温度估算(EST),一阶平滑防抖:temp_n = 0.7*temp_n-1 + 0.3*temp_new */
    float temp_raw = 38.0f + usage * 0.25f;
    smooth_temp = 0.7f * smooth_temp + 0.3f * temp_raw;

    /* 功耗估算(EST):与板子公式同构(频率项用 1.0 归一,Windows 无实时频率 API) */
    float power = 0.35f + usage * 0.012f;

    out->cpu_usage = usage;
    out->cpu_temp  = smooth_temp;
    out->cpu_power = power;
    out->cpu_freq  = 1400.0f;
}

#else

/* ===================== board:真实数据 ===================== */
#include <stdint.h>
#include <inttypes.h>

static uint64_t prev_total = 0, prev_idle = 0;

/* 读 /proc/stat 第一行 "cpu user nice system idle iowait irq softirq steal ..." */
static int read_cpu_stat(uint64_t * total, uint64_t * idle)
{
    FILE * f = fopen("/proc/stat", "r");
    if (!f) return -1;

    char line[256];
    if (!fgets(line, sizeof(line), f)) { fclose(f); return -1; }
    fclose(f);

    uint64_t user, nice, sys, id, iowait, irq, soft, steal;
    int n = sscanf(line, "cpu %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64
                   " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64,
                   &user, &nice, &sys, &id, &iowait, &irq, &soft, &steal);
    if (n < 4) return -1;

    *idle  = id + iowait;                     /* 闲置 = idle + iowait */
    *total = user + nice + sys + id + iowait + irq + soft + steal;
    return 0;
}

/* 读 sysfs 里单个数值文件(温度/频率等,均为整数微秒/千赫兹级别) */
static float read_sysfs_double(const char * path)
{
    FILE * f = fopen(path, "r");
    if (!f) return -1.0f;
    double v;
    if (fscanf(f, "%lf", &v) != 1) v = -1.0;
    fclose(f);
    return (float)v;
}

void data_source_init(void)
{
    /* 预热:第一次采样只记录基线,get() 用差值计算占用率 */
    read_cpu_stat(&prev_total, &prev_idle);
}

void data_source_get(cpu_info_t * out)
{
    uint64_t total, idle;
    if (read_cpu_stat(&total, &idle) != 0) {
        out->cpu_usage = 0; out->cpu_temp = 0;
        out->cpu_power = 0; out->cpu_freq = 0;
        return;
    }

    /* 占用率 = 1 - (idle差值 / total差值):两次采样的经典算法 */
    float usage = 0.0f;
    if (total > prev_total) {
        uint64_t d_total = total - prev_total;
        uint64_t d_idle  = idle  - prev_idle;
        usage = 100.0f * (1.0f - (float)d_idle / (float)d_total);
        if (usage < 0) usage = 0;
        if (usage > 100) usage = 100;
    }
    prev_total = total;
    prev_idle  = idle;

    float temp     = read_sysfs_double("/sys/class/thermal/thermal_zone0/temp");
    if (temp > 0) temp /= 1000.0f;                       /* 微度 -> 摄氏度 */
    float freq_mhz = read_sysfs_double("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
    if (freq_mhz > 0) freq_mhz /= 1000.0f;               /* kHz -> MHz */

    /* 功耗估算(EST):系数为典型值,非精确 —— UI 标 "EST" */
    float power = 0.35f + usage * 0.011f * (freq_mhz / 1400.0f);

    out->cpu_usage = usage;
    out->cpu_temp  = temp > 0 ? temp : 0;
    out->cpu_power = power;
    out->cpu_freq  = freq_mhz;
}

#endif /* _WIN32 */
