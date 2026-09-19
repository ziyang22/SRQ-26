# SRQ-26 - 工作指南

> 红线见 `AGENTS.md`；搜索与实验方法见 `PLAYBOOK.md`。

## 1. 当前起点

当前代码是待优化基线：

- `task/task.c`：`i-k-j` 三重循环矩阵乘法；
- `task/activation.c`：缩放、偏置、Sigmoid 三趟循环；
- `src/main.c`：冻结的参考链、随机数据、正确性与性能 harness。

初始化阶段不预设硬件专属优化，也不继承其他赛题的性能结论。

## 2. 现状

| 指标 | 当前值 | 备注 |
|---|---|---|
| 正确性测试 | 待测 | `./run_matrix_multiplication test` |
| case 1 speedup | 待测 | 1024x4096x1024 |
| case 2 speedup | 待测 | 2048x2048x2048 |
| case 3 speedup | 待测 | 10240x1024x10240，5% 稀疏 |
| case 4 speedup | 待测 | 8192x8x8192 |
| 加权端到端 speedup | 待测 | 权重 2:2:2:4 |

目标由用户后续补充。在目标明确前，不自行发明目标值。

## 3. 远程节点与资源限制

项目直接在计算节点运行，不提交 DSUB、Slurm 或其他任务脚本。连接参数：

```sh
ssh -p 17255 nvidia@global.prd.ga.launchpad.nvidia.com
```

统一入口会将程序限制到 32 个逻辑 CPU，并设置 `OMP_NUM_THREADS=32`、`OMP_DYNAMIC=false`。使用远程入口前先探测节点：

```sh
scripts/remote-probe.sh
scripts/remote-run.sh test
scripts/remote-run.sh benchmark
```

远程运行脚本通过 GitHub `origin` 同步，只接受本地和远程均干净、已提交且已推送的 commit，并仅做 `fetch` + fast-forward 更新。它不使用 `sudo`，不安装依赖，不修改系统配置。私有仓库可显式设置 `SRQ_SSH_FORWARD_AGENT=true` 使用 SSH agent 转发；不得把 GitHub token 或私钥写入仓库。当前节点信息和高危操作红线见 `AGENTS.md`。

## 4. 构建与本地运行

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./run_matrix_multiplication test
./run_matrix_multiplication
```

也可以使用统一入口：

```sh
./run.sh test       # 配置、编译、正确性测试
./run.sh benchmark  # 配置、编译、四个正式性能用例
```

本地入口也支持 `OMP_NUM_THREADS`，但性能证据应在远程节点的 32 逻辑核限制下获取。远程节点当前没有 CMake，`run.sh` 会用 GCC 13.3、`-O3 -std=c99 -fopenmp` 直接构建同一源码清单；无需也不得用 `sudo` 安装 CMake。`CC`、`BUILD_DIR`、`BUILD_JOBS` 等环境变量可由实验显式设置；记录中必须写清实际值。

## 5. 代码、归档与 GitHub 同步

- 新的优化源码只能放在 `task/`；公共头文件放在 `include/`。
- 远程/本地辅助脚本放在 `scripts/`，实验配方和元数据放在 `experiments/`。
- 退出当前构建路径的实现放入 `archive/<实验编号或日期>/`，不要删除历史实现。
- 根目录只保留项目入口、规范文档和构建配置，不放临时源码或日志。
- `scripts/check-layout.sh` 用于检查目录布局。
- Git 提交前执行 `git diff --check`、`git status` 并确认没有秘密或构建产物。
- 远程 GitHub 地址由用户确认后再配置；同步使用普通 `fetch`、`pull --ff-only`、`push`，禁止 force push。

`git init`、首次 `git remote add`、首次 push 和任何会影响远程历史的操作都需要用户明确确认。

## 6. 每轮实验

1. 写清假设、对照变量和能够区分两种解释的测量。
2. 先运行 correctness，再进入性能测试。
3. 基线与候选分别构建；性能比较尽量置于同一资源分配中交错执行。
4. 保存四个 case 的两侧原始时间、speedup、线程和构建信息。
5. 同时更新 `OPTIMIZATIONS.md` 与 `LEDGER.tsv`。
6. 只有用户要求且仓库已经初始化 Git 时才提交 commit。

## 6. 停止条件

仅在以下条件之一成立时结束优化阶段：

1. 达到用户设定目标；
2. 可用机时或资源耗尽；
3. 按 `PLAYBOOK.md` 审计后，可证伪假设确实枯竭；
4. 用户明确暂停或改变任务。

## 7. 目录约定

```text
task/                 优化实现
src/                  harness 与参考实现（只读）
include/              公共接口与算子常量（默认只读）
AGENTS.md              硬约束和已知事实
CLAUDE.md              当前状态与执行流程
PLAYBOOK.md            搜索方法论
OPTIMIZATIONS.md       人类可读实验记录
LEDGER.tsv             结构化实验账本
run.sh                 本地统一入口
scripts/               远程探测、远程运行和布局检查脚本
experiments/            实验配方与运行元数据
archive/                退出当前构建路径的实现归档
submit/                后续冻结当前最优交付包（需要时创建）
```
