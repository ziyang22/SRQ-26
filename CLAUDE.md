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
| 正确性测试 | 通过 | 本地和远程 `taskset 0-31` 各验证一次 |
| case 1 speedup | 待测 | 1024x4096x1024 |
| case 2 speedup | 待测 | 2048x2048x2048 |
| case 3 speedup | 待测 | 10240x1024x10240，5% 稀疏 |
| case 4 speedup | 待测 | 8192x8x8192 |
| 加权端到端 speedup | 待测 | 权重 2:2:2:4，需在目标节点 32 逻辑核下测 |

本地 0.975x 的结果来自 AppleClang、未启用 OpenMP，不能作为成绩或基线结论。

第一条正式记录应是目标节点上的基线测量。目标由用户后续补充，在此之前不自行发明目标值。

## 3. 远程节点与资源限制

远程地址就是计算节点本身，直接在上面编译和运行，不提交 DSUB、Slurm 或其他任务脚本：

```sh
ssh -p 17255 nvidia@global.prd.ga.launchpad.nvidia.com
```

节点是双路 Intel Xeon Gold 6548Y+，2 个 NUMA 节点，共 64 物理核、128 逻辑 CPU；项目固定只用其中 **32 个逻辑 CPU**。远程入口绑定 `taskset -c 0-31`，并设置：

```text
OMP_NUM_THREADS=32
OMP_DYNAMIC=false
OMP_PROC_BIND=close
OMP_PLACES=cores
```

使用远程入口前先探测节点，并确认实际 cpuset 与线程数：

```sh
scripts/remote-probe.sh
scripts/remote-run.sh test
scripts/remote-run.sh benchmark
```

CPU 集合、线程数或绑定方式的变化属于实验变量，改变后必须重新测基线并在记录中注明。不得使用整机 128 个逻辑 CPU。

远程运行脚本通过公开 GitHub `origin` 同步，只接受本地和远程均干净、已提交且已推送的 commit，并仅做 `fetch` + fast-forward 更新。它不使用 `sudo`，不安装依赖，不修改系统配置。不得把 GitHub token 或私钥写入仓库。当前节点信息和高危操作红线见 `AGENTS.md`。

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

本地入口也支持 `OMP_NUM_THREADS`，但性能证据必须在远程节点的 32 逻辑核限制下获取。远程节点当前没有 CMake，`run.sh` 会用 GCC 13.3、`-O3 -std=c99 -fopenmp` 直接构建同一源码清单；无需也不得用 `sudo` 安装 CMake。`CC`、`BUILD_DIR`、`BUILD_JOBS` 等环境变量可由实验显式设置；记录中必须写清实际值。

## 5. 代码、归档与 GitHub 同步

- 新的优化源码只能放在 `task/`；公共头文件放在 `include/`。
- 远程/本地辅助脚本放在 `scripts/`，实验配方和元数据放在 `experiments/`。
- 退出当前构建路径的实现放入 `archive/<实验编号或日期>/`，不要删除历史实现。
- 根目录只保留项目入口、规范文档和构建配置，不放临时源码或日志。
- `scripts/check-layout.sh` 用于检查目录布局。
- Git 提交前执行 `git diff --check`、`git status` 并确认没有秘密或构建产物。
- 公开仓库地址：<https://github.com/ziyang22/SRQ-26>，远程机器从它拉取代码。
- 同步只用普通 `git fetch`、`git pull --ff-only`、`git push`；禁止 force push、覆盖远程历史或删除远程分支。
- GitHub 写操作（push、改设置、建仓库）只在本机用已配置的认证执行，不把凭据写入项目或日志。

涉及远程历史的非常规操作（force push、改默认分支、删除分支或仓库）必须先获得用户明确确认。

## 6. 每轮实验

1. 写清假设、对照变量和能够区分两种解释的测量。
2. 先运行 correctness，再进入性能测试。
3. 基线与候选分别构建；性能比较尽量置于同一资源分配中交错执行。
4. 保存四个 case 的两侧原始时间、speedup、线程和构建信息。
5. 同时更新 `OPTIMIZATIONS.md` 与 `LEDGER.tsv`。
6. 保留的有效改动及时 commit 并 push 到公开 `origin`；证据不足的改动标记为 reject 后再归档。

## 7. 停止条件

仅在以下条件之一成立时结束优化阶段：

1. 达到用户设定目标；
2. 可用机时或资源耗尽；
3. 按 `PLAYBOOK.md` 审计后，可证伪假设确实枯竭；
4. 用户明确暂停或改变任务。

## 8. 目录约定

```text
task/                 优化实现
src/                  harness 与参考实现（只读）
include/              公共接口与算子常量（默认只读）
README.md              项目入口说明
AGENTS.md              硬约束和已知事实
CLAUDE.md              当前状态与执行流程
PLAYBOOK.md            搜索方法论
OPTIMIZATIONS.md       人类可读实验记录
LEDGER.tsv             结构化实验账本
run.sh                 本地统一入口
scripts/               远程探测、远程运行和布局检查脚本
experiments/            实验配方与运行元数据
archive/                退出当前构建路径的实现归档
```

`submit/` 不是常驻目录；需要按 `PLAYBOOK.md` 冻结当前最优交付包时再创建。
