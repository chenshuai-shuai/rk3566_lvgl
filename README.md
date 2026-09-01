# ui_app — 泰山派 LVGL UI 工程

LVGL v9.6.0-dev 轻量 UI。**双入口设计**:同一份 UI 代码,PC 仿真开发 + 板子编译运行。

## 目录结构

| 文件 | 作用 |
|---|---|
| `main_board.c` | 板子入口:fb0 mmap + evdev 鼠标 + 自绘指针 + 心跳日志 |
| `main_sim.c` | PC 仿真入口:SDL2 窗口 480x800(与屏幕同尺寸,所见即所得) |
| `lv_conf.h` | 板子配置(LV_USE_SDL=0) |
| `lv_conf_sim.h` | PC 配置(LV_USE_SDL=1,从 lv_conf.h 派生,勿两边分叉) |
| `Makefile` | 板子版(Linaro aarch64 交叉编译,静态链接) |
| `Makefile.sim` | PC 版(MSYS2 MinGW64 + SDL2) |

## 开发工作流

**PC 仿真(日常开发,Windows)** — MSYS2 终端:
```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2 mingw-w64-x86_64-make
mingw32-make -f Makefile.sim      # 编译
./ui_sim.exe                      # 运行,鼠标可点,效果与板子一致
```

**板子编译(发布,VM)** — 工程经共享文件夹直接可见:
```bash
make                    # 产物 lv_demo
adb push lv_demo /root/
adb shell chmod +x /root/lv_demo
adb shell /root/lv_demo
```

## 关键约定

- **lvgl 源码不入库**:从 VM 拷回(见下)或官方仓库 clone 到 `lvgl/` 目录再编译
  ```bash
  # 方式1(推荐):VM 开机后从 Z 盘拷回(与当前工程同版本)
  robocopy "Z:inuxmy_projectv_demovgl" "C:VirtualBox_shareI_appvgl" /E /XF "*.o" /XD ".git"
  # 方式2:官方仓库克隆(需网络良好)
  git clone --depth 1 https://github.com/lvgl/lvgl.git lvgl
  ```

- **源码 LF 行尾**(VM 交叉编译需要),git 已设 `core.autocrlf=false`
- sdl 驱动源码被 `#if LV_USE_SDL` 包裹(LV_USE_SDL=0 时板子链接无冲突)
- 板子工具链:Linaro 6.3.1 + buildroot sysroot(勿用 Buildroot gcc 9.3.0,缺 libisl)
- 板子 UI 入口:开机由 `/etc/init.d/S99lvgl` 拉起(见知识库 day04)
