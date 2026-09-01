#!/bin/bash
# 重建仓库:排除 git_backup 备份目录(rebuild after accidental backup inclusion)
set -e
cd /c/VirtualBox_share/ui_app

# 1) 备份目录移出(保留旧历史,不再进仓库)
if [ -d .git_backup ]; then
  mv .git_backup /c/VirtualBox_share/ui_app_git_backup_20260901
  echo "backup moved -> /c/VirtualBox_share/ui_app_git_backup_20260901"
fi

# 2) 删除含垃圾对象的 .git
rm -rf .git

# 3) 全新初始化
git init -q
git config user.name "cs343"
git config user.email "cs343@local"
git config core.autocrlf false
git config http.proxy "http://127.0.0.1:7897"
git config http.sslBackend openssl
git remote add rk3566_lvgl https://github.com/chenshuai-shuai/rk3566_lvgl.git

# 4) 防止备份目录再进来
[ -f .gitignore ] || printf '*.o\nlv_demo\nui_sim.exe\n' > .gitignore
grep -q git_backup .gitignore || printf 'git_backup*\n' >> .gitignore

# 5) 重新加 lvgl 子模块
rm -rf lvgl
git submodule add --depth 1 https://github.com/lvgl/lvgl.git lvgl 2>&1 | tail -2

# 6) 提交(所有自身代码)
git add -A
git commit -q -m "init: ui_app slim (lvgl as submodule, own code only)

- lvgl 官方库 = git submodule(仓库仅含自身代码,push 秒传)
- 不包含 .git_backup(旧历史已移出到 C:\\\\VirtualBox_share\\\\ui_app_git_backup_20260901)
- lv_conf.h / lv_conf_sim.h / main_board.c / main_sim.c / ui_ui.c / data_source.c / Makefile*"

echo "=== 索引文件数: $(git ls-files | wc -l)"
echo "=== git 对象大小:"
du -sh .git
git log --oneline | head -2
