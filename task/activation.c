#include "../include/activation.h"
#include <math.h>
#include <omp.h>
#ifdef SRQ_PROFILE_PHASES
#include <stdio.h>
#define PROFILE_NOW() omp_get_wtime()
#define PROFILE_PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PROFILE_NOW() 0.0
#define PROFILE_PRINT(...) ((void)0)
#endif

#if defined(__GNUC__) && defined(__x86_64__)
typedef double vector8d __attribute__((vector_size(64), aligned(1)));
extern vector8d _ZGVeN8v_exp(vector8d value);

__attribute__((target("avx512f")))
static void epilogue_chunk(double* data, size_t first, size_t last)
{
    const vector8d scale = { EPILOGUE_SCALE, EPILOGUE_SCALE,
                             EPILOGUE_SCALE, EPILOGUE_SCALE,
                             EPILOGUE_SCALE, EPILOGUE_SCALE,
                             EPILOGUE_SCALE, EPILOGUE_SCALE };
    const vector8d bias = { EPILOGUE_BIAS, EPILOGUE_BIAS,
                            EPILOGUE_BIAS, EPILOGUE_BIAS,
                            EPILOGUE_BIAS, EPILOGUE_BIAS,
                            EPILOGUE_BIAS, EPILOGUE_BIAS };
    const vector8d one = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };
    size_t i = first;
    for (; i + 8 <= last; i += 8) {
        vector8d x = *(const vector8d*)(data + i) * scale + bias;
        vector8d e = _ZGVeN8v_exp(-x);
        *(vector8d*)(data + i) = one / (one + e);
    }
    for (; i < last; ++i) {
        const double x = data[i] * EPILOGUE_SCALE + EPILOGUE_BIAS;
        data[i] = 1.0 / (1.0 + exp(-x));
    }
}
#else
static void epilogue_chunk(double* data, size_t first, size_t last)
{
    for (size_t i = first; i < last; ++i) {
        const double x = data[i] * EPILOGUE_SCALE + EPILOGUE_BIAS;
        data[i] = 1.0 / (1.0 + exp(-x));
    }
}
#endif

/*
 * Tail processing chain: sigmoid(x * scale + bias).
 */
void epilogue_optimized(double* data, size_t size)
{
    const double t_start = PROFILE_NOW();
#pragma omp parallel
    {
        const size_t tid = (size_t)omp_get_thread_num();
        const size_t threads = (size_t)omp_get_num_threads();
        const size_t first = size * tid / threads;
        const size_t last = size * (tid + 1) / threads;
        epilogue_chunk(data, first, last);
    }
    PROFILE_PRINT("PROFILE activation=%.6f size=%zu\n",
                  PROFILE_NOW() - t_start, size);
}
