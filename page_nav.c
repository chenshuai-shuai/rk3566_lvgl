/**
 * page_nav.c — 页面导航框架(移植自 X-Track PageManager,C 精简版)
 *
 * 参考:X-Track 工程 USER/App/Utils/PageManager 下的
 *   PageBase.h / PageManager.h / PM_Base / PM_Router / PM_State / PM_Anim
 *
 * 移植的核心语义:
 *   1) 单 screen,多 root:所有页面 root 均建在 lv_screen_active() 之下;
 *   2) 页面生命周期:on_load → on_will_appear → on_did_appear
 *                    → on_will_disappear → (on_unload);
 *   3) 栈式路由:push 入栈、pop 回退;cache=true 页面离开后 root 隐藏保留
 *      (HIDDEN),cache=false 页面离开时 on_unload + 销毁,返回时重建;
 *   4) 切换动画协调:push 新页 x=480→0 右→左滑入,旧页 0→-96 轻微左推;
 *      pop 反向(下页 -96→0,离开页 0→480);均为 300ms ease_out;
 *      动画期间 s_busy 置起,ready 回调中释放,防重入。
 *
 * 移植日期:2026-09-01
 *
 * 注意:页面表本轮为占位(回调全 NULL,编译零依赖),由页面文件
 * (page_dial.c / page_info.c)下一轮提供真实函数时替换,例如:
 *   { "dial", page_dial_create, page_dial_will_appear, ... }
 * 函数原型形如 extern void page_dial_create(lv_obj_t * root);
 */
#include "page_nav.h"
#include "ui_palette.h"      /* CLR_BG = 0x0E1116,全工程唯一颜色源 */
#include "status_bar.h"
#include "data_source.h"
#include "page_dial.h"
#include "page_cube.h"
#include "page_test.h"

#include <string.h>

/* ---- 几何与动画参数 ---- */
#define SCR_W        480     /* 屏幕宽 */
#define SCR_H        800     /* 屏幕高 */
#define ANIM_TIME    300     /* 切换动画时长 ms */
#define PUSH_NEW_X   480     /* push 新页起点:右侧屏外 */
#define PUSH_OLD_X   -96     /* push 旧页终点:轻微左推 */
#define POP_BELOW_X  -96     /* pop 下页起点:左推位 */
#define POP_TOP_X    480     /* pop 离开页终点:右侧屏外 */

/* ---- 页面注册表(真实页面,dial=启动页,2026-09-01 装配) ---- */
static const page_t s_pages[] = {
    { "dial", page_dial_on_load, page_dial_on_will_appear, page_dial_on_did_appear, page_dial_on_will_disappear, page_dial_on_unload, true },
    { "cube", page_cube_on_load, page_cube_on_will_appear, page_cube_on_did_appear, page_cube_on_will_disappear, page_cube_on_unload, true },
    { "test", page_test_on_load, page_test_on_will_appear, page_test_on_did_appear, page_test_on_will_disappear, page_test_on_unload, true },
};
#define PAGE_COUNT 3                 /* 与 s_pages 条目数一致,改页表时同步 */
#define PAGE_NUM   ((int)(sizeof(s_pages) / sizeof(s_pages[0])))

/* ---- 运行状态 ---- */
static const page_t * s_stack[8];   /* 页面栈(存页表指针) */
static int   s_stack_idx = -1;      /* 栈顶下标,栈底=0;<0 表示未初始化 */
static const page_t * s_cur   = NULL;   /* 当前页 */
static lv_obj_t *      s_cur_root = NULL;/* 当前页 root */
static lv_obj_t *      s_root_cache[PAGE_COUNT]; /* 按页表下标缓存 root(cache=true) */
static bool   s_busy  = false;      /* 动画忙:期间拒绝一切切换 */
static const page_t * s_leave_page = NULL;  /* 离场页(动画 ready 后隐藏/销毁) */
static lv_obj_t *      s_leave_root = NULL; /* 其 root */
static void (*s_install)(void) = NULL;      /* page_nav_register 预留钩子 */

/* 按名字查页表下标;不存在返回 -1 */
static int page_index(const char * name)
{
    for (int i = 0; i < PAGE_NUM; i++) {
        if (strcmp(s_pages[i].name, name) == 0)
            return i;
    }
    return -1;
}

/* 创建 root:remove_style_all 后铺 CLR_BG 全屏底(铁律:先 remove 再摆位) */
static lv_obj_t * root_make(void)
{
    lv_obj_t * root = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(root);              /* 先 remove,清主题样式 */
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_size(root, SCR_W, SCR_H);
    lv_obj_set_style_bg_color(root, CLR_BG, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_scroll_dir(root, LV_DIR_NONE);   /* 整页 root 不滚动 */
    return root;
}

/* 取页面 root:cache 命中直接复用(跳过 on_load),否则新建并触发 on_load */
static lv_obj_t * page_root_acquire(int idx)
{
    const page_t * p = &s_pages[idx];
    if (p->cache) {
        lv_obj_t * r = s_root_cache[idx];
        if (r != NULL)
            return r;                           /* 缓存命中:跳过 on_load */
    }
    lv_obj_t * root = root_make();
    if (p->on_load) p->on_load(root);           /* 一次性建 UI */
    if (p->cache) s_root_cache[idx] = root;
    return root;
}

/* 离场处理:cache → 隐藏保留;非 cache → on_unload + 销毁 */
static void root_leave(const page_t * p, lv_obj_t * root)
{
    if (p->cache) {
        lv_obj_set_hidden(root, true);
    } else {
        if (p->on_unload) p->on_unload(root);
        lv_obj_delete(root);
    }
}

/* 统一动画:水平移动 + 可选完成回调(lv_anim_completed_cb_t) */
static void anim_move_x(lv_obj_t * obj, int32_t from, int32_t to,
                        lv_anim_completed_cb_t completed)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, ANIM_TIME);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    if (completed) lv_anim_set_completed_cb(&a, completed);
    lv_anim_start(&a);
}

/* push:新页滑入完成 → did_appear + 释放 s_busy */
static void push_completed_cb(lv_anim_t * a)
{
    (void)a;
    if (s_cur->on_did_appear) s_cur->on_did_appear(s_cur_root);
    s_busy = false;
}

/* pop:下页归位完成 → 弹出栈顶 + did_appear + 释放 s_busy */
static void pop_completed_cb(lv_anim_t * a)
{
    (void)a;
    s_stack_idx--;                              /* 栈顶弹出 */
    if (s_cur->on_did_appear) s_cur->on_did_appear(s_cur_root);
    s_busy = false;
}

/* 离场页动画完成:隐藏(缓存)或 on_unload+销毁(不缓存) */
static void leave_completed_cb(lv_anim_t * a)
{
    (void)a;
    root_leave(s_leave_page, s_leave_root);
    s_leave_page = NULL;
    s_leave_root = NULL;
}

/* ---- 公共接口 ---- */

void page_nav_init(void)
{
    if (s_install) s_install();                 /* 预留钩子(本轮空闲) */

    /* 启动页 = 页表第 0 项 "dial":建 root + 生命周期,无动画 */
    s_cur = &s_pages[0];
    s_cur_root = page_root_acquire(0);
    lv_obj_move_foreground(s_cur_root);
    lv_obj_set_hidden(s_cur_root, false);
    if (s_cur->on_will_appear) s_cur->on_will_appear(s_cur_root);
    s_stack[0] = s_cur;
    s_stack_idx = 0;
    if (s_cur->on_did_appear) s_cur->on_did_appear(s_cur_root);
}

void page_nav_push(const char * name)
{
    if (s_busy) return;                         /* 动画忙防护 */
    int idx = page_index(name);
    if (idx < 0) return;                        /* 未知页 */
    const page_t * p = &s_pages[idx];

    /* 去重:同名页面已在栈中则忽略 */
    for (int i = 0; i <= s_stack_idx; i++) {
        if (s_stack[i] == p) return;
    }

    const page_t * old_page = s_cur;
    lv_obj_t * old_root = s_cur_root;

    /* a) root:首次新建 + on_load;缓存命中直接复用(跳过 on_load) */
    lv_obj_t * root = page_root_acquire(idx);
    /* e) 新页置顶 */
    lv_obj_move_foreground(root);
    lv_obj_set_hidden(root, false);
    /* c) 未显示先通知 */
    if (p->on_will_appear) p->on_will_appear(root);

    /* 新页入栈 / 更新当前页 */
    s_cur = p;
    s_cur_root = root;
    s_stack[++s_stack_idx] = p;

    /* d) 动画:新页 480→0,旧页 0→-96,完成后旧页隐藏或销毁 */
    s_busy = true;
    if (old_root != NULL) {
        s_leave_page = old_page;
        s_leave_root = old_root;
        anim_move_x(old_root, 0, PUSH_OLD_X, leave_completed_cb);
    } else {
        s_leave_page = NULL;
    }
    anim_move_x(root, PUSH_NEW_X, 0, push_completed_cb);
}

void page_nav_pop(void)
{
    if (s_busy) return;                         /* 动画忙防护 */
    if (s_stack_idx <= 0) return;               /* 栈深≤1,不可弹 */

    const page_t * top_page = s_cur;            /* 离开页 */
    lv_obj_t * top_root = s_cur_root;
    const page_t * below_page = s_stack[s_stack_idx - 1];
    int below_idx = page_index(below_page->name);

    /* 旧页离场前通知 */
    if (top_page->on_will_disappear) top_page->on_will_disappear(top_root);

    /* 下页 root:缓存命中或重建(非缓存页被销毁过的场合) */
    lv_obj_t * below_root = page_root_acquire(below_idx);
    lv_obj_move_foreground(below_root);
    lv_obj_set_hidden(below_root, false);
    if (below_page->on_will_appear) below_page->on_will_appear(below_root);

    s_cur = below_page;
    s_cur_root = below_root;

    /* 反向动画:下页 -96→0,离开页 0→480 */
    s_busy = true;
    s_leave_page = top_page;
    s_leave_root = top_root;
    anim_move_x(below_root, POP_BELOW_X, 0, pop_completed_cb);
    anim_move_x(top_root, 0, POP_TOP_X, leave_completed_cb);
}

/* 应用入口:main_sim.c / main_board.c 均调用(声明见 ui_ui.h)
 * 组装:数据源 → 状态栏 → 页面导航(直接进启动页 dial) */
void ui_start(void)
{
    data_source_init();
    status_bar_create();
    page_nav_init();
}


void page_nav_register(void(*install)(void))
{
    s_install = install;        /* 可选外部钩子:本轮不用,init 时调用 */
}
