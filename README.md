你的任务是：利用你的计算机的**CPU**，**在保证计算结果正确的前提下，让一个矩阵乘法 + 激活函数程序跑得更快**。

---

## 1. 题目描述

先做一次**矩阵乘法** `C = A × B`，再对结果矩阵 `C` 逐个元素做一次 **Sigmoid 激活函数**。

程序会用两种实现算同一件事，然后对比运行时间。

## 2. 编译

cmake -B build -S .

cmake --build build


编译成功后，项目根目录下会出现一个叫 `run_matrix_multiplication` 的文件，这就是可以运行的程序。

## 3. 运行程序

这个程序有两种运行模式。

### 一：正确性验证

```Plain Text
./run_matrix_multiplication test
```

它会用一个较小的矩阵，检查你的优化版和基准版算出来的结果**是否一致**。看到绿色的 `Correctness passed!` 就说明结果正确。

> `./` 的意思是“运行当前目录下的这个程序”，不能省略。

### 二：性能测试

```Plain Text
./run_matrix_multiplication
```

它会用 4 组不同规模的矩阵分别计时，打印基准版和优化版的耗时，以及**加速比（Speedup）**。最后给出一个加权平均的总成绩。

> **注意**：性能测试中有的样例可能要跑好几分钟，请耐心等待。这正是你需要优化的原因。

---

## 4. 项目结构

```Plain Text
SRQ-26/
├── CMakeLists.txt              编译配置
├── README.md                     本说明文档
├── include/
│   ├── task.h                 矩阵乘法函数声明
│   └── activation.h           Sigmoid 函数声明
├── src/
│   ├── main.c                 主程序：矩阵定义、计时、正确性验证
│   └── activ_baseline.c       Sigmoid 基准版
└── task/
    ├──task.c                  矩阵乘法，优化对象 ★
    └── activation.c           Sigmoid 激活，优化对象 ★
```
