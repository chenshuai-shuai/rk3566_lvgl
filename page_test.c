/**
 * page_test.c — 测试页(page_nav 框架回调式)
 *
 * 生命周期:on_load 建 UI(一次性)→ on_will_appear / on_did_appear
 *           → on_will_disappear → (cache=false 时)on_unload。
 *
 * root 由页面框架 page_nav.c 创建:remove_style_all 后铺 CLR_BG
 * 全屏(480x800),页面内不再建底、不再设屏。顶部约 30px 被
 * StatusBar 覆盖,内容从 y≈36 起(返回按钮即占位于此)。
 *
 * 本页无定时器/面板刷新等资源,除 on_load 外其余回调均为空实现,
 * 仅用于验证框架生命周期与返回按钮(page_nav_pop)。
 */
#include "lvgl/lvgl.h"
#include "page_test.h"
#include "page_nav.h"        /* page_nav_pop() */

/* 主题颜色统一来自 ui_palette.h(全工程唯一颜色定义源) */
#include "ui_palette.h"

/* 返回按钮点击:弹栈回上一页 */
static void page_test_back_cb(lv_event_t * e)
{
    (void)e;
    page_nav_pop();
}

void page_test_on_load(lv_obj_t * root)
{
    /* --- 标题(状态栏下方,20px) --- */
    lv_obj_t * title = lv_label_create(root);
    lv_label_set_text(title, "TEST");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, CLR_TXT, 0);
    lv_obj_set_pos(title, 96, 44);

    /* --- 返回按钮:(0,36) 88x40,去样式后铺 CLR_CARD --- */
    lv_obj_t * btn = lv_button_create(root);
    lv_obj_remove_style_all(btn);              /* 先 remove,再摆位 */
    lv_obj_set_pos(btn, 0, 36);
    lv_obj_set_size(btn, 88, 40);
    lv_obj_set_style_bg_color(btn, CLR_CARD, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);

    lv_obj_t * lab = lv_label_create(btn);
    lv_label_set_text(lab, LV_SYMBOL_LEFT " BACK");
    lv_obj_set_style_text_font(lab, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lab, CLR_TXT, 0);
    lv_obj_center(lab);

    lv_obj_add_event_cb(btn, page_test_back_cb, LV_EVENT_CLICKED, NULL);

    /* --- 居中大字 --- */
    lv_obj_t * big = lv_label_create(root);
    lv_label_set_text(big, "HELLO LVGL");
    lv_obj_set_style_text_font(big, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(big, CLR_TXT, 0);
    lv_obj_align(big, LV_ALIGN_CENTER, 0, -20);

    /* --- 副行小字 --- */
    lv_obj_t * sub = lv_label_create(root);
    lv_label_set_text(sub, "page lifecycle OK");
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sub, CLR_TXT_DIM, 0);
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, 30);
}

void page_test_on_will_appear(lv_obj_t * root)
{
    /* 将要显示:本页无定时器/参数需初始化,空实现 */
    (void)root;
}

void page_test_on_did_appear(lv_obj_t * root)
{
    /* 显示动画完成:本页无周期刷新,空实现 */
    (void)root;
}

void page_test_on_will_disappear(lv_obj_t * root)
{
    /* 要离场:本页无定时器需删除,空实现 */
    (void)root;
}

void page_test_on_unload(lv_obj_t * root)
{
    /* root 即将销毁(仅 cache=false):本页无堆资源,空实现 */
    (void)root;
}
