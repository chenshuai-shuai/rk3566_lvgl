/* ===== LVGL PC 仿真入口(Windows + MinGW/MSYS2 + SDL2) =====
 *
 * 设计:与板子版(main_board.c)共用同一份 lvgl 源码与 UI 代码,
 * 只有平台入口不同:
 *   - 板子: fbdev + evdev(Linux 专用)
 *   - PC  : SDL2 窗口(Windows 开发调试)
 * 配置:本文件用 -DLV_CONF_PATH="lv_conf_sim.h"(LV_USE_SDL=1)
 * 板子配置: lv_conf.h(LV_USE_SDL=0),二者互不影响。
 *
 * 编译: make -f Makefile.sim && ./ui_sim.exe
 * 窗口尺寸 = 泰山派屏幕 480x800,保证所见即所得。
 */
#include "lvgl/lvgl.h"
#include "lvgl/demos/lv_demos.h"
#include "ui_ui.h"
#include <stdio.h>
/* 用标准 main() 而非 SDL_main(argc,argv)(须在 include SDL 之前定义) */
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

int main(void)
{
    printf("[sim] ==== LVGL UI simulator (Windows/SDL2) ====\n");

    /* ★ 必须最先调 lv_init()(LVGL 全局初始化)!lv_sdl_window_create 内部
     * 只做 SDL_Init + tick + 事件定时器,不含 lv_init */
    lv_init();

    /* lv_sdl_window_create 内部:
     *   SDL_Init + lv_tick_set_cb(SDL_GetTicks) + 5ms 事件定时器
     *  => 不需要手动配置 tick 和事件循环 */
    lv_display_t * disp = lv_sdl_window_create(480, 800);
    if (!disp) { printf("[sim] FATAL: window create failed\n"); return 1; }
    printf("[sim] window 480x800 OK\n");

    lv_indev_t * mouse = lv_sdl_mouse_create();
    if (!mouse) printf("[sim] WARN: mouse indev failed\n");
    else        printf("[sim] mouse indev OK\n");

    /* 我们的 UI 页面(sim/板子共用,数据源自动切换) */
    ui_start();
    printf("[sim] UI started, enter loop\n");

    while (1) {
        lv_timer_handler();
        SDL_Delay(5);
    }
    return 0;
}
