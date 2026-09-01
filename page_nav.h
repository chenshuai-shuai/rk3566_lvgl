/**
 * page_nav.h — 页面导航框架(移植自 X-Track PageManager,C 精简版)
 *
 * X-Track 的 PageManager 提供「单屏幕 + 多 root + 页面生命周期 +
 * 栈式路由 + 切换动画协调」机制;本文件是其核心语义的 C 移植版,
 * 适配 LVGL 9.6 与 480x800 竖屏。
 *
 * 移植日期:2026-09-01
 */
#ifndef PAGE_NAV_H
#define PAGE_NAV_H

#include "lvgl/lvgl.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    const char * name;
    void (*on_load)(lv_obj_t * root);            /* root 创建后:建 UI(一次性) */
    void (*on_will_appear)(lv_obj_t * root);     /* 即将显示:取参数/建 timer */
    void (*on_did_appear)(lv_obj_t * root);      /* 显示动画完成后:启动周期刷新 */
    void (*on_will_disappear)(lv_obj_t * root);  /* 离场前:删 timer/清理 */
    void (*on_unload)(lv_obj_t * root);          /* root 被销毁前(仅 cache=false 时) */
    bool cache;                                  /* 退出后保留 root(HIDDEN)而非销毁 */
} page_t;

void page_nav_init(void);          /* 装配全部页面,进入启动页(见下) */
void page_nav_push(const char * name);   /* 入栈并切换(左→右滑入) */
void page_nav_pop(void);           /* 返回上一页(右滑出) */
void page_nav_register(void(*install)(void)); /* 可选:外部注册回调,本轮可不用 */

#endif /* PAGE_NAV_H */
