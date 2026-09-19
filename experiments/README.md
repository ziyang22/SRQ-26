# Experiments

存放具名实验配方、环境快照和原始结果摘要，不使用无编号的散落文本文件。每个实验一个子目录，命名为 `EXP-XXX/`（例如 `EXP-001/`），目录内可放：

```text
recipe.md     假设、变量、测量设计和执行命令
env.txt       机器、编译器、线程、cpuset、OMP 设置
raw/          原始 stdout/stderr 和计时输出
```

不在本目录放源码；源码只在 `task/`，废弃实现归档到 `archive/`。

## 必须记录的环境字段

性能结论只有配上这些字段才可解释：

- 节点与 CPU 型号、NUMA 数量；
- 实际使用的逻辑 CPU 集合（默认 `taskset -c 0-31`）；
- `OMP_NUM_THREADS`、`OMP_PROC_BIND`、`OMP_PLACES`、`OMP_DYNAMIC`；
- 编译器与版本、优化等级、链接库；
- 本地和远程的 commit 哈希。

结论同时写入 `OPTIMIZATIONS.md` 和 `LEDGER.tsv`。
