/**
 * status_bar.c — X-Track 风格状态栏(LVGL 顶层常驻)
 * 移植自 X-Track StatusBar(2026-09-01)
 *
 * 挂 lv_layer_top() 常驻于所有页面之上,高 30px:
 *   左:LV_SYMBOL_REFRESH 装饰图标(CLR_ACCENT)
 *   中:时钟 "%02d:%02d"(CLR_TXT,20px)
 *   右:CPU 占用 "LVGL %d%%"(CLR_TXT_DIM,14px)
 * 页面通过 status_bar_set_visible() 控制显示/隐藏(默认显示)。
 * 时钟与 CPU 占用共用 1 个 1s 定时器刷新。
 */
#include "status_bar.h"

#include "ui_palette.h"
#include "data_source.h"

#include <stdio.h>
#include <time.h>

/* 静态句柄:供 1s 定时器 / 显示切换使用 */
static lv_obj_t * s_bar     = NULL;
static lv_obj_t * s_clock   = NULL;
static lv_obj_t * s_cpu_lab = NULL;

/* 时钟 + CPU 占用,同一刷新逻辑 */
static void refresh_status_bar(void)
{
    char tmp[16];

    /* 时钟:time.h localtime */
    time_t now = time(NULL);
    struct tm * tmv = localtime(&now);
    snprintf(tmp, sizeof(tmp), "%02d:%02d", tmv->tm_hour, tmv->tm_min);
    lv_label_set_text(s_clock, tmp);

    /* CPU 占用:四舍五入取整(与 app_menu.c 一致) */
    cpu_info_t d;
    data_source_get(&d);
    snprintf(tmp, sizeof(tmp), "LVGL %d%%", (int)(d.cpu_usage + 0.5f));
    lv_label_set_text(s_cpu_lab, tmp);
}

static void on_one_sec_timer(lv_timer_t * t)
{
    (void)t;
    refresh_status_bar();
}

void status_bar_create(void)
{
    if (s_bar != NULL)
        return;   /* 幂等:重复调用直接返回 */

    /* 顶层常驻条:480x30,CLR_BAR,不可点,不可滚 */
    s_bar = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_bar);          /* 铁律:先 remove_style_all 再定位 */
    lv_obj_set_pos(s_bar, 0, 0);
    lv_obj_set_size(s_bar, 480, 30);
    lv_obj_set_style_bg_color(s_bar, CLR_BAR, 0);
    lv_obj_set_style_bg_opa(s_bar, LV_OPA_COVER, 0);
    lv_obj_set_clickable(s_bar, false);
    lv_obj_set_scrollable(s_bar, false);

    /* 左:装饰图标(CLR_ACCENT,垂直居中) */
    lv_obj_t * icon = lv_label_create(s_bar);
    lv_label_set_text(icon, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(icon, CLR_ACCENT, 0);
    lv_obj_align(icon, LV_ALIGN_LEFT_MID, 16, 0);

    /* 中:时钟(20px CLR_TXT,顶部居中) */
    s_clock = lv_label_create(s_bar);
    lv_label_set_text(s_clock, "--:--");
    lv_obj_set_style_text_font(s_clock, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_clock, CLR_TXT, 0);
    lv_obj_align(s_clock, LV_ALIGN_TOP_MID, 0, 4);

    /* 右:CPU 占用(14px CLR_TXT_DIM,顶部右对齐) */
    s_cpu_lab = lv_label_create(s_bar);
    lv_label_set_text(s_cpu_lab, "LVGL --%");
    lv_obj_set_style_text_font(s_cpu_lab, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_cpu_lab, CLR_TXT_DIM, 0);
    lv_obj_align(s_cpu_lab, LV_ALIGN_TOP_RIGHT, 0, 4);

    /* 1s 刷新:时钟与 CPU 占用同一 timer(lv_timer_create 默认周期重复) */
    lv_timer_create(on_one_sec_timer, 1000, NULL);
    refresh_status_bar();   /* 建即刷,避免首秒空白 */
}

void status_bar_set_visible(bool vis)
{
    if (s_bar != NULL)
        lv_obj_set_hidden(s_bar, !vis);
}
