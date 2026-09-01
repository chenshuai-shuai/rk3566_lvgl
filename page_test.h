#ifndef PAGE_TEST_H
#define PAGE_TEST_H

#include "lvgl/lvgl.h"

/* page_nav 框架页面回调(root 由框架创建,480x800 全屏 CLR_BG) */
void page_test_on_load(lv_obj_t * root);
void page_test_on_will_appear(lv_obj_t * root);    /* 空 */
void page_test_on_did_appear(lv_obj_t * root);     /* 空 */
void page_test_on_will_disappear(lv_obj_t * root); /* 空 */
void page_test_on_unload(lv_obj_t * root);         /* 空 */

#endif
