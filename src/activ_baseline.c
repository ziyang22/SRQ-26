#include "../include/activation.h"
#include <math.h>

/**
 * @brief 尾处理链基线实现（正确性参考 + 性能基线）。
 *
 * epilogue(x) = sigmoid(x * scale + bias)
 *
 */
void epilogue_baseline(double* data, size_t size) {
    /* 第 1 趟：缩放 */
    for (size_t i = 0; i < size; ++i) {
        data[i] = data[i] * EPILOGUE_SCALE;
    }
    /* 第 2 趟：加偏置 */
    for (size_t i = 0; i < size; ++i) {
        data[i] = data[i] + EPILOGUE_BIAS;
    }
    /* 第 3 趟：Sigmoid 激活 */
    for (size_t i = 0; i < size; ++i) {
        data[i] = 1.0 / (1.0 + exp(-data[i]));
    }
}
