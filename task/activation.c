#include "../include/activation.h"
#include <stdint.h>

static inline double exp_negative(double x)
{
    const double log2e = 1.4426950408889634074;
    const double ln2_hi = 0.6931471805599453;
    const int n = (int)(x * log2e - 0.5);
    const double r = x - (double)n * ln2_hi;

    double p = 1.0 / 3628800.0;
    p = 1.0 / 362880.0 + r * p;
    p = 1.0 / 40320.0 + r * p;
    p = 1.0 / 5040.0 + r * p;
    p = 1.0 / 720.0 + r * p;
    p = 1.0 / 120.0 + r * p;
    p = 1.0 / 24.0 + r * p;
    p = 1.0 / 6.0 + r * p;
    p = 0.5 + r * p;
    p = 1.0 + r * p;
    p = 1.0 + r * p;

    union {
        uint64_t bits;
        double value;
    } scale = { (uint64_t)(n + 1023) << 52 };
    return scale.value * p;
}

/*
 * Epilogue: sigmoid(x * scale + bias). The saturation threshold keeps the
 * maximum absolute error below 1e-6 while avoiding expensive exp calls.
 */
void epilogue_optimized(double* data, size_t size)
{
#pragma omp parallel for simd schedule(static)
    for (size_t i = 0; i < size; ++i) {
        const double x = data[i] * EPILOGUE_SCALE + EPILOGUE_BIAS;
        if (x >= 14.0) {
            data[i] = 1.0;
        } else if (x <= -14.0) {
            data[i] = 0.0;
        } else {
            const double e = exp_negative(x < 0.0 ? x : -x);
            data[i] = x < 0.0 ? e / (1.0 + e) : 1.0 / (1.0 + e);
        }
    }
}
