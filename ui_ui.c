/* ===== CPU 监视页面(480x800,深色主题) =====
 *
 * 布局(自上而下,坐标固定 480x800 屏):
 *   状态栏   0-52     标题 + 实时时钟
 *   环形仪表 70-450   CPU 占用率 lv_arc(380x380),中心大数字
 *   滚动曲线 470-640  最近 60s 占用率 lv_chart(1s 一点)
 *   卡片     660-770  温度 / 功耗(EST)
 *
 * 数据:1s 定时器调 data_source_get()(sim=模拟,board=真数据)
 */
#include "lvgl/lvgl.h"
#include "ui_ui.h"
#include "data_source.h"
#include <stdio.h>
#include <time.h>

/* ---- 深色主题调色板(集中定义,改这里全页生效) ---- */
#define CLR_BG       lv_color_hex(0x0E1116)   /* 页面底 */
#define CLR_CARD     lv_color_hex(0x1A2028)   /* 卡片底 */
#define CLR_BAR      lv_color_hex(0x141A22)   /* 状态栏底 */
#define CLR_TXT      lv_color_hex(0xDDE3EA)   /* 主文字 */
#define CLR_TXT_DIM  lv_color_hex(0x8A94A2)   /* 次文字 */
#define CLR_TRACK    lv_color_hex(0x232B36)   /* 环形/图表轨道 */
#define CLR_GREEN    lv_color_hex(0x35C26E)   /* 正常(<60%) */
#define CLR_YELLOW   lv_color_hex(0xF0B429)   /* 注意(60-80%) */
#define CLR_RED      lv_color_hex(0xE5484D)   /* 危险(>80%) */

/* 页面元素句柄(ui_tick 里刷新用) */
static lv_obj_t * g_arc;
static lv_obj_t * g_lab_usage;
static lv_obj_t * g_lab_temp;
static lv_obj_t * g_lab_power;
static lv_obj_t * g_lab_clock;
static lv_obj_t * g_chart;
static lv_chart_series_t * g_ser;

/* 占用率 -> 状态色 */
static lv_color_t usage_color(float u)
{
    if (u < 60.0f) return CLR_GREEN;
    if (u < 80.0f) return CLR_YELLOW;
    return CLR_RED;
}

/* 1s 定时:取数据 -> 刷仪表/曲线/卡片/时钟 */
static void ui_tick(lv_timer_t * timer)
{
    (void)timer;
    cpu_info_t d;
    data_source_get(&d);

    lv_arc_set_value(g_arc, (int32_t)(d.cpu_usage + 0.5f));
    lv_label_set_text_fmt(g_lab_usage, "%d %%", (int)(d.cpu_usage + 0.5f));
    lv_obj_set_style_arc_color(g_arc, usage_color(d.cpu_usage), LV_PART_INDICATOR);

    lv_chart_set_next_value(g_chart, g_ser, (lv_coord_t)d.cpu_usage);

    /* ★浮点格式化坑:LVGL 内置 sprintf 不支持 %f(默认 LV_STDLIB_BUILTIN),
     * lv_label_set_text_fmt 输出空串 → 先用 libc snprintf 再喂 label */
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%0.1f C", d.cpu_temp);
    lv_label_set_text(g_lab_temp, tmp);
    snprintf(tmp, sizeof(tmp), "%0.2f W", d.cpu_power);
    lv_label_set_text(g_lab_power, tmp);

    time_t t = time(NULL);
    struct tm * tm = localtime(&t);
    lv_label_set_text_fmt(g_lab_clock, "%02d:%02d", tm->tm_hour, tm->tm_min);
}

void ui_start(void)
{
    data_source_init();

    /* ===== 屏幕底 ===== */
    lv_obj_t * scr = lv_screen_active();
    lv_obj_remove_style_all(scr);
    lv_obj_set_style_bg_color(scr, CLR_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* ===== 状态栏 ===== */
    lv_obj_t * bar = lv_obj_create(scr);
    lv_obj_remove_style_all(bar);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_size(bar, 480, 52);
    lv_obj_set_style_bg_color(bar, CLR_BAR, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);

    lv_obj_t * title = lv_label_create(bar);
    lv_label_set_text(title, "CPU Monitor");
    lv_obj_set_style_text_color(title, CLR_TXT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(title, 16, 14);

    g_lab_clock = lv_label_create(bar);
    lv_obj_set_style_text_color(g_lab_clock, CLR_TXT, 0);
    lv_obj_set_style_text_font(g_lab_clock, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(g_lab_clock, 380, 14);

    /* ===== 环形仪表(占用率) ===== */
    g_arc = lv_arc_create(scr);
    lv_obj_remove_style_all(g_arc);
    lv_obj_set_pos(g_arc, 50, 70);
    lv_obj_set_size(g_arc, 380, 380);
    lv_arc_set_range(g_arc, 0, 100);          /* 值域=百分比 */
    lv_arc_set_rotation(g_arc, -90);          /* 0% 从正上方开始,顺时针 */
    lv_arc_set_value(g_arc, 0);
    lv_arc_set_bg_angles(g_arc, 10, 350);     /* 轨道留一个小缺口更像仪表 */
    lv_obj_set_style_arc_color(g_arc, CLR_TRACK, LV_PART_MAIN);        /* 轨道 */
    lv_obj_set_style_arc_width(g_arc, 24, LV_PART_MAIN);
    lv_obj_set_style_arc_color(g_arc, CLR_GREEN, LV_PART_INDICATOR);   /* 进度 */
    lv_obj_set_style_arc_width(g_arc, 24, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(g_arc, true, LV_PART_INDICATOR);

    /* 中心:大数字 + 说明(直接放屏幕,叠在 arc 之上) */
    g_lab_usage = lv_label_create(scr);
    lv_label_set_text(g_lab_usage, "-- %");
    lv_obj_set_style_text_color(g_lab_usage, CLR_TXT, 0);
    lv_obj_set_style_text_font(g_lab_usage, &lv_font_montserrat_32, 0);
    lv_obj_align(g_lab_usage, LV_ALIGN_CENTER, 0, -26);
    lv_obj_move_foreground(g_lab_usage);

    lv_obj_t * lab_sub = lv_label_create(scr);
    lv_label_set_text(lab_sub, "CPU USED");
    lv_obj_set_style_text_color(lab_sub, CLR_TXT_DIM, 0);
    lv_obj_set_style_text_font(lab_sub, &lv_font_montserrat_20, 0);
    lv_obj_align(lab_sub, LV_ALIGN_CENTER, 0, 14);
    lv_obj_move_foreground(lab_sub);

    /* ===== 滚动曲线(60s) ===== */
    g_chart = lv_chart_create(scr);
    lv_obj_remove_style_all(g_chart);
    lv_obj_set_pos(g_chart, 20, 470);
    lv_obj_set_size(g_chart, 440, 170);
    lv_obj_set_style_bg_color(g_chart, CLR_CARD, 0);
    lv_obj_set_style_bg_opa(g_chart, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(g_chart, 12, 0);
    lv_obj_set_style_pad_all(g_chart, 12, 0);
    lv_chart_set_type(g_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(g_chart, 60);               /* 60 个点,1s/点 => 最近 1 分钟 */
    lv_chart_set_axis_range(g_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_div_line_count(g_chart, 0, 4);
    lv_obj_set_style_line_color(g_chart, CLR_TRACK, LV_PART_MAIN);       /* 网格线 */
    lv_obj_set_style_line_width(g_chart, 1, LV_PART_MAIN);

    g_ser = lv_chart_add_series(g_chart, CLR_GREEN, LV_CHART_AXIS_PRIMARY_Y);
    lv_obj_set_style_line_width(g_chart, 3, LV_PART_ITEMS);              /* 折线宽 */

    /* ===== 底部卡片:温度 / 功耗 ===== */
    /* 温度卡 */
    lv_obj_t * c_temp = lv_obj_create(scr);
    lv_obj_remove_style_all(c_temp);
    lv_obj_set_pos(c_temp, 20, 660);
    lv_obj_set_size(c_temp, 214, 110);
    lv_obj_set_style_bg_color(c_temp, CLR_CARD, 0);
    lv_obj_set_style_bg_opa(c_temp, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(c_temp, 12, 0);

    g_lab_temp = lv_label_create(c_temp);
    lv_label_set_text(g_lab_temp, "--.- C");
    lv_obj_set_style_text_color(g_lab_temp, CLR_TXT, 0);
    lv_obj_set_style_text_font(g_lab_temp, &lv_font_montserrat_26, 0);
    lv_obj_set_pos(g_lab_temp, 18, 12);

    lv_obj_t * lab_temp_name = lv_label_create(c_temp);
    lv_label_set_text(lab_temp_name, "CPU TEMPERATURE");
    lv_obj_set_style_text_color(lab_temp_name, CLR_TXT_DIM, 0);
    lv_obj_set_style_text_font(lab_temp_name, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lab_temp_name, 18, 62);

    /* 功耗卡 */
    lv_obj_t * c_pwr = lv_obj_create(scr);
    lv_obj_remove_style_all(c_pwr);
    lv_obj_set_pos(c_pwr, 246, 660);
    lv_obj_set_size(c_pwr, 214, 110);
    lv_obj_set_style_bg_color(c_pwr, CLR_CARD, 0);
    lv_obj_set_style_bg_opa(c_pwr, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(c_pwr, 12, 0);

    g_lab_power = lv_label_create(c_pwr);
    lv_label_set_text(g_lab_power, "--.-- W");
    lv_obj_set_style_text_color(g_lab_power, CLR_TXT, 0);
    lv_obj_set_style_text_font(g_lab_power, &lv_font_montserrat_26, 0);
    lv_obj_set_pos(g_lab_power, 18, 12);

    lv_obj_t * lab_pwr_name = lv_label_create(c_pwr);
    lv_label_set_text(lab_pwr_name, "POWER (EST.)");
    lv_obj_set_style_text_color(lab_pwr_name, CLR_TXT_DIM, 0);
    lv_obj_set_style_text_font(lab_pwr_name, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lab_pwr_name, 18, 62);

    /* ===== 数据刷新定时器(1s) ===== */
    lv_timer_create(ui_tick, 1000, NULL);
}
