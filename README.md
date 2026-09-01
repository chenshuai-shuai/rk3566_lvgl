# ui_app — 泰山派 LVGL UI(X-Track 式页面框架)

LVGL v9.6.0 轻量 UI,**双入口**:同一份代码,Windows SDL2 仿真开发 + 板子真机编译运行。
页面架构移植自 X-Track(Shiren)的 PageManager 语义(C 精简版,非逐行为抄):单 screen 多 root、
页面生命周期回调、栈式路由、切换动画、状态栏常驻。

## 目录结构

| 文件/目录 | 作用 |
|---|---|
| `main_sim.c` | PC 仿真入口:SDL2 窗口 480x800(所见即所得) |
| `main_board.c` | 板子入口:fb0 mmap(fbdev)+ evdev 鼠标 + 自绘指针 + 心跳日志 |
| `page_nav.c/.h` | ★页面导航框架(移植 X-Track PageManager):页面注册表、5 回调生命周期(on_load/will_appear/did_appear/will_disappear/unload)、push/pop 栈路由、300ms 切换动画、忙防护、root 缓存 |
| `page_dial.c/.h` | CPU 监视主页(启动页,X-Track GPS 页样式):标题 + 大图标 + 橙竖线 + 6 行真数据(LOAD/TEMP/POWER/MEMORY/LOAD AVG/UPTIME)+ 底部导航按钮 |
| `page_cube.c/.h` | 3D 线框旋转立方体页(软件渲染:旋转矩阵 + 透视投影 + 深度颜色,33ms 帧) |
| `page_test.c/.h` | 最小测试页(验证框架链路) |
| `status_bar.c/.h` | 常驻状态栏(挂 lv_layer_top):橙色图标 + 时钟 + CPU 占用,1s 刷新 |
| `data_source.c/.h` | 数据源(同接口双实现):sim=Windows API(GetSystemTimes/GlobalMemoryStatusEx/GetTickCount64),板子=/proc(stat/meminfo/loadavg/uptime/thermal_zone0/cpufreq) |
| `ui_palette.h` | 集中色板(X-Track 风格,主橙 CLR_ACCENT 0xFF931E;全工程唯一颜色源) |
| `lv_conf.h` | 板子配置(LV_USE_SDL=0) |
| `lv_conf_sim.h` | PC 配置(LV_USE_SDL=1,从 lv_conf.h 派生,勿两边分叉) |
| `Makefile` / `Makefile.sim` | 板子(Linaro 交叉编译,静态)/ PC 仿真 |
| `.vscode/tasks.json` | VS Code Ctrl+Shift+B 一键编译 sim(MSYS make 包装) |

## 环境搭建(另一台电脑继续开发)

### PC 仿真(Windows + MSYS2)

1. 安装 [MSYS2](https://www.msys2.org/)(默认 `C:\msys64`),打开 MSYS2 终端安装依赖:
   ```bash
   pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2 mingw-w64-x86_64-make
   ```
2. 准备 LVGL(**不入库**,代码按 v9.6.0 API 编写,必须同版本):
   ```bash
   cd <你的 checkout 目录>
   # 方式A:官方 master(核对 v9.6.0):
   git clone --depth 1 https://github.com/lvgl/lvgl.git lvgl
   grep -E "LVGL_VERSION_(MAJOR|MINOR|PATCH)" lvgl/include/lvgl/lv_version.h   # 应为 9.6.0
   # 方式B:从已有环境拷贝 lvgl/ 目录(/E /XF "*.o" /XD ".git")
   ```
3. 编译 + 运行(**必须 MSYS 版 make**,mingw32-make 有引号 bug;exe 运行需 SDL2.dll 在 PATH):
   ```bash
   export PATH=/c/msys64/mingw64/bin:$PATH
   make -f Makefile.sim
   ./ui_sim.exe
   ```
   ⚠️ git-bash 直启 MSYS make 可能因 TMP 被清空导致长链接命令行截断(ld: cannot find ...);解法:用 `C:/msys64/usr/bin/bash.exe -lc` 包装,或直接在 MSYS2 终端里执行。
4. VS Code 打开工程后 `Ctrl+Shift+B` 一键编译,错误自动进 Problems 面板。

### 板子(VM)

- 工具链:Linaro 6.3.1 + buildroot sysroot(勿用 Buildroot gcc 9.3.0,缺 libisl.so.15);静态链接
- 工程经共享文件夹可见,VM 内 `make` 出 `lv_demo`:
  ```bash
  adb push lv_demo /root/ && adb shell chmod +x /root/lv_demo && adb shell /root/lv_demo
  ```
- 开机自启:/etc/init.d/S99lvgl(rcS 通配符 `S??*` 坑:禁用脚本须去掉 S 前缀,加 .disabled 无效)
- 防 fbcon 日志盖 UI:`memset(fbp,0,smem_len)` 清屏 + `echo "0 4 1 7" > /proc/sys/kernel/printk`

## 页面扩展规范(新增一个页面)

1. 新建 `page_xxx.c/.h`,实现 5 个生命周期回调(on_load 建 UI;on_did_appear 建 timer;on_will_disappear 删 timer;其他可空)
2. `page_nav.c` 的 `s_pages[]` 加一行注册(名称与回调),即可在框架里 push/pop
3. 页面内容避开顶部 30px(状态栏常驻区);返回按钮参考 page_cube/page_test(◄ BACK → page_nav_pop())

## UI 铁律(LVGL 9.6 实测坑,踩过勿重复)

1. **`lv_obj_remove_style_all()` 必须先于 `lv_obj_set_pos()`**——v9 的 set_pos 把坐标存进 style 属性,顺序反了坐标清零,对象全叠回 (0,0)
2. **`lv_line_set_points()` 只存指针不拷贝**——points 必须常驻(static)数组,栈上临时数组会悬垂
3. **浮点显示**:LVGL 内置 sprintf 不支持 `%f`,必须 libc `snprintf` 到 char[] 再 `lv_label_set_text`
4. **字体按需启用**:lv_conf 里 `LV_FONT_MONTSERRAT_14/20/24/26/32` 已启用(未启用字号会编译报错);改 lv_conf 宏后必须 `make clean`(源码时间戳旧,make 会误判不重编)
5. **页面 timer 必须成对**:on_did_appear 建、on_will_disappear 删(后台页不空转,内存不泄漏)
6. **数据源铁律**:`cpu_info_t` 字段名/类型是接口契约,UI 不关心数据来源;板子侧 /proc 读失败给默认 0

## 已知限制

- 网络:GitHub 大流量直连被断(克隆 lvgl 79MB 会 408);lvgl 应从其他路径拷贝或用代理
- sim 温度的功耗是估算值(UI 已标 EST);板子侧温度走 thermal_zone0
- sim 鼠标无滚轮事件(wheel indev 未接,拖动/点击已可用)
