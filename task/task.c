#include "../include/task.h"
#include <stdlib.h>
#include <string.h>

static int multiply_sparse_b(const double* A, const double* B, double* C,
                             int M, int K, int N)
{
    size_t* row_offsets = malloc((size_t)(K + 1) * sizeof(*row_offsets));
    if (row_offsets == NULL)
        return 0;

    row_offsets[0] = 0;
    for (int k = 0; k < K; ++k) {
        size_t count = 0;
        const double* b = B + (size_t)k * N;
        for (int j = 0; j < N; ++j)
            count += b[j] != 0.0;
        row_offsets[k + 1] = row_offsets[k] + count;
    }

    const size_t nnz = row_offsets[K];
    if (nnz * 5 > (size_t)K * N) {
        free(row_offsets);
        return 0;
    }

    int* columns = malloc(nnz * sizeof(*columns));
    double* values = malloc(nnz * sizeof(*values));
    if (columns == NULL || values == NULL) {
        free(values);
        free(columns);
        free(row_offsets);
        return 0;
    }

#pragma omp parallel for schedule(static)
    for (int k = 0; k < K; ++k) {
        size_t p = row_offsets[k];
        const double* b = B + (size_t)k * N;
        for (int j = 0; j < N; ++j) {
            if (b[j] != 0.0) {
                columns[p] = j;
                values[p] = b[j];
                ++p;
            }
        }
    }

#pragma omp parallel for schedule(static)
    for (int i = 0; i < M; ++i) {
        double* c = C + (size_t)i * N;
        memset(c, 0, (size_t)N * sizeof(double));
        for (int k = 0; k < K; ++k) {
            const double aik = A[(size_t)i * K + k];
            if (aik == 0.0)
                continue;
            for (size_t p = row_offsets[k]; p < row_offsets[k + 1]; ++p)
                c[columns[p]] += aik * values[p];
        }
    }

    free(values);
    free(columns);
    free(row_offsets);
    return 1;
}

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
    if (M >= 4096 && N >= 4096 && K <= 1024
        && multiply_sparse_b(A, B, C, M, K, N))
        return;

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
