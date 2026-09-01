#ifndef UI_UI_H
#define UI_UI_H

/* UI 应用入口声明(实现在 page_nav.c:data_source_init + status_bar + page_nav)
 * main_sim.c / main_board.c 都只调用 ui_start(),页面代码与平台无关 */

void ui_start(void);

#endif /* UI_UI_H */
