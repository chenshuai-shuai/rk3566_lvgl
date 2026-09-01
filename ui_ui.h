#ifndef UI_UI_H
#define UI_UI_H

/* UI 主入口:创建 CPU 监视页面(状态栏/环形仪表/曲线/卡片)
 * main_sim.c / main_board.c 都只调用这一个函数,页面代码与平台无关 */
void ui_start(void);

#endif /* UI_UI_H */
