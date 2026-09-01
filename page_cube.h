#ifndef PAGE_CUBE_H
#define PAGE_CUBE_H
#include "lvgl/lvgl.h"
void page_cube_on_load(lv_obj_t * root);
void page_cube_on_will_appear(lv_obj_t * root);   /* 空 */
void page_cube_on_did_appear(lv_obj_t * root);    /* 建 33ms 旋转 timer */
void page_cube_on_will_disappear(lv_obj_t * root);/* 删 timer */
void page_cube_on_unload(lv_obj_t * root);        /* 空(线对象随 root 删除) */
#endif
