# SRQ-26 实验记录

> 每条实验同时写入本文件和 `LEDGER.tsv`。记录两侧原始数据、旁置条件与推理链，不只记录结论。

## 条目模板

```markdown
## EXP-XXX: 一句话描述改动（算子家族：1-8）

- **动机 / 要判别的两个解释**：
- **改动**：
- **构建与环境**：机器/节点、实际 cpuset、编译器与版本、flags、openmp 设置、两侧 commit
- **测量设计**：
- **正确性**：测试命令、结果、最大误差（如可得）
- **两侧原始数字**：

  | 构型 | case 1 | case 2 | case 3 | case 4 | 加权 speedup |
  |---|---:|---:|---:|---:|---:|
  | 基线（commit / 配方） | | | | | |
  | 候选 | | | | | |

- **旁置条件**：为测试此变量还改变或关闭了什么？
- **推理链**：
- **Outcome**: keep / reject / neutral
```

---

## EXP-001: OpenBLAS reference replacement and fixed Task baseline

- **改动**：`src/main.c:multiply_reference()` 直接使用 OpenBLAS CBLAS `dgemm`；新增 `SRQ_SEED` 固定输入序列；新增远程硬件 microbench。
- **远端**：Intel Xeon Gold 6548Y+，2 socket，2 NUMA；`taskset -c 0-31`；GCC 13.3；`OMP_NUM_THREADS=32`、`OMP_DYNAMIC=false`、`OMP_PROC_BIND=close`、`OMP_PLACES=cores`。
- **构建**：OpenBLAS 0.3.28，用户目录 `/home/nvidia/Ziyoung/deps/openblas`；`DYNAMIC_ARCH=1 NO_AFFINITY=1 USE_OPENMP=0 NUM_THREADS=32`。
- **seed**：`SRQ_SEED=260919`。
- **commit**：`3dfb264af1316421b1cc3b7a05318c997bf325c1`。
- **基线定义**：下表中的 Task seconds 是最初 native `task` 实现的 5 次中位数，已经固定为后续成绩比较的 baseline。OpenBLAS 只生成 correctness oracle，不参与成绩计时或 speedup 分母。

| case | OpenBLAS oracle (不计时) (s) | 固定 native Task baseline (s) | baseline / candidate 计分口径 |
|---|---:|---:|---:|
| 1 | 0.012809 | 1.167841 | 0.011x |
| 2 | 0.035183 | 2.281671 | 0.015x |
| 3 | 0.667933 | 56.250248 | 0.012x |
| 4 | 0.376709 | 0.687851 | 0.548x |
| 加权 `2:2:2:4` | | **固定 baseline 数据写入 `baseline.tsv`** | candidate speedup = fixed native baseline / timed Task candidate |

五次运行四个 case 的最终输出均通过误差 `1e-6` 校验。固定 Task baseline 已写入 `experiments/EXP-001-openblas-reference-20260919/baseline.tsv`，后续候选只与该文件的实测中位数比较，不把时间常量注入计时或判分代码。microbench 输出：`memcpy_gib_s=8.850`、`omp_threads=32`、`matmul_gflop_s=63.799`、`checksum=0.008192`。详见 `experiments/EXP-001-openblas-reference-20260919/README.md`。
