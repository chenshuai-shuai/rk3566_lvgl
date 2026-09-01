#ifndef STATUS_BAR_H
#define STATUS_BAR_H
#include "lvgl/lvgl.h"
void status_bar_create(void);            /* 挂 lv_layer_top(),App 启动时调用一次 */
void status_bar_set_visible(bool vis);   /* 页面可控制显示/隐藏(默认显示) */
#endif
