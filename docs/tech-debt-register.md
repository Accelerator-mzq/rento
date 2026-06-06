# Tech Debt Register

> 记录已知技术债与延迟验证项。每条注明来源 story 与归属去向。

- **2026-06-06** (FF-003 异步加载纪律 + Deinitialize CancelHandle): AC-4 真磁盘 DA 的真 deferred-callback UAF 防护**完整验证**留 story-007 PIE 实测 — headless `-nullrhi` 无 tick、deferred completion delegate 不被 `FlushAsyncLoading` pump，transient 内存对象走同步完成路径，无法在 headless 可靠驱动「真异步加载中途 PIE Stop → 悬挂回调写已 GC 宿主」这一真实地雷场景。当前 FF-003 已覆盖 CancelHandle 调用（变异测试坐实）+ TWeakObjectPtr/bIsDeinitializing 双 guard 逻辑；真异步路径验证 = ADR-0001 Verification ③，归 story-007（PIE 隔离验证）。来源 story: production/epics/foundation-framework/story-003-async-load-cancel-handle.md
