#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <errno.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include "lvgl/lvgl.h"
#include "ui_ui.h"

/* ===== 详细日志:每步打点,LOG 后立即强制刷出(adb 下 stdout 可能是全缓冲) ===== */
#define LOG(fmt, ...) do { \
    printf("[lvgl] " fmt "\n", ##__VA_ARGS__); \
    fflush(stdout); \
} while (0)

static int fbfd = 0;
static char *fbp = NULL;
static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;
static int flush_count = 0;

/* LVGL 渲染完成回调:把 LVGL 的像素块拷贝到显存 */
static void fb_flush_cb(lv_display_t *disp, const lv_area_t *area,
                         uint8_t *px_map)
{
    int w = lv_area_get_width(area);
    for (int y = area->y1; y <= area->y2; y++) {
        int offset = (y * vinfo.xres + area->x1) * 4;
        int map_off = ((y - area->y1) * w) * 4;
        memcpy(fbp + offset, px_map + map_off, w * 4);
    }
    lv_display_flush_ready(disp);
    flush_count++;
}

/* ============== LVGL 时间源(v9 必须!否则 timer/动画/输入全冻结) ============== */
static uint32_t lvgl_tick_cb(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    /* ★ LVGL v9 的 tick 回调必须返回【毫秒 ms】(不是 us!官方 SDL 移植即为 ms) */
    return (uint32_t)(ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL);
}

/* click 事件审计回调:直接挂在 indev 上(LVGL 每次 CLICKED 必经过 indev,
 * 不受对象冒泡规则限制),日志证明 click 真实产生并显示命中目标 */
static void indev_click_audit(lv_event_t * e)
{
lv_obj_t * t = lv_event_get_param(e);   /* indev 事件参数 = 命中的目标对象 */
LOG("[EVENT] CLICKED obj=%p", (void *)t);
}

/* ================= USB 鼠标输入(LVGL indev) =================
 * /dev/input/event3 = 鼠标(内核已枚举)
 * 鼠标是相对坐标(REL_X/REL_Y):自己累积;左键 = KEY 事件
 */
static int evdev_fd = -1;
static int cur_x = 240, cur_y = 400;   /* 指针位置(从屏幕中心起步) */
static bool btn_pressed = false;
/* 事件锁存:快速点击时按下+抬起可能被同一次 read_cb 合并,
 * 必须把"按下"先报告给 LVGL,下一 tick 再报告"抬起",否则 LVGL 永远看不到 click */
static bool btn_pressed_pending = false;
static bool btn_released_pending = false;
static int mouse_reads = 0;   /* read_cb 被 LVGL 调用的次数 */
static int mouse_events = 0; /* 从 event3 读到的事件条数 */
static int last_x = -1, last_y = -1;   /* 指针上次位置(防重复重绘) */

/* ========== 动态查找鼠标设备(不再硬编码 event3,编号每次开机可能漂移) ========== */
static int find_mouse_dev(void)
{
    char path[64], name[128];

    for (int i = 0; i < 32; i++) {
        snprintf(path, sizeof(path), "/sys/class/input/event%d/device/name", i);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        if (fgets(name, sizeof(name), f)) {
            name[strcspn(name, "\r\n")] = 0;   /* 去掉换行 */
            if (strstr(name, "Mouse") || strstr(name, "mouse") || strstr(name, "MOUSE")) {
                fclose(f);
                LOG("  找到鼠标: /dev/input/event%d = %s", i, name);
                return i;
            }
        }
        fclose(f);
    }
    return -1;
}

static void mouse_read_cb(lv_indev_t * indev, lv_indev_data_t * data)
{
    struct input_event ev;
    /* 清空 LVGL 输入数据结构(v9.6 无 lv_indev_data_init,用标准 memset) */
    memset(data, 0, sizeof(lv_indev_data_t));

    mouse_reads++;

    /* 非阻塞:一次把缓冲里的事件全部读完 */
    ssize_t n;
    while ((n = read(evdev_fd, &ev, sizeof(ev))) == (ssize_t)sizeof(ev)) {
        mouse_events++;
        if (ev.type == EV_REL) {
            if (ev.code == REL_X)      cur_x += ev.value;
            else if (ev.code == REL_Y) cur_y += ev.value;
        }
        else if (ev.type == EV_KEY) {
            /* 左键/右键/中键 都当作"按压"(BTN_LEFT=0x110, RIGHT=0x111, MIDDLE=0x112)
             * 边沿检测:只在状态变化时置"待报告"标志(锁存,不丢失快速点击) */
            if (ev.code == BTN_LEFT || ev.code == BTN_RIGHT || ev.code == BTN_MIDDLE) {
                if (ev.value == 1) {
                    if (!btn_pressed) btn_pressed_pending = true;  /* 上升沿:按下待报告 */
                    btn_pressed = true;
                } else if (ev.value == 0) {
                    if (btn_pressed) btn_released_pending = true;  /* 下降沿:抬起待报告 */
                    btn_pressed = false;
                }
            }
        }
    }

    /* 指针范围约束在屏幕内 */
    if (cur_x < 0) cur_x = 0;
    if (cur_x >= 480) cur_x = 479;
    if (cur_y < 0) cur_y = 0;
    if (cur_y >= 800) cur_y = 799;

    data->point.x = cur_x;
    data->point.y = cur_y;

    /* 锁存报告(关键):优先报告"按下";抬起推迟到下一次回调。
     * 这样即使按下+抬起被同一次回调合并,LVGL 仍按顺序看到 press → release,
     * click 事件必然产生(一次点击 = 一次 click,不再随机丢)。 */
    if (btn_pressed_pending) {
        data->state = LV_INDEV_STATE_PRESSED;
        btn_pressed_pending = false;
        LOG("[PRESSED] pos=(%d,%d)", cur_x, cur_y);
    } else if (btn_released_pending) {
        data->state = LV_INDEV_STATE_RELEASED;
        btn_released_pending = false;
        LOG("[RELEASED] pos=(%d,%d)", cur_x, cur_y);
    } else {
        data->state = btn_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    }
}

int main(void)
{
    LOG("==== lv_demo 启动 ====");

    LOG("step 1/8: open /dev/fb0");
    fbfd = open("/dev/fb0", O_RDWR);
    if (fbfd < 0) { perror("[lvgl] open fb0"); return 1; }

    LOG("step 2/8: ioctl 读取屏参");
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo)) { perror("[lvgl] vscreeninfo"); return 1; }
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo)) { perror("[lvgl] fscreeninfo"); return 1; }
    LOG("  fb0 = %dx%d bpp=%d line_length=%d smem_len=%d",
        vinfo.xres, vinfo.yres, vinfo.bits_per_pixel,
        finfo.line_length, finfo.smem_len);

    LOG("step 3/8: mmap 显存");
    fbp = mmap(0, finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if (fbp == MAP_FAILED) { perror("[lvgl] mmap"); return 1; }
    LOG("  mmap OK @ %p (%d bytes)", fbp, finfo.smem_len);

    LOG("step 4/8: 清屏(抹掉 fbcon 开机日志残影)");
    memset(fbp, 0, finfo.smem_len);

    LOG("step 5/8: 抑制内核日志刷屏 (printk=0 4 1 7)");
    FILE *pfp = fopen("/proc/sys/kernel/printk", "w");
    if (pfp) {
        fprintf(pfp, "0 4 1 7\n");
        fclose(pfp);
        LOG("  printk 已抑制");
    } else {
        perror("[lvgl] fopen printk");
    }

    LOG("step 6/8: lv_init() + 时间源 tick");
    lv_init();
    lv_tick_set_cb(lvgl_tick_cb);   /* 关键:不设 tick,LVGL 所有 timer 冻结 */
    LOG("  tick 已配置 (lv_tick_set_cb)");

    LOG("step 7/8: lv_display_create(%dx%d) + XRGB8888 + 部分刷新 480x50",
        vinfo.xres, vinfo.yres);
    lv_display_t *disp = lv_display_create(vinfo.xres, vinfo.yres);
    if (!disp) { LOG("FATAL: lv_display_create 失败"); return 1; }
    lv_display_set_flush_cb(disp, fb_flush_cb);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_XRGB8888);
    static lv_color_t buf1[480 * 50];
    lv_display_set_buffers(disp, buf1, NULL, sizeof(buf1),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    LOG("step 8/8: 注册 USB 鼠标 indev (动态查找)");
    int mouse_idx = find_mouse_dev();
    if (mouse_idx >= 0) {
        char devpath[64];
        snprintf(devpath, sizeof(devpath), "/dev/input/event%d", mouse_idx);
        evdev_fd = open(devpath, O_RDONLY | O_NONBLOCK);
    } else {
        LOG("  WARN: 未找到鼠标设备(Mouse 关键字扫描失败)");
    }
    if (evdev_fd >= 0) {
        lv_indev_t * indev = lv_indev_create();
        lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(indev, mouse_read_cb);
        /* 放大点击容差:移动 ≤40px 仍判定为 click(默认 10px 太严格,手抖易误判拖拽) */
        lv_indev_set_scroll_limit(indev, 40);
        /* click 事件审计:每次 CLICKED 必经过 indev(不受冒泡限制) */
        lv_indev_add_event_cb(indev, indev_click_audit, LV_EVENT_CLICKED, NULL);
        LOG("  indev OK: 鼠标已接入 LVGL (click 容差 40px)");
    } else {
        perror("[lvgl] open event3");
        LOG("  WARN: 鼠标不可用(无输入)");
    }

    /* step 9/9: 启动我们自己的 UI(sim/板子共用,数据源自动切换) */
    LOG("step 9/9: ui_start() 创建 CPU 监视页面");
    ui_start();
    LOG("  UI 创建完成: 屏幕对象数 children=%d",
        lv_obj_get_child_cnt(lv_screen_active()));

    /* 自绘指针:白色小圆点 + 黑色描边(在 demo 之后创建 + move_foreground 置顶) */
    lv_obj_t * cursor = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(cursor);
    /* ★关键:指针圆点不参与点击命中!(lv_obj_create 默认 CLICKABLE=true,
     * 而 LVGL 命中测试从最顶层开始找可点击对象 → 事件全被圆点吃掉,
     * 下方按钮永远收不到 click。置不可点击后事件穿透到真实控件。)*/
    lv_obj_set_clickable(cursor, false);
    lv_obj_set_size(cursor, 10, 10);
    lv_obj_set_style_bg_color(cursor, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(cursor, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(cursor, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(cursor, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(cursor, 1, 0);
    lv_obj_move_foreground(cursor);   /* 置顶:永远盖在 demo 之上 */
    lv_obj_set_pos(cursor, cur_x - 5, cur_y - 5);

    LOG("进入主循环:lv_timer_handler(), 每 5 秒心跳报告一次");
    int hb = 0;
    while (1) {
        lv_timer_handler();
        /* 指针圆点跟随鼠标:只在位置真变时重绘,省 CPU */
        if (cur_x != last_x || cur_y != last_y) {
            lv_obj_set_pos(cursor, cur_x - 5, cur_y - 5);
            last_x = cur_x; last_y = cur_y;
        }
        usleep(5000);
        if (++hb >= 1000) {   /* 5 秒 */
            hb = 0;
            LOG("heartbeat: flush=%d mouse_reads=%d mouse_events=%d pos=(%d,%d)%s",
                flush_count, mouse_reads, mouse_events, cur_x, cur_y,
                btn_pressed ? " [PRESSED]" : "");
        }
    }
    return 0;
}
