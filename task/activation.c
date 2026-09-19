#include "../include/activation.h"
#include <math.h>

/*
 * 尾处理链（epilogue）：epilogue(x) = sigmoid(x * scale + bias)
 */
void epilogue_optimized(double* data, size_t size)
{
    for (size_t i = 0; i < size; ++i)
        data[i] = data[i] * EPILOGUE_SCALE;
    for (size_t i = 0; i < size; ++i)
        data[i] = data[i] + EPILOGUE_BIAS;
    for (size_t i = 0; i < size; ++i)
        data[i] = 1.0 / (1.0 + exp(-data[i]));
}
