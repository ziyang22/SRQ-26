# SRQ-26 - 硬约束

本文件只定义任务红线和从当前代码可确认的事实。工作流程见 `CLAUDE.md`，优化搜索方法见 `PLAYBOOK.md`。

## 任务定义

完整算子链为：

1. 行主序双精度矩阵乘法 `C = A * B`；
2. 逐元素尾处理 `sigmoid(C * 0.5 + 0.1)`。

允许修改的优化对象：

- `task/task.c` 中的 `multiply_naive()`；
- `task/activation.c` 中的 `epilogue_optimized()`；
- 如有必要，可新增仅由上述实现使用的源码，并同步更新 `CMakeLists.txt`。

除非用户明确改变规则，不得修改参考实现、测试数据、计时和判分逻辑：

- `src/main.c`
- `src/activ_baseline.c`
- `include/task.h`
- `include/activation.h`

不得硬编码答案、依赖随机种子、跳过计算、缓存与输入相关的跨调用结果，或放宽正确性标准。

## 正确性口径

官方口径由 `src/main.c` 定义：优化链与参考链逐元素比较，绝对误差不得超过 `1e-6`。

每次候选变更至少运行：

```sh
./run_matrix_multiplication test
```

保留程序退出码和输出中的 `Correctness test passed!` 作为最低正确性证据。性能测试还会对每个正式用例做端到端校验。

## 性能口径

正式 benchmark 包含四个用例：

| 用例 | M | K | N | 输入 | 权重 |
|---|---:|---:|---:|---|---:|
| case 1 | 1024 | 4096 | 1024 | 稠密 | 2 |
| case 2 | 2048 | 2048 | 2048 | 稠密 | 2 |
| case 3 | 10240 | 1024 | 10240 | 5% 稀疏 | 2 |
| case 4 | 8192 | 8 | 8192 | 稠密 | 4 |

最终指标是程序输出的 `Weighted end-to-end speedup (2:2:2:4)`。成绩包含矩阵乘法和尾处理的端到端时间；不得用自建 microbenchmark 替代正式成绩。

基线与候选比较时必须使用相同编译器、编译选项、线程环境和机器资源。当前仓库没有已确认的目标机拓扑、线程上限或 NUMA 结论，不得从其他项目照搬。

## 远程运行环境

本项目的性能运行目标是远程计算节点：

```text
ssh -p 17255 nvidia@global.prd.ga.launchpad.nvidia.com
```

不要把密码、私钥、known_hosts 内容或 GitHub token 写入仓库。推荐通过本机 SSH config、SSH agent 或环境变量提供认证。

当前已探测到的节点事实：

- Ubuntu 24.04 系列内核，x86_64；
- Intel Xeon Gold 6548Y+，2 socket，64 个物理核、128 个逻辑 CPU；
- 2 个 NUMA 节点；
- 约 1 TiB 内存、无 swap；
- GCC 13.3 和 `libgomp.so.1` 可用；当前未安装 CMake，`run.sh` 会回退到等价的 GCC 构建；
- 支持 AVX2、AVX-512 及 AMX 相关 CPU 特性。

以上硬件事实来自一次只读探测，重新连接后应使用 `scripts/remote-probe.sh` 更新，不得假定永久不变。

机器限制：本项目所有编译、正确性和性能命令都必须限制在 **32 个逻辑 CPU** 内。默认使用 `taskset`/OpenMP 约束，实际绑定结果必须在记录中注明。不得使用整机 128 个逻辑 CPU。

## 构建事实

- C99，默认 `Release` 构建。
- CMake 会探测 OpenMP；远程 GCC 环境应链接 `OpenMP::OpenMP_C`。
- 可执行文件输出到项目根目录：`run_matrix_multiplication`。
- 数学库 `libm` 是必要依赖。

编译器、优化级别、线程数或链接库的变化都属于实验变量，必须记录。

## 高危操作防护

虽然登录账户当前具备 `sudo` 能力，但 agent 默认不得执行以下操作：

- `sudo`、切换 root、修改用户/SSH/系统服务或安全策略；
- `rm -rf`、`git reset --hard`、`git clean`、强制 checkout、强制 push；
- 重启/关机、磁盘分区或格式化、挂载、修改网络、iptables、sysctl；
- 安装/卸载系统软件，修改 `/etc`、`/boot`、`/var/lib` 或其他系统目录；
- 杀死不属于本项目的进程，修改全局 CPU governor 或 NUMA 配置；
- 把秘密、令牌、私钥、远程认证信息写入仓库或日志。

清理只允许删除本项目明确生成的 `build/`、`*.out`、`*.err` 和 `jobs/.generated/` 内容；任何其他删除都必须先获得用户明确确认。远程命令优先使用只读探测或在项目目录内执行的构建命令。

## 代码与归档规范

目录职责固定如下：

- `include/`：公共头文件；
- `src/`：harness、参考实现和程序入口，默认只读；
- `task/`：唯一的优化实现目录；
- `scripts/`：可复用的本地/远程执行脚本，不放 C 源码；
- `experiments/`：实验配方、原始汇总和运行元数据；
- `archive/`：已经退出当前构建路径的代码或实验快照，只读归档；
- `build/`：临时构建产物，不提交 Git。

禁止在项目根目录、`scripts/`、`experiments/` 或 `archive/` 随意放置新的 C/C++ 源文件。新的优化源码只能进入 `task/` 并更新 `CMakeLists.txt`；废弃版本只能放进带日期/实验编号的 `archive/` 子目录，并在记录中说明来源。实验结果写入 `OPTIMIZATIONS.md`、`LEDGER.tsv` 或 `experiments/`，不能散落生成多个无名文本文件。

## Git 与 GitHub

本项目使用 Git 管理本地和远程 GitHub 仓库。提交前必须检查 `git diff`、`git status` 和敏感文件；不得提交构建产物、日志、密钥或机器本地配置。远程仓库 URL 和 GitHub 认证方式由用户后续提供或通过本机 Git credential/SSH agent 配置。

允许的同步动作是普通的 `git fetch`、`git pull --ff-only`、`git push`；首次配置远程仓库或执行 push 前需要用户明确提供/确认 GitHub 仓库地址。禁止 force push、覆盖远程历史或自动创建/删除远程仓库。

## 实验纪律

- 性能数据必须标注机器、编译器、线程数和关键环境变量。
- 小于 2% 的变化不得凭单次结果下结论；基线与候选应在同一资源分配内交错复测。
- 每次实验同时更新 `OPTIMIZATIONS.md` 和 `LEDGER.tsv`。
- 不得把未运行的初始化占位符写成实测结论。
