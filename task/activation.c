#include "../include/activation.h"
#include <math.h>

#if defined(__GNUC__)
#pragma GCC push_options
#pragma GCC optimize("fast-math")
#endif

/*
 * 尾处理链（epilogue）：epilogue(x) = sigmoid(x * scale + bias)
 */
void epilogue_optimized(double* data, size_t size)
{
#pragma omp parallel for schedule(static)
    for (size_t i = 0; i < size; ++i) {
        const double x = data[i] * EPILOGUE_SCALE + EPILOGUE_BIAS;
        data[i] = 1.0 / (1.0 + exp(-x));
    }
}

#if defined(__GNUC__)
#pragma GCC pop_options
#endif
