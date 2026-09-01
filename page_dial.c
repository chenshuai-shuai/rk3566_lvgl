/**
 * page_dial.c — CPU 监视主页面(X-Track dial 风格,2026-09-01)
 *
 * 启动页(页面名 "dial"),X-Track GPS/Sport 数据页样式,480x800 竖屏:
 *   - 标题 "CPU MONITOR" 20px @ (16,44),避开顶部 30px StatusBar 常驻区
 *   - 中部数据区:左大图标 + 橙色竖线 5x220 + 6 行名称/数值
 *     (LOAD / TEMP / POWER / MEMORY / LOAD AVG / UPTIME,x=160/300,行距 44)
 *   - 底部 3 按钮 130x50 @ y=700(3D / TEST / INFO),按压高 50→44 + 深色 + 200ms 过渡
 *
 * 数据:1s 定时器调 data_source_get();浮点走 libc snprintf
 * (LVGL 内置 sprintf 不支持 %f,见 ui_ui.c 同款注释,勿用 lv_label_set_text_fmt)。
 */
#include "lvgl/lvgl.h"
#include "page_dial.h"
#include "page_nav.h"      /* page_nav_push() */
#include "data_source.h"
/* 主题颜色统一来自 ui_palette.h(全工程唯一颜色定义源) */
#include "ui_palette.h"
#include <stdio.h>

/* ---- 几何(480x800 固定坐标) ---- */
#define DATA_Y0     170     /* 数据行第 1 行 y */
#define DATA_STEP   44      /* 行距 */
#define BTN_X0      34      /* 按钮 0 x */
#define BTN_DX      145     /* 按钮间距 */
#define BTN_Y       700     /* 按钮 y */
#define BTN_W       130
#define BTN_H       50

/* ---- 本页句柄(cache=true,root 隐藏保留,句柄常驻) ---- */
static lv_obj_t   * s_vals[6];   /* 6 行数值 label(1s 刷新) */
static lv_timer_t * s_timer;     /* 1s 刷新定时器,did_appear 建 / will_disappear 删 */

/* ---- 按钮按压反馈样式(仿旧 app_menu:Dialplate 按钮) ---- */
static lv_style_t              s_btn_norm;    /* 常态:高 50,CLR_CARD 底 */
static lv_style_t              s_btn_press;   /* 按压:变扁 44 + 深色 0x2A3038 */
static lv_style_transition_dsc_t s_btn_tr;    /* 过渡 200ms */
static const lv_style_prop_t   s_btn_props[] = { LV_STYLE_HEIGHT, LV_STYLE_BG_COLOR };
static bool                    s_styles_inited = false;

/* 按钮事件:跳转 3D / TEST 页 */
static void on_btn_cube(lv_event_t * e)
{
    (void)e;
    page_nav_push("cube");
}

static void on_btn_test(lv_event_t * e)
{
    (void)e;
    page_nav_push("test");
}

/* 样式一次性初始化(cache 页 on_load 只跑一次,加防呆) */
static void btn_styles_init(void)
{
    if (s_styles_inited) return;
    s_styles_inited = true;

    lv_style_init(&s_btn_norm);
    lv_style_init(&s_btn_press);
    lv_style_transition_dsc_init(&s_btn_tr, s_btn_props, lv_anim_path_ease_out, 200, 0, NULL);

    /* 常态:130x50 圆角 20 CLR_CARD 底 */
    lv_style_set_height(&s_btn_norm, BTN_H);
    lv_style_set_radius(&s_btn_norm, 20);
    lv_style_set_bg_color(&s_btn_norm, CLR_CARD);
    lv_style_set_bg_opa(&s_btn_norm, LV_OPA_COVER);
    lv_style_set_transition(&s_btn_norm, &s_btn_tr);

    /* 按压:高 50→44 变扁,底色 0x2A3038,过渡 200ms */
    lv_style_set_height(&s_btn_press, BTN_H - 6);
    lv_style_set_radius(&s_btn_press, 20);
    lv_style_set_bg_color(&s_btn_press, lv_color_hex(0x2A3038));
    lv_style_set_bg_opa(&s_btn_press, LV_OPA_COVER);
    lv_style_set_transition(&s_btn_press, &s_btn_tr);
}

/* 底部按钮:lv_button_create,remove_style_all 后套上下两态样式 */
static void make_btn(lv_obj_t * root, int i, const char * text,
                     lv_event_cb_t cb, lv_color_t txt_color)
{
    lv_obj_t * btn = lv_button_create(root);
    /* 铁律:remove_style_all 先于 set_pos */
    lv_obj_remove_style_all(btn);
    lv_obj_set_pos(btn, BTN_X0 + i * BTN_DX, BTN_Y);
    lv_obj_set_size(btn, BTN_W, BTN_H);
    lv_obj_add_style(btn, &s_btn_norm, 0);
    lv_obj_add_style(btn, &s_btn_press, LV_STATE_PRESSED);

    lv_obj_t * lab = lv_label_create(btn);
    lv_label_set_text(lab, text);
    lv_obj_set_style_text_font(lab, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lab, txt_color, 0);
    lv_obj_center(lab);

    if (cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
}

/* 1s 定时:取数据 -> libc snprintf -> 刷 6 行数值(格式同 ui_ui.c) */
static void page_dial_refresh(lv_timer_t * t)
{
    (void)t;
    if (s_vals[0] == NULL) return;      /* 防呆:root 未建时直接返回 */

    cpu_info_t d;
    data_source_get(&d);

    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%d %%", (int)(d.cpu_usage + 0.5f));
    lv_label_set_text(s_vals[0], tmp);
    snprintf(tmp, sizeof(tmp), "%0.1f C", d.cpu_temp);
    lv_label_set_text(s_vals[1], tmp);
    snprintf(tmp, sizeof(tmp), "%0.2f W", d.cpu_power);
    lv_label_set_text(s_vals[2], tmp);
    snprintf(tmp, sizeof(tmp), "%d %%", (int)(d.mem_usage + 0.5f));
    lv_label_set_text(s_vals[3], tmp);
    snprintf(tmp, sizeof(tmp), "%0.1f", d.load_avg);
    lv_label_set_text(s_vals[4], tmp);

    /* 运行时长:Xd HH:MM,不足 1 天只显示 HH:MM */
    uint32_t sec  = d.uptime_sec;
    uint32_t days = sec / 86400u;
    uint32_t rem  = sec % 86400u;
    if (days > 0)
        snprintf(tmp, sizeof(tmp), "%ud %02u:%02u", days, rem / 3600u, (rem % 3600u) / 60u);
    else
        snprintf(tmp, sizeof(tmp), "%02u:%02u", rem / 3600u, (rem % 3600u) / 60u);
    lv_label_set_text(s_vals[5], tmp);
}

/* 一行数据:名称 20px CLR_TXT_DIM @ x=160,数值 20px CLR_TXT @ x=300 */
static void add_row(lv_obj_t * root, int row, const char * name)
{
    int y = DATA_Y0 + row * DATA_STEP;

    lv_obj_t * n = lv_label_create(root);
    lv_label_set_text(n, name);
    lv_obj_set_style_text_font(n, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(n, CLR_TXT_DIM, 0);
    lv_obj_set_pos(n, 160, y);

    lv_obj_t * v = lv_label_create(root);
    lv_label_set_text(v, "--");            /* 初值,首帧后由 timer 覆盖 */
    lv_obj_set_style_text_font(v, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(v, CLR_TXT, 0);
    lv_obj_set_pos(v, 300, y);
    s_vals[row] = v;
}

void page_dial_on_load(lv_obj_t * root)
{
    btn_styles_init();

    /* 标题(避开顶部 30px 状态栏,内容区从 y≈30 起) */
    lv_obj_t * title = lv_label_create(root);
    lv_label_set_text(title, "CPU MONITOR");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, CLR_TXT, 0);
    lv_obj_set_pos(title, 16, 44);

    /* ===== 中部数据区(X-Track GPS 页样式) ===== */
    /* 左大图标:刷新符号(40px 未启用,用工程最大 32) */
    lv_obj_t * icon = lv_label_create(root);
    lv_label_set_text(icon, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(icon, CLR_TXT, 0);
    lv_obj_set_pos(icon, 44, 260);

    /* 橙色竖线 5x220 */
    lv_obj_t * line = lv_obj_create(root);
    lv_obj_remove_style_all(line);
    lv_obj_set_pos(line, 110, 270);
    lv_obj_set_size(line, 5, 220);
    lv_obj_set_style_bg_color(line, CLR_ACCENT, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);

    /* 6 行名称/数值 */
    static const char * names[6] = { "LOAD", "TEMP", "POWER", "MEMORY", "LOAD AVG", "UPTIME" };
    for (int i = 0; i < 6; i++)
        add_row(root, i, names[i]);

    /* ===== 底部 3 按钮(仿 X-Track Dialplate btnCont) ===== */
    make_btn(root, 0, "3D", on_btn_cube, CLR_TXT);                          /* 跳 3D CUBE */
    make_btn(root, 1, "TEST", on_btn_test, CLR_TXT);                        /* 跳 TEST 页 */
    make_btn(root, 2, "INFO", NULL, lv_color_hex(0x3A4350));                /* 灰显预留 */
}

void page_dial_on_will_appear(lv_obj_t * root)
{
    (void)root;   /* 空实现 */
}

void page_dial_on_did_appear(lv_obj_t * root)
{
    (void)root;
    if (s_timer == NULL) {
        s_timer = lv_timer_create(page_dial_refresh, 1000, NULL);
        page_dial_refresh(s_timer);   /* 立即刷第一帧,避免切入时停留 "--" */
    }
}

void page_dial_on_will_disappear(lv_obj_t * root)
{
    (void)root;
    if (s_timer) {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }
}

void page_dial_on_unload(lv_obj_t * root)
{
    (void)root;   /* 空实现 */
}
