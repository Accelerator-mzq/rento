---
name: ai-heuristic-buyscore-dimensional-imbalance
description: ai-opponent F-2 BuyScore 四项量纲乘权重后失衡(MonopolyProgress 独裁)+ F-1 Buffer 后期被 clamp 钉死 = 基线参数即退化,须改公式非调参
metadata:
  type: project
---

ai-opponent.md 启发式打分的结构性缺陷：加权线性组合的各项量纲不可比，乘权重后某项独裁。

具体（审查 2026-06-05，对抗 review）：
- F-2 BuyScore = W_rent×RentPotential + W_mono×MonopolyProgress + ...。RentPotential 标称 [0,33] 但对经典盘地价/base 租比值实际落 [0,3]（GDD Example 自算=1），MonopolyProgress[1,10] 是二次曲线。乘 F-7 基线权重后 MonopolyProgress 占 BuyScore ~86%，RentPotential 名存实亡 → AI 买地由垄断进度独裁，低估孤立高 ROI 地（车站/深蓝）。
- F-1 Buffer = clamp(B_frac×MaxRentExposure, min, max)。MaxRentExposure 是对手最高单次租（酒店租 1050–2000），Sharp B_frac=1.5、Buffer_max=1200 → 任一对手有酒店即 Buffer 钉死 1200，B_frac 失效（line 297 自称"对最坏租的倍数"被 clamp 截断后无意义）。后期玩家现金 500–1500 → Sharp 满足不了缓冲 → 不建/不买（concept 头号风险"现金充裕不建房"在基线触发）。

**Why:** 这类缺陷会让 GDD 自己 Tuning 节枚举的退化（旁观者/集组不建）在**基线参数下**发生，而非"调坏参数才触发"——所以是公式/量纲/clamp 语义问题，改 F-7 数值救不了。
**How to apply:** 审查任何加权启发式打分系统，先对每项代入真实数据域算"乘权重后的实际贡献占比"，别信变量表标称范围（标称 [0,33] 不等于实际可达）；clamp 上下界要代入中后期常态值验证是否被钉死失语义。参见 [[obligation-registered-not-consistent]]（标称/登记≠实际行为）。
