#ifndef ACTIVATION_H
#define ACTIVATION_H

#include <stddef.h>

// 尾处理链（epilogue），作用于 matmul 结果矩阵 C，逐元素计算：
//   epilogue(x) = sigmoid(x * EPILOGUE_SCALE + EPILOGUE_BIAS)
//              = 1 / (1 + exp(-(x * EPILOGUE_SCALE + EPILOGUE_BIAS)))

#define EPILOGUE_SCALE 0.5
#define EPILOGUE_BIAS  0.1

// 正确性参考 + 性能基线（禁止修改）
void epilogue_baseline(double* data, size_t size);

// 待实现：结果需与 epilogue_baseline 逐元素一致
void epilogue_optimized(double* data, size_t size);

#endif // ACTIVATION_H
