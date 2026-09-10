# Architecture Decision Records

本目录只记录跨阶段长期有效、会约束后续实现的重要技术决定。普通讨论、临时尝试和实施步骤不写入 ADR。

文件命名：

```text
ADR-NNNN-short-topic.md
```

每份 ADR 包含以下固定部分：

```markdown
# ADR-NNNN: Decision Title

## Status
Proposed / Accepted / Superseded / Rejected

## Context
需要解决的问题、约束和证据。

## Decision
已经选择的方案和适用边界。

## Consequences
正面影响、代价、风险和后续约束。
```

已接受的 ADR 不静默改写；改变决定时创建新 ADR，并将旧记录标记为 `Superseded`。
