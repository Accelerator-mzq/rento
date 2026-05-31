#!/usr/bin/env bash
# 合并状态栏:claude-hud(ctx%/model 等)+ Game Studios 阶段面包屑
# stdin JSON 只读一次,分别喂给两者;模板行去掉与 claude-hud 重复的 ctx%/model。

input=$(cat)

# --- claude-hud(取最新版本目录)---
hud_dir=$(ls -d "${CLAUDE_CONFIG_DIR:-$HOME/.claude}"/plugins/cache/claude-hud/claude-hud/*/ 2>/dev/null \
  | awk -F/ '{ print $(NF-1) "\t" $0 }' \
  | sort -t. -k1,1n -k2,2n -k3,3n -k4,4n | tail -1 | cut -f2-)
hud_out=""
if [ -n "$hud_dir" ] && [ -f "${hud_dir}dist/index.js" ]; then
  hud_out=$(printf '%s' "$input" | "/c/Program Files/nodejs/node" "${hud_dir}dist/index.js" 2>/dev/null)
fi

# --- Game Studios 模板 statusline,仅取「阶段 [| 面包屑]」段 ---
script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
tmpl_full=$(printf '%s' "$input" | bash "${script_dir}/statusline.sh" 2>/dev/null)
# 模板格式 "ctx: X% | model | stage[ | breadcrumb]" —— 切掉前两段(ctx/model)
tmpl_stage=$(printf '%s' "$tmpl_full" | cut -d'|' -f3- | sed 's/^ *//;s/ *$//')

# --- 拼接:claude-hud 在上,阶段行在末 ---
if [ -n "$hud_out" ] && [ -n "$tmpl_stage" ]; then
  printf '%s\n%s' "$hud_out" "$tmpl_stage"
elif [ -n "$hud_out" ]; then
  printf '%s' "$hud_out"
else
  printf '%s' "$tmpl_full"   # claude-hud 失败 → 优雅降级为完整模板行
fi
