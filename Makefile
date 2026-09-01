# 2026-08-31 改用 Linaro 6.3.1(9.3.0 工具链缺 libisl.so.15,cc1 起不来)
CROSS_COMPILE := /home/csubuntu/share/linux/prebuilts/gcc/linux-x86/aarch64/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-
CC            := $(CROSS_COMPILE)gcc
SYSROOT       := /home/csubuntu/share/linux/buildroot/output/rockchip_rk3566/host/aarch64-buildroot-linux-gnu/sysroot

CFLAGS  = -O2 -Wall -I. -Ilvgl -Ilvgl/src -DLV_CONF_INCLUDE_SIMPLE
CFLAGS += --sysroot=$(SYSROOT)
LDFLAGS = --sysroot=$(SYSROOT) -static -lm

# 一把抓 LVGL src 下所有 .c
LVGL_SRC = $(shell find lvgl/src -name '*.c') $(shell find lvgl/demos -name '*.c')

# 板子版入口(2026-09-01 拆分):main_board.c = 原 main.c(fbdev+evdev+指针+心跳)
# 注意:lv_conf.h 中 LV_USE_SDL=0 → sdl 驱动源码被 #if LV_USE_SDL 编成空文件,链接无冲突
SRCS = main_board.c ui_ui.c data_source.c $(LVGL_SRC)
OBJS = $(SRCS:.c=.o)

all: lv_demo

lv_demo: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f lv_demo $(OBJS)