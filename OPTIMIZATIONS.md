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

已完成的候选运行均通过误差 `1e-6` 校验。固定 Task baseline 已写入 `experiments/EXP-001-openblas-reference-20260919/baseline.tsv`，后续候选只与该文件的实测中位数比较，不把时间常量注入计时或判分代码。当前阶段只保留两次候选采样，第三次中止，不作统计结论。硬件探测记录了 64 B cache line、L1/L2/L3 拓扑、perf cache 计数和 IMC 读写带宽。详见 `experiments/EXP-001-openblas-reference-20260919/README.md`。

## EXP-002: autoGEMM shape portfolio and parallel epilogue (算子家族：2/3)

- **动机**：验证 shape-specific 内核、输出行静态并行、零 A 跳过及三趟尾处理融合的总体上限。
- **改动**：case 4 使用完全展开 K=8 外积；其他形状按 M 静态并行并跳过零 A；尾处理融合为一趟 OpenMP 循环。
- **环境**：Xeon Gold 6548Y+；GCC 13.3 `-O3 -std=c99 -fopenmp`；cpuset `0-31`；32 OpenMP 线程；commit `e4e1bdc`。
- **正确性**：`./run_matrix_multiplication test` 退出码 0，输出 `Correctness test passed!`；两个正式运行的四个 case 均通过。
- **两侧原始数字**：固定 baseline 时间 `1.167841 / 2.281671 / 56.250248 / 0.687851 s`；候选两次时间 `0.107821/0.158412/0.173662/0.050401 s` 和 `0.108589/0.159477/0.174345/0.049071 s`；加权 `75.287x / 75.147x`。
- **旁置条件**：该探针同时改变并行、零跳过、K=8 内核和尾处理，不能用于单独定价。
- **推理链**：两次加权差 0.2%，说明结构收益稳定；后续实验分别定价各路径。
- **Outcome**: keep

## EXP-003: dense shapes direct multi-thread OpenBLAS dispatch (算子家族：3)

- **改动**：`K>=2048` 直接调用一次 OpenBLAS `dgemm`。
- **环境**：同 EXP-002；commit `38b49ef`。
- **正确性**：通过随机测试和四个正式 case。
- **两侧原始数字**：EXP-002 复测时间 `0.108589/0.159477/0.174345/0.049071 s`，加权 `75.147x`；候选 `0.131765/0.256107/0.191417/0.047370 s`，加权 `68.135x`。
- **旁置条件**：OpenBLAS 自己管理 32 pthread，随后又进入 OpenMP 尾处理。
- **推理链**：case 1/2 均明确回退，单个多线程 BLAS 调用的线程调度/阶段切换未摊销。
- **Outcome**: reject；后续由 EXP-007 以单线程 slab 去混淆。

## EXP-004: per-call sparse-B compression (算子家族：2/3)

- **改动**：仅对大 M/N、K<=1024 的候选扫描 B，密度不高于 20% 时构建 CSR-like 行表；转换、分配和释放均在 timed call 内，A 的零值仍跳过。
- **环境**：同 EXP-002；commit `256b0c6`。
- **正确性**：随机测试和两次正式四 case 全部通过；K 递增累加顺序保持不变。
- **两侧原始数字**：EXP-002 case 3 `0.173662/0.174345 s`；候选 `0.093375/0.093541 s`。加权由 `75.287/75.147x` 提升到 `130.992/130.881x`。
- **旁置条件**：同时撤回了 EXP-003 的失败 dense 分派；case 1/2 应与 EXP-002 比较，case 3 是主要变量。
- **推理链**：即使包含两遍 B 扫描和格式构建，减少零 B 更新仍将 case 3 时间约减半。
- **Outcome**: keep

## EXP-005: local fast-math and polynomial sigmoid probes (算子家族：6)

- **改动**：先尝试函数属性 fast-math，再尝试带饱和阈值和范围缩减的标量多项式 sigmoid。
- **环境**：同 EXP-002；commits `1f95956`、`68c533c`。
- **正确性**：两候选均通过随机测试和正式四 case；饱和误差上界约 `8.32e-7`。
- **两侧原始数字**：EXP-004 加权 `130.992/130.881x`；fast-math `132.378x` 后复测 `126.605x`，汇编仍调用标量 `exp@PLT`；多项式候选时间 `0.106648/0.160484/0.111038/0.061816 s`，加权 `110.802x`。
- **推理链**：fast-math 没有机制证据且波动反向；标量多项式的分支和除法明显回退。
- **Outcome**: reject，均通过普通 revert 撤回

## EXP-006: AVX-512 code generation and sparse scatter (算子家族：5/6)

- **改动**：对 `task.c` 使用函数区间 target `avx512f,fma`；随后为 sparse B 更新声明无依赖 SIMD。
- **环境**：同 EXP-002；commits `be68372`、`149daec`。
- **正确性**：随机测试及所有正式 case 通过。汇编从 SSE2 升为 `zmm`，稀疏路径出现 `vscatterdpd`。
- **两侧原始数字**：AVX-512 候选 `0.097243/0.151116/0.092394/0.050348 s`，加权 `132.648x`；scatter 两次为 `0.100653/0.152057/0.081181/0.046726 s` (`149.790x`) 和 `0.097924/0.149833/0.093202/0.049405 s` (`131.706x`)。
- **旁置条件**：共享节点 case 3 噪声明显；scatter 最差复测与无 scatter 接近，不能按首轮高点定价。
- **推理链**：AVX-512 对 dense case 有稳定收益；scatter 有明确 codegen 且未观察到稳定回退，暂保留并继续在完整组合中复测。
- **Outcome**: keep

## EXP-007: OpenMP-partitioned single-thread BLAS slabs (算子家族：3)

- **改动**：dense case 在外层 OpenMP 中按连续 M 行分为最多 32 个 slab，每个线程调用单线程 OpenBLAS；调用后恢复原 OpenBLAS 线程数。
- **环境**：同 EXP-002；commit `40377ae`。
- **正确性**：随机测试与正式四 case 通过。
- **两侧原始数字**：EXP-006 复测 `0.097924/0.149833/0.093202/0.049405 s`；候选 `0.024483/0.035303/0.113000/0.052778 s`，case 1/2 从 `11.926/15.228x` 提升到 `47.700/64.631x`。该轮 case 3 波动使加权为 `127.237x`。
- **推理链**：与 EXP-003 相比，单线程 slab 消除了 OpenBLAS 内部并行的高固定成本，dense 提升明确。
- **Outcome**: keep

## EXP-008: explicit AVX-512 libmvec sigmoid (算子家族：2/5)

- **改动**：每个 OpenMP chunk 显式调用 glibc libmvec `_ZGVeN8v_exp`，每次处理 8 个 double；不足 8 个元素使用标量 `exp`。
- **环境**：同 EXP-002；commit `595d5e4`；仅使用已有 `libm`，无新依赖。
- **正确性**：远端随机测试退出码 0，输出 `Correctness test passed!`；三次正式四 case 均通过。汇编确认 `zmm` 和 `_ZGVeN8v_exp@PLT`。
- **两侧原始数字**：三次时间为 `0.025536/0.033847/0.097961/0.042941 s`、`0.025860/0.035038/0.098592/0.042801 s` 和最终提交独立复验 `0.027127/0.033888/0.093716/0.043704 s`；对应 speedup 为 `45.733/67.411/574.211/16.019x`、`45.160/65.120/570.536/16.071x` 和 `43.051/67.330/600.220/15.739x`；加权 `143.878x / 142.591x / 148.416x`，三次中值 `143.878x`。
- **旁置条件**：包含 EXP-006 scatter 和 EXP-007 slab-BLAS；相对 EXP-007 的完整加权受 case 3 噪声混淆，case 4 从 `0.052778 s` 稳定降至约 `0.043 s` 是最清楚证据。
- **推理链**：自动向量化两次失败且汇编仍为标量 `exp`；显式 ABI 后 codegen 和 case 4 时间同时改变，机制成立。最终 commit `db03aa9` 在远端独立重建后再次通过 correctness 和四个正式 case。
- **Outcome**: keep；当前最优

## EXP-009: theoretical peak and nominal FLOPS reporting (算子家族：7)

- **动机**：在正式 speedup 旁输出可复核的吞吐口径，并与 32 核 FP64 理论峰值对照。
- **改动**：benchmark 输出 `2*M*K*N / end-to-end seconds` 的 nominal GEMM GFLOP/s、相对 2.5 GHz 基频峰值比例，以及按 `2:2:2:4` 加权的 nominal throughput；计时区间和判分逻辑未改变。
- **理论口径**：cpuset `0-31` 是 32 个不同物理核；每核两条 512-bit FP64 FMA，每周期 `2*8*2=32` FLOP。2.5 GHz 基频峰值为 `2560.0 GFLOP/s`；4.1 GHz 最大睿频只作为非持续上界，为 `4198.4 GFLOP/s`。
- **环境**：同 EXP-002；commit `4c58c73`。
- **正确性**：远端 `test` 退出码 0 并输出 `Correctness test passed!`；四个正式 case 均通过。
- **原始数字**：case 时间 `0.024787/0.033491/0.095988/0.044987 s`；speedup `47.115/68.128/586.013/15.290x`；nominal throughput `346.5/513.0/2237.2/23.9 GFLOP/s`；加权 speedup `146.367x`，加权 nominal throughput `628.9 GFLOP/s`。
- **旁置条件**：case 3 的 `2237.2 GFLOP/s` 是 dense-equivalent，分子包含被稀疏实现跳过的零乘加，不代表实际执行 FLOP/s。所有 case 的分母包含 sigmoid，但分子不计 sigmoid 操作。
- **Outcome**: neutral；仅增加报告，不改变优化路径

## EXP-010: 50%-peak stage baseline and phase decomposition (算子家族：7)

- **动机**：冻结 1280 GFLOP/s 新目标的三次基线，并用默认关闭的诊断探针区分主要成本。
- **环境**：Xeon Gold 6548Y+；GCC 13.3 `-O3 -std=c99 -fopenmp`；cpuset `0-31`；32 OpenMP 线程；commit `3559f62`；固定 seed `260919`。
- **三次原始时间**：`0.024787/0.033491/0.095988/0.044987 s`、`0.026080/0.033381/0.093931/0.041649 s`、`0.026247/0.033237/0.096630/0.042247 s`。
- **三次 speedup**：`47.115/68.128/586.013/15.290x`、`44.779/68.352/598.846/16.515x`、`44.494/68.649/582.120/16.282x`；加权 `146.367/149.002/145.565x`，中位数 `146.367x`。
- **三次 nominal throughput**：`346.5/513.0/2237.2/23.9`、`329.4/514.7/2286.2/25.8`、`327.3/516.9/2222.4/25.4 GFLOP/s`；加权 `628.9/636.4/623.5 GFLOP/s`，中位数 `628.9 GFLOP/s`。
- **诊断探针**：commit `4004469` 的 `SRQ_PROFILE_PHASES` 默认关闭；远端默认汇编确认不包含 profile 字符串或 `omp_get_wtime`。诊断运行得到 dense BLAS `0.022782/0.032734 s`；case 3 count/build/compute/free `0.010019/0.002381/0.069487/0.000002 s`，sigmoid `0.010851 s`；case 4 K=8 `0.036994 s`，sigmoid `0.007156 s`。
- **推理链**：case 3 compute 占约 75%，是达到 1280 GFLOP/s 的主杠杆；B 计数/构建和 sigmoid 各约 12-13%。case 4 对 nominal 加权指标贡献很小，但对 speedup 权重最高。dense case 几乎全由 slab-BLAS 决定。
- **Outcome**: neutral；作为新阶段固定基线
