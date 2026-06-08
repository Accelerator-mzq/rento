#!/usr/bin/env bash
# =============================================================================
# check-authoritative-purity.sh
# pt-009 / story-009 / ADR-0007 — CI 权威纯净门
#
# 两类门（每次 push to main + 每个 PR 运行；命中违规即 exit 1 = 阻塞 gate，禁 disable/skip）：
#   Gate A（AC-3 随机硬门）：权威 C++ 模块禁引擎全局 RNG
#     （FMath::Rand* / rand( / std::rand）——AI RNG 须走 UDiceRngService 唯一入口（白名单 FRandomStream）。
#   Gate B（AC-2 目录级断言）：权威模块目录下无 BP 派生类资产
#     B1：权威 C++ 源码树无 .uasset（BP 类不得混入 C++ 源码目录）。
#     B2：Content/ 无【派生自权威 C++ 类】的 BP 派生类资产（DataAsset 数据实例放行）。
#
# 设计（ADR-0007 MVP 分级）：MVP 以「权威逻辑无 BP 类」目录级断言替代逐 K2Node 扫描；
#   Alpha 若 BP 触 gameplay 边缘再增 BP 静态符号扫描检出裸随机 K2Node。
#
# 运行环境：纯 grep/find，无需 UE —— 可在云端 ubuntu-latest 跑（区别于 UE Automation 自托管 job）。
#   兼容 Git-Bash（Windows 本地）与 Linux（CI）。
# =============================================================================
set -uo pipefail

# 仓库根（脚本位于 tools/ci/ → 上两级）
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

FAIL=0

# 权威 C++ 模块源码树（AI/经济/建房/破产/回合/RNG 全落 Source/rento，ADR-0001/0007）
AUTH_SRC="Source/rento"

# 权威 C++ 类名（BP 若派生这些类 = 承载权威逻辑 / 写权威状态，禁止）
AUTH_CLASSES="PlayerTurnSubsystem|RentoPlayerState|DiceRngService|MatchSubsystemBase"

echo "=============================================================="
echo " ADR-0007 权威纯净门（pt-009 / story-009）"
echo "=============================================================="

# -----------------------------------------------------------------------------
# Gate A — 随机硬门（AC-3）
#   精确匹配调用语法（要求紧跟 '('，使 doc 注释里的 'FMath::Rand 等' 不误判）
#   + 排除注释行（以 * 或 // 或 /* 开头的内容行）。
#   白名单：UDiceRngService 内 FRandomStream 是允许的 RNG 原语，不在禁用列表（无需特判）。
# -----------------------------------------------------------------------------
echo ""
echo "==> [Gate A] 随机硬门：禁 FMath::Rand* / rand( / std::rand（白名单 UDiceRngService FRandomStream）"
FORBIDDEN_RNG='FMath::F?Rand[A-Za-z]*\(|(^|[^A-Za-z0-9_])rand\(|std::rand'
RNG_HITS="$(grep -rnE "$FORBIDDEN_RNG" "$AUTH_SRC" --include=*.h --include=*.cpp \
  | grep -vE ':[0-9]+:[[:space:]]*(\*|//|/\*)' || true)"
if [[ -n "$RNG_HITS" ]]; then
  echo "✘ Gate A FAIL — 权威模块命中禁用全局 RNG（AI RNG 须走 UDiceRngService 唯一入口）："
  echo "$RNG_HITS"
  FAIL=1
else
  echo "✔ Gate A PASS — 无禁用全局 RNG 命中"
fi

# -----------------------------------------------------------------------------
# Gate B1 — 权威 C++ 源码树无 .uasset（AC-2）
# -----------------------------------------------------------------------------
echo ""
echo "==> [Gate B1] 目录断言：权威 C++ 源码树（$AUTH_SRC）无 .uasset"
SRC_UASSET="$(find "$AUTH_SRC" -name '*.uasset' 2>/dev/null || true)"
if [[ -n "$SRC_UASSET" ]]; then
  echo "✘ Gate B1 FAIL — 权威源码目录出现 .uasset（疑似 BP 派生类混入 C++ 模块）："
  echo "$SRC_UASSET"
  FAIL=1
else
  echo "✔ Gate B1 PASS — 源码目录无 .uasset"
fi

# -----------------------------------------------------------------------------
# Gate B2 — Content/ 无派生自权威 C++ 类的 BP 派生类资产（AC-2）
#   检出判据（MVP 廉价启发）：.uasset 二进制同时含 'BlueprintGeneratedClass' 标记
#     与某个权威 C++ 类名引用 = BP 派生权威类（违规）。
#   DataAsset 数据实例（如 DA_Board_Classic40，UPrimaryDataAsset）无 BlueprintGeneratedClass 标记 → 放行。
#   呈现层 BP（未来 WBP_*，不派生权威类）→ 放行。
# -----------------------------------------------------------------------------
echo ""
echo "==> [Gate B2] 目录断言：Content/ 无派生自权威 C++ 类的 BP 派生类（DataAsset 放行）"
BP_VIOLATIONS=""
if [[ -d Content ]]; then
  while IFS= read -r -d '' f; do
    if grep -aq "BlueprintGeneratedClass" "$f" && grep -aqE "$AUTH_CLASSES" "$f"; then
      BP_VIOLATIONS+="$f"$'\n'
    fi
  done < <(find Content -name '*.uasset' -print0 2>/dev/null)
fi
if [[ -n "$BP_VIOLATIONS" ]]; then
  echo "✘ Gate B2 FAIL — Content/ 出现派生自权威 C++ 类的 BP 派生类资产："
  echo "$BP_VIOLATIONS"
  FAIL=1
else
  echo "✔ Gate B2 PASS — 无 BP 派生权威类资产（DataAsset 数据实例不计）"
fi

# -----------------------------------------------------------------------------
echo ""
echo "=============================================================="
if [[ "$FAIL" -ne 0 ]]; then
  echo "❌ 权威纯净门失败（ADR-0007）—— 阻塞 gate，禁 disable/skip 失败门。"
  exit 1
fi
echo "✅ 权威纯净门全部通过（ADR-0007）。"
exit 0
