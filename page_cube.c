/**
 * page_cube.c — 3D 线框立方体页(旋转矩阵 + 透视投影 + 深度颜色插值)
 *
 * page_nav 框架页面版(2026-09-01):
 *   · 由独立屏页面改为 page_nav 回调式页面(root 由框架创建传入,全屏 CLR_BG);
 *   · 生命周期:on_load 建 UI → on_did_appear 起 33ms 旋转 timer →
 *     on_will_disappear 停 timer;on_unload 空(12 条边线随 root 一起销毁)。
 */
#include "lvgl/lvgl.h"
#include "page_nav.h"     /* page_nav_pop() */
#include "page_cube.h"
#include <math.h>

/* 主题颜色统一来自 ui_palette.h(全工程唯一颜色定义源) */
#include "ui_palette.h"

#define DEG2RAD      (3.14159265358979f / 180.0f)

/* 立方体中心(480x800 分辨率;root 全屏,顶部约 30px 被 StatusBar 覆盖) */
#define CUBE_CX      240
#define CUBE_CY      390

/* 透视投影:模型放大倍数 150,透视系数 4/(4+z) */
#define CUBE_SCALE   150.0f
#define CUBE_PERSP   4.0f

/* 边颜色插值:背面暗 正面亮 */
#define EDGE_DARK    0x3A4450
#define EDGE_LIGHT   0xDDE3EA

/* 12 条边(顶点索引对),顶点 0..7:比特0=x, 比特1=y, 比特2=z */
static const uint8_t g_edges[12][2] = {
    {0,1},{0,2},{0,4},{1,3},{1,5},{2,3},{2,6},
    {3,7},{4,5},{4,6},{5,7},{6,7}
};

static lv_obj_t    * g_lines[12];   /* 12 条边线对象(随 root 销毁) */
static lv_point_precise_t s_pts[12][2]; /* 每边两点;lv_line_set_points 只存指针不拷贝,数据必须常驻! */
static float         g_ang_y;       /* 绕 Y 主轴旋转角(度) */
static float         g_ang_x;       /* 绕 X 晃动角(度) */
static lv_timer_t   * g_timer;      /* 旋转定时器 */

/* 每帧:旋转+透视投影+边线描画(全在宿主侧算,LVGL 只拿整型点) */
static void cube_tick(lv_timer_t * t)
{
    (void)t;

    /* 主旋转 1.6°/帧,绕 X 的晃动角随主轴相位正弦摆动 */
    g_ang_y += 1.6f;
    if (g_ang_y >= 360.0f)
        g_ang_y -= 360.0f;
    g_ang_x = 12.0f * sinf(DEG2RAD * (g_ang_y * 0.05f));

    /* 预计算旋转矩阵(度转弧度) */
    float cy = cosf(DEG2RAD * g_ang_y);
    float sy = sinf(DEG2RAD * g_ang_y);
    float cx = cosf(DEG2RAD * g_ang_x);
    float sx = sinf(DEG2RAD * g_ang_x);

    /* 8 个顶点:旋转 v' = R_y(ang_y) * R_x(ang_x) * v,再透视投影 */
    float rz[8];
    int16_t p[8][2];

    for (int i = 0; i < 8; i++) {
        float x = (i & 1) ? 1.0f : -1.0f;
        float y = (i & 2) ? 1.0f : -1.0f;
        float z = (i & 4) ? 1.0f : -1.0f;

        /* R_x(ang_x) */
        float yx = y * cx - z * sx;
        float zx = y * sx + z * cx;
        /* R_y(ang_y) */
        float ryv = yx;
        float xrv = x * cy + zx * sy;
        float zrv = -x * sy + zx * cy;

        /* z 夹到 [-1,1],防背面过近导致 scale 爆炸、出屏 */
        if (zrv < -1.0f) zrv = -1.0f;
        if (zrv > 1.0f)  zrv = 1.0f;

        rz[i] = zrv;

        /* 透视缩放 + 整型屏幕坐标(四舍五入) */
        float sc = CUBE_SCALE * (CUBE_PERSP / (CUBE_PERSP + zrv));
        p[i][0] = (int16_t)(CUBE_CX + xrv * sc + 0.5f);
        p[i][1] = (int16_t)(CUBE_CY + ryv * sc + 0.5f);
    }

    /* 逐边描画:按平均深度插值颜色,暗->亮 */
    int r0 = (EDGE_DARK >> 16) & 0xFF, g0 = (EDGE_DARK >> 8) & 0xFF, b0 = EDGE_DARK & 0xFF;
    int r1 = (EDGE_LIGHT >> 16) & 0xFF, g1 = (EDGE_LIGHT >> 8) & 0xFF, b1 = EDGE_LIGHT & 0xFF;

    for (int e = 0; e < 12; e++) {
        int a = g_edges[e][0];
        int b = g_edges[e][1];

        /* 深度系数 t∈[0,1],用于颜色插值(整型四舍五入) */
        float t = 0.5f * (rz[a] + rz[b] + 1.0f);
        int   cr = r0 + (int)(t * (r1 - r0) + 0.5f);
        int   cg = g0 + (int)(t * (g1 - g0) + 0.5f);
        int   cb = b0 + (int)(t * (b1 - b0) + 0.5f);

        lv_obj_set_style_line_color(g_lines[e], lv_color_hex(((uint32_t)cr << 16) |
                                    ((uint32_t)cg << 8) | (uint32_t)cb), 0);

        /* 写入常驻数组(数据必须持续有效,set_points 只存指针) */
        s_pts[e][0].x = p[a][0];
        s_pts[e][0].y = p[a][1];
        s_pts[e][1].x = p[b][0];
        s_pts[e][1].y = p[b][1];
        lv_line_set_points(g_lines[e], s_pts[e], 2);   /* 自带 invalidate */
    }
}

/* 返回按钮点击:pop 回上一页 */
static void back_btn_cb(lv_event_t * e)
{
    (void)e;
    page_nav_pop();
}

void page_cube_on_load(lv_obj_t * root)
{
    /* 返回按钮:先 remove 再设底(顺序铁律),点击弹栈 */
    lv_obj_t * btn = lv_button_create(root);
    lv_obj_remove_style_all(btn);
    lv_obj_set_pos(btn, 0, 36);
    lv_obj_set_size(btn, 88, 40);
    lv_obj_set_style_bg_color(btn, CLR_CARD, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);

    lv_obj_t * bl = lv_label_create(btn);
    lv_label_set_text(bl, LV_SYMBOL_LEFT " BACK");
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(bl, CLR_TXT, 0);
    lv_obj_center(bl);

    lv_obj_add_event_cb(btn, back_btn_cb, LV_EVENT_CLICKED, NULL);

    /* 标题 */
    lv_obj_t * title = lv_label_create(root);
    lv_label_set_text(title, "3D CUBE");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, CLR_TXT, 0);
    lv_obj_set_pos(title, 96, 44);

    /* 底部说明文字 */
    lv_obj_t * tip = lv_label_create(root);
    lv_label_set_text(tip, "LVGL software 3D line render");
    lv_obj_set_style_text_font(tip, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(tip, CLR_TXT_DIM, 0);
    lv_obj_align(tip, LV_ALIGN_TOP_MID, 0, 745);

    /* 12 条边线建到 root 上:先 remove 再设宽/圆头/颜色;对象随 root 销毁 */
    for (int i = 0; i < 12; i++) {
        g_lines[i] = lv_line_create(root);
        lv_obj_remove_style_all(g_lines[i]);
        lv_obj_set_style_line_width(g_lines[i], 4, 0);
        lv_obj_set_style_line_rounded(g_lines[i], true, 0);
        lv_obj_set_style_line_color(g_lines[i], CLR_TXT, 0);   /* 初始色,首帧后覆盖 */
    }

    /* 角度复位(仅首次建 root 时执行;缓存命中 on_load 跳过) */
    g_ang_y = 0.0f;
    g_ang_x = 0.0f;
}

void page_cube_on_will_appear(lv_obj_t * root)
{
    (void)root;   /* 空 */
}

void page_cube_on_did_appear(lv_obj_t * root)
{
    (void)root;
    if (g_timer == NULL) {
        g_timer = lv_timer_create(cube_tick, 33, NULL);
        cube_tick(g_timer);   /* 立即画首帧,避免切入时空白 */
    } else {
        cube_tick(g_timer);
    }
}

void page_cube_on_will_disappear(lv_obj_t * root)
{
    (void)root;
    if (g_timer) {
        lv_timer_delete(g_timer);
        g_timer = NULL;
    }
}

void page_cube_on_unload(lv_obj_t * root)
{
    (void)root;   /* 空:12 条边线对象随 root 删除,无需手动释放 */
}
