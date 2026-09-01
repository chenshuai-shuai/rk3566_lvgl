/**
 * ui_palette.h — 全工程唯一颜色定义源(视觉契约)
 *
 * 视觉契约:X-Track 风格 — 黑底 + 主橙 0xFF931E + 灰阶卡片 + 大圆角。
 * 改主题色只改本文件,全工程生效;各页面 #include "ui_palette.h" 即可。
 * 注意:页面若有本地颜色宏(如线框色 EDGE_*),不要与 palette 重名。
 */
#ifndef UI_PALETTE_H
#define UI_PALETTE_H

#include "lvgl/lvgl.h"

#define CLR_BG      lv_color_hex(0x0E1116)   /* 页面底(近黑) */
#define CLR_CARD    lv_color_hex(0x1A2028)   /* 卡片底 */
#define CLR_BAR     lv_color_hex(0x141A22)   /* 状态栏底 */
#define CLR_TXT     lv_color_hex(0xDDE3EA)   /* 主文字 */
#define CLR_TXT_DIM lv_color_hex(0x8A94A2)   /* 次文字 */
#define CLR_TRACK   lv_color_hex(0x232B36)   /* 轨道/网格 */
#define CLR_ACCENT  lv_color_hex(0xFF931E)   /* X-Track 主橙:焦点/高亮/仪表指示 */
#define CLR_WARN    lv_color_hex(0xF0B429)   /* 告警黄 */
#define CLR_ERR     lv_color_hex(0xE5484D)   /* 危险红 */

#endif /* UI_PALETTE_H */
