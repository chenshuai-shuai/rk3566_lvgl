/**
 * page_dial.h — CPU 监视主页面(X-Track dial 风格,启动页,2026-09-01)
 *
 * 页面名 "dial",由 page_nav 管理生命周期(root 由框架创建,480x800):
 *   on_load          建标题/数据区/底部按钮(一次性)
 *   on_will_appear   空实现
 *   on_did_appear    建 1s 数据刷新 timer 并立即刷第一帧
 *   on_will_disappear 删 timer
 *   on_unload        空实现(页面 cache=true,root 隐藏保留)
 */
#ifndef PAGE_DIAL_H
#define PAGE_DIAL_H
#include "lvgl/lvgl.h"
void page_dial_on_load(lv_obj_t * root);
void page_dial_on_will_appear(lv_obj_t * root);   /* 空实现 */
void page_dial_on_did_appear(lv_obj_t * root);    /* 建 1s 数据刷新 timer */
void page_dial_on_will_disappear(lv_obj_t * root);/* 删 timer */
void page_dial_on_unload(lv_obj_t * root);        /* 空实现 */
#endif
