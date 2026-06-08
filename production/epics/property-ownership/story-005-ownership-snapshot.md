# Story 005: 归属快照 BuildOwnershipSnapshot (值拷贝, 防漂移)

> **Epic**: 地产所有权 Property Ownership
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: [TBD]
> **Manifest Version**: 2026-06-06
> **Last Updated**: [set by /dev-story]

## Context

**GDD**: `design/gdd/property-ownership.md`（CR-6）
**Requirement**: `TR-prop-007`、`TR-prop-008`
*(Requirement text lives in `docs/architecture/tr-registry.yaml`)*

**ADR Governing Implementation**: ADR-0006（GameStateSnapshot 值语义，primary）· ADR-0001
**ADR Decision Summary**: `BuildOwnershipSnapshot` 组装只读**值快照**（owner/is_mortgaged/is_monopoly/station_count/utility_count，**不含 house_count**）push 给经济算租；值拷贝非活引用，防结算期漂移；单向 push 防 5↔6 环。

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: `static_assert` 字段集核验；值拷贝 USTRUCT。`-nullrhi` 可测。

**Control Manifest Rules (Core/ADR-0006)**:
- Required: 快照值拷贝（非活引用）；不含 house_count（防 6↔8 环）；单向 push（economy 不反查 6）。
- Forbidden: 快照含 house_count（读了系统8）；返活引用/指针（结算期漂移）；6 读建房8。
- Guardrail: 快照在归属事务完成后构建（取最新 owner map）。

---

## Acceptance Criteria

- [ ] **AC-30** 快照字段存在性 + 具体值：① 字段存在性——`BuildOwnershipSnapshot(P1)` 返 struct 恰含 owner/is_mortgaged/is_monopoly/station_count/utility_count 五字段、类型正确、**不含 house_count**（reflection/`static_assert` 核验字段集==5；误加 house_count 则负断言失败）；② 具体值——P1 拥红组 {21,23,24} 全部且 21 已抵押、拥 Railroad {5,15} → `is_monopoly(Red)==true` ∧ `is_mortgaged(21)==true` ∧ `station_count==2`。
- [ ] **AC-30b** [快照隔离] 值拷贝防漂移：已 `BuildOwnershipSnapshot(P1)` 得 snapshot，随后修改 owner map（Buy 另一格/TransferOwnership）→ snapshot 字段**不随之改变**（值拷贝非活引用）。
- [ ] **AC-31** [Advisory·code-review / ADR-0013 升 Logic] push 方向：snapshot 由 6 push 给经济租金入口（economy 不反查 6 字段）；economy 算租入口接收快照参数，economy 实现内无对本系统查询接口调用。
- [ ] **AC-31b** [push 时机] 快照反映最新归属：同程内先 `Buy(tile,P1)` 完成，随后 `BuildOwnershipSnapshot(P1)` → snapshot.owner 反映本次 Buy 结果（非过期值）。
- [ ] **AC-32** [Advisory·code-review / ADR-0013 升 Logic] 6 不读建房(8)：spy/静态检查本系统对系统#8 调用==0（快照不含 house_count，防 6↔8 环）。

---

## Implementation Notes

*Derived from ADR-0006:*

- **`BuildOwnershipSnapshot(int32 PlayerId, int32 TileIndex) → FOwnershipSnapshot`**（值返回）：组装 `{owner, is_mortgaged, is_monopoly, station_count, utility_count}` 五字段（聚合 story-004 谓词 + 容器读），**不含 house_count**（归建房8，防 6↔8 环）。
- **字段集核验（AC-30①）**：`static_assert`/reflection 断言字段集恰 5 个且不含 house_count；实现若误加 house_count（读了系统8）则负断言失败。
- **值拷贝隔离（AC-30b）**：返回 `FOwnershipSnapshot` 值（非活引用/指针）；构建后即便 owner map 被外部修改，经济使用的仍是构建瞬间的值（防结算期归属漂移）。
- **push 时机（AC-31b）**：快照在调用方（回合2 ResolvePhase）需算租那一刻、且在本程任何归属事务（Buy/Mortgage）完成**之后**构建（取最新 owner map）。
- **push 方向（AC-31，5↔6 无环）**：snapshot 由 6 push 给经济租金入口；economy 不反查 6 字段（静态扫描 economy 算租入口接收快照参数、无对本系统查询调用，ADR-0013 升 Logic）。
- **6 不读 8（AC-32，6↔8 无环）**：spy/静态检查本系统对系统#8 调用==0。完整 rent 参数 = 6 快照 + 8 house_count，二者在回合2 ResolvePhase 聚合（聚合接口形态归回合2 + ADR-0006，本系统只保证自己快照字段正确且不含 house_count）。
- `FOwnershipSnapshot` 最终 struct 形态归 ADR-0006 GameStateSnapshot 体系协调（本 story 保证不含 house_count + 值拷贝）。

---

## Out of Scope

- Story 004：谓词本体（本 story 聚合）。Story 002/003：归属事务。
- 回合2 的 snapshot 聚合（6 快照 + 8 house_count，player-turn/ADR-0006）；经济算租消费（economy story-004/005）；建房8 house_count（building epic）。

---

## QA Test Cases

- **AC-30①**：`static_assert`/reflection 字段集==5 {owner,is_mortgaged,is_monopoly,station_count,utility_count}，不含 house_count（误加则负断言 FAIL）。
- **AC-30②**：P1 拥红组全 + 21 抵押 + Railroad {5,15} → is_monopoly(Red)==true ∧ is_mortgaged(21)==true ∧ station_count==2（mock GetTilesInGroup(Red)→{21,23,24}）。
- **AC-30b**：取 snapshot → 随后 Buy 另一格/TransferOwnership → snapshot 字段不变（值拷贝）。
- **AC-31**：[静态扫描] economy 算租入口接收快照参数、无对本系统查询调用。
- **AC-31b**：同程先 Buy(tile,P1) → BuildOwnershipSnapshot(P1) → snapshot.owner 反映 GetOwner(tile)==P1（非过期）。
- **AC-32**：[spy/静态] 本系统对系统#8 调用==0。

---

## Test Evidence

**Story Type**: Logic
**Required evidence**: `tests/unit/property-ownership/ownership_snapshot_test.cpp`（类目 `Rento.Property.OwnershipSnapshot`）— 存在且通过。AC-30b 值拷贝隔离须变异坐实。
**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001（容器）、Story 004（谓词）
- Unlocks: None
