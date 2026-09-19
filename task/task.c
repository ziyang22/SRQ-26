#include "../include/task.h"
#include <string.h>

extern void cblas_dgemm(const int order, const int trans_a, const int trans_b,
                        const int rows_a, const int cols_b, const int inner,
                        const double alpha, const double* a, const int lda,
                        const double* b, const int ldb, const double beta,
                        double* c, const int ldc);

static void multiply_k8(const double* A, const double* B, double* C,
                        int M, int N)
{
#pragma omp parallel for schedule(static)
    for (int i = 0; i < M; ++i) {
        const double* a = A + (size_t)i * 8;
        double* c = C + (size_t)i * N;
        const double a0 = a[0], a1 = a[1], a2 = a[2], a3 = a[3];
        const double a4 = a[4], a5 = a[5], a6 = a[6], a7 = a[7];

#pragma omp simd
        for (int j = 0; j < N; ++j) {
            c[j] = a0 * B[j]
                 + a1 * B[(size_t)N + j]
                 + a2 * B[(size_t)2 * N + j]
                 + a3 * B[(size_t)3 * N + j]
                 + a4 * B[(size_t)4 * N + j]
                 + a5 * B[(size_t)5 * N + j]
                 + a6 * B[(size_t)6 * N + j]
                 + a7 * B[(size_t)7 * N + j];
        }
    }
}

/*
 * Matrix multiplication: C = A * B. All matrices are row-major.
 */
void multiply_naive(const double* A, const double* B, double* C,
                    int M, int K, int N)
{
    if (K == 8) {
        multiply_k8(A, B, C, M, N);
        return;
    }
    if (K >= 2048) {
        cblas_dgemm(101, 111, 111, M, N, K, 1.0, A, K, B, N, 0.0, C, N);
        return;
    }

#pragma omp parallel for schedule(static)
    for (int i = 0; i < M; ++i) {
        double* c = C + (size_t)i * N;
        memset(c, 0, (size_t)N * sizeof(double));
        for (int k = 0; k < K; ++k) {
            const double aik = A[(size_t)i * K + k];
            if (aik == 0.0)
                continue;
            const double* b = B + (size_t)k * N;
#pragma omp simd
            for (int j = 0; j < N; ++j)
                c[j] += aik * b[j];
        }
    }
}
