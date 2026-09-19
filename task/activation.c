#include "../include/activation.h"
#include <math.h>
#include <omp.h>

#if defined(__GNUC__) && defined(__x86_64__)
__attribute__((optimize("fast-math"), target("avx512f")))
#endif
static void epilogue_chunk(double* data, size_t first, size_t last)
{
#pragma omp simd
    for (size_t i = first; i < last; ++i) {
        const double x = data[i] * EPILOGUE_SCALE + EPILOGUE_BIAS;
        data[i] = 1.0 / (1.0 + exp(-x));
    }
}

/*
 * Tail processing chain: sigmoid(x * scale + bias).
 */
void epilogue_optimized(double* data, size_t size)
{
#pragma omp parallel
    {
        const size_t tid = (size_t)omp_get_thread_num();
        const size_t threads = (size_t)omp_get_num_threads();
        const size_t first = size * tid / threads;
        const size_t last = size * (tid + 1) / threads;
        epilogue_chunk(data, first, last);
    }
}
