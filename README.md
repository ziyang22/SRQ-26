# SRQ-26

CPU 矩阵乘法与 Sigmoid 尾处理优化项目。

## 任务

优化 `C = A × B` 以及逐元素 `sigmoid(C × 0.5 + 0.1)`，在保持正确性的前提下提高端到端 speedup。正式 benchmark 包含四组固定 case，最终指标为权重 `2:2:2:4` 的加权 speedup。

允许修改的实现位于 `task/`；`src/` 中的参考实现、测试和计时逻辑默认保持不变。详细规则见 [AGENTS.md](AGENTS.md)，实验流程见 [CLAUDE.md](CLAUDE.md) 和 [PLAYBOOK.md](PLAYBOOK.md)。

## 本地验证

```sh
./run.sh test
./run.sh benchmark
```

`run.sh` 优先使用 CMake；远程节点没有 CMake 时自动回退到 GCC 直接构建。性能结论以远程节点上的 32 逻辑 CPU 限制为准。

## 远程运行

```sh
scripts/remote-probe.sh
scripts/remote-run.sh test
scripts/remote-run.sh benchmark
```

远程节点通过 GitHub `origin` 同步，要求本地改动已经提交并推送，且本地和远程工作区干净。连接参数、硬件信息和安全限制见 [AGENTS.md](AGENTS.md)。

## 目录

```text
task/          优化实现
src/           参考实现、harness 和入口
include/       公共头文件
scripts/       远程运行和检查脚本
experiments/   实验配方与结果摘要
archive/       退出当前构建路径的代码归档
build/         临时构建产物，不提交
```

仓库地址：<https://github.com/ziyang22/SRQ-26>
