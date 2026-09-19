#include "../include/task.h"
#if defined(__GNUC__) && defined(__x86_64__)
#include <immintrin.h>
#endif
#include <omp.h>
#include <stdlib.h>
#include <string.h>
#ifdef SRQ_PROFILE_PHASES
#include <stdio.h>
#define PROFILE_NOW() omp_get_wtime()
#define PROFILE_PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PROFILE_NOW() 0.0
#define PROFILE_PRINT(...) ((void)0)
#endif

#ifndef SRQ_DENSE_CASE1_M_THREADS
#define SRQ_DENSE_CASE1_M_THREADS 4
#endif
#ifndef SRQ_DENSE_CASE1_N_THREADS
#define SRQ_DENSE_CASE1_N_THREADS 8
#endif
#ifndef SRQ_DENSE_CASE2_M_THREADS
#define SRQ_DENSE_CASE2_M_THREADS 16
#endif
#ifndef SRQ_DENSE_CASE2_N_THREADS
#define SRQ_DENSE_CASE2_N_THREADS 2
#endif
#ifndef SRQ_SPARSE_N_BLOCKS
#define SRQ_SPARSE_N_BLOCKS 1
#endif
#ifndef SRQ_SPARSE_SIMD
#define SRQ_SPARSE_SIMD 1
#endif
#ifndef SRQ_SPARSE_FLOAT_VALUES
#define SRQ_SPARSE_FLOAT_VALUES 0
#endif
#ifndef SRQ_SPARSE_UNROLL
#define SRQ_SPARSE_UNROLL 0
#endif

extern void cblas_dgemm(const int order, const int trans_a, const int trans_b,
                        const int rows_a, const int cols_b, const int inner,
                        const double alpha, const double* a, const int lda,
                        const double* b, const int ldb, const double beta,
                        double* c, const int ldc);
extern int openblas_get_num_threads(void);
extern void openblas_set_num_threads(int num_threads);

#if defined(__GNUC__) && defined(__x86_64__)
#pragma GCC push_options
#pragma GCC target("avx512f,fma")
#endif

#if SRQ_SPARSE_N_BLOCKS > 1
static int multiply_sparse_blocked(const double* A, const double* B, double* C,
                                   int M, int K, int N)
{
    const int blocks = SRQ_SPARSE_N_BLOCKS;
    const size_t stride = (size_t)K + 1;
    size_t* b_offsets = calloc((size_t)blocks * stride, sizeof(*b_offsets));
    size_t* a_offsets = malloc((size_t)(M + 1) * sizeof(*a_offsets));
    if (b_offsets == NULL || a_offsets == NULL) {
        free(a_offsets);
        free(b_offsets);
        return 0;
    }

    a_offsets[0] = 0;
    for (int i = 0; i < M; ++i) {
        size_t count = 0;
        const double* a = A + (size_t)i * K;
        for (int k = 0; k < K; ++k)
            count += a[k] != 0.0;
        a_offsets[i + 1] = a_offsets[i] + count;
    }

    for (int k = 0; k < K; ++k) {
        const double* b = B + (size_t)k * N;
        for (int j = 0; j < N; ++j) {
            if (b[j] != 0.0) {
                const int block = (int)((long long)j * blocks / N);
                ++b_offsets[(size_t)block * stride + k + 1];
            }
        }
    }

    size_t b_nnz = 0;
    for (int block = 0; block < blocks; ++block) {
        size_t* offsets = b_offsets + (size_t)block * stride;
        for (int k = 0; k < K; ++k) {
            const size_t count = offsets[k + 1];
            offsets[k] = b_nnz;
            b_nnz += count;
        }
        offsets[K] = b_nnz;
    }
    if (b_nnz * 5 > (size_t)K * N) {
        free(a_offsets);
        free(b_offsets);
        return 0;
    }

    const size_t a_nnz = a_offsets[M];
    int* a_columns = malloc(a_nnz * sizeof(*a_columns));
    double* a_values = malloc(a_nnz * sizeof(*a_values));
    int* b_columns = malloc(b_nnz * sizeof(*b_columns));
    double* b_values = malloc(b_nnz * sizeof(*b_values));
    if (a_columns == NULL || a_values == NULL
        || b_columns == NULL || b_values == NULL) {
        free(b_values);
        free(b_columns);
        free(a_values);
        free(a_columns);
        free(a_offsets);
        free(b_offsets);
        return 0;
    }

#pragma omp parallel for schedule(static)
    for (int i = 0; i < M; ++i) {
        size_t p = a_offsets[i];
        const double* a = A + (size_t)i * K;
        for (int k = 0; k < K; ++k) {
            if (a[k] != 0.0) {
                a_columns[p] = k;
                a_values[p] = a[k];
                ++p;
            }
        }
    }

#pragma omp parallel for schedule(static)
    for (int k = 0; k < K; ++k) {
        size_t positions[SRQ_SPARSE_N_BLOCKS];
        for (int block = 0; block < blocks; ++block)
            positions[block] = b_offsets[(size_t)block * stride + k];
        const double* b = B + (size_t)k * N;
        for (int j = 0; j < N; ++j) {
            if (b[j] != 0.0) {
                const int block = (int)((long long)j * blocks / N);
                const size_t p = positions[block]++;
                b_columns[p] = j;
                b_values[p] = b[j];
            }
        }
    }

#pragma omp parallel for schedule(static)
    for (int i = 0; i < M; ++i) {
        double* c = C + (size_t)i * N;
        memset(c, 0, (size_t)N * sizeof(double));
        for (int block = 0; block < blocks; ++block) {
            const size_t* offsets = b_offsets + (size_t)block * stride;
            for (size_t q = a_offsets[i]; q < a_offsets[i + 1]; ++q) {
                const int k = a_columns[q];
                const double aik = a_values[q];
#pragma omp simd
                for (size_t p = offsets[k]; p < offsets[k + 1]; ++p)
                    c[b_columns[p]] += aik * b_values[p];
            }
        }
    }

    free(b_values);
    free(b_columns);
    free(a_values);
    free(a_columns);
    free(a_offsets);
    free(b_offsets);
    return 1;
}
#endif

#if SRQ_SPARSE_UNROLL > 0 && !SRQ_SPARSE_FLOAT_VALUES
__attribute__((target("avx512f")))
static inline void sparse_axpy(double* c, const int* columns,
                               const double* values, size_t first, size_t last,
                               double aik)
{
    const __m512d va = _mm512_set1_pd(aik);
    size_t p = first;
#if SRQ_SPARSE_UNROLL >= 4
    for (; p + 32 <= last; p += 32) {
        const __m256i i0 = _mm256_loadu_si256((const __m256i*)(columns + p));
        const __m256i i1 = _mm256_loadu_si256((const __m256i*)(columns + p + 8));
        const __m256i i2 = _mm256_loadu_si256((const __m256i*)(columns + p + 16));
        const __m256i i3 = _mm256_loadu_si256((const __m256i*)(columns + p + 24));
        __m512d c0 = _mm512_i32gather_pd(i0, c, 8);
        __m512d c1 = _mm512_i32gather_pd(i1, c, 8);
        __m512d c2 = _mm512_i32gather_pd(i2, c, 8);
        __m512d c3 = _mm512_i32gather_pd(i3, c, 8);
        const __m512d b0 = _mm512_loadu_pd(values + p);
        const __m512d b1 = _mm512_loadu_pd(values + p + 8);
        const __m512d b2 = _mm512_loadu_pd(values + p + 16);
        const __m512d b3 = _mm512_loadu_pd(values + p + 24);
        c0 = _mm512_add_pd(c0, _mm512_mul_pd(va, b0));
        c1 = _mm512_add_pd(c1, _mm512_mul_pd(va, b1));
        c2 = _mm512_add_pd(c2, _mm512_mul_pd(va, b2));
        c3 = _mm512_add_pd(c3, _mm512_mul_pd(va, b3));
        _mm512_i32scatter_pd(c, i0, c0, 8);
        _mm512_i32scatter_pd(c, i1, c1, 8);
        _mm512_i32scatter_pd(c, i2, c2, 8);
        _mm512_i32scatter_pd(c, i3, c3, 8);
    }
#endif
#if SRQ_SPARSE_UNROLL >= 2
    for (; p + 16 <= last; p += 16) {
        const __m256i i0 = _mm256_loadu_si256((const __m256i*)(columns + p));
        const __m256i i1 = _mm256_loadu_si256((const __m256i*)(columns + p + 8));
        __m512d c0 = _mm512_i32gather_pd(i0, c, 8);
        __m512d c1 = _mm512_i32gather_pd(i1, c, 8);
        const __m512d b0 = _mm512_loadu_pd(values + p);
        const __m512d b1 = _mm512_loadu_pd(values + p + 8);
        c0 = _mm512_add_pd(c0, _mm512_mul_pd(va, b0));
        c1 = _mm512_add_pd(c1, _mm512_mul_pd(va, b1));
        _mm512_i32scatter_pd(c, i0, c0, 8);
        _mm512_i32scatter_pd(c, i1, c1, 8);
    }
#endif
    for (; p + 8 <= last; p += 8) {
        const __m256i index = _mm256_loadu_si256((const __m256i*)(columns + p));
        __m512d cv = _mm512_i32gather_pd(index, c, 8);
        const __m512d bv = _mm512_loadu_pd(values + p);
        cv = _mm512_add_pd(cv, _mm512_mul_pd(va, bv));
        _mm512_i32scatter_pd(c, index, cv, 8);
    }
    for (; p < last; ++p)
        c[columns[p]] += aik * values[p];
}
#endif

static int multiply_sparse_b(const double* A, const double* B, double* C,
                             int M, int K, int N)
{
    const double t_start = PROFILE_NOW();
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

    const double t_counted = PROFILE_NOW();
    const size_t nnz = row_offsets[K];
    if (nnz * 5 > (size_t)K * N) {
        free(row_offsets);
        return 0;
    }

    int* columns = malloc(nnz * sizeof(*columns));
#if SRQ_SPARSE_FLOAT_VALUES
    float* values = malloc(nnz * sizeof(*values));
#else
    double* values = malloc(nnz * sizeof(*values));
#endif
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
#if SRQ_SPARSE_FLOAT_VALUES
                values[p] = (float)b[j];
#else
                values[p] = b[j];
#endif
                ++p;
            }
        }
    }

    const double t_built = PROFILE_NOW();
#pragma omp parallel for schedule(static)
    for (int i = 0; i < M; ++i) {
        double* c = C + (size_t)i * N;
        memset(c, 0, (size_t)N * sizeof(double));
        for (int k = 0; k < K; ++k) {
            const double aik = A[(size_t)i * K + k];
            if (aik == 0.0)
                continue;
#if SRQ_SPARSE_UNROLL > 0 && !SRQ_SPARSE_FLOAT_VALUES
            sparse_axpy(c, columns, values, row_offsets[k], row_offsets[k + 1], aik);
#else
#if SRQ_SPARSE_SIMD
#pragma omp simd
#endif
            for (size_t p = row_offsets[k]; p < row_offsets[k + 1]; ++p)
                c[columns[p]] += aik * values[p];
#endif
        }
    }

    const double t_computed = PROFILE_NOW();
    free(values);
    free(columns);
    free(row_offsets);
    const double t_freed = PROFILE_NOW();
    PROFILE_PRINT("PROFILE sparse count=%.6f build=%.6f compute=%.6f free=%.6f nnz=%zu\n",
                  t_counted - t_start, t_built - t_counted,
                  t_computed - t_built, t_freed - t_computed, nnz);
    return 1;
}

static void multiply_dense_blas(const double* A, const double* B, double* C,
                                int M, int K, int N)
{
    const double t_start = PROFILE_NOW();
    const int m_threads = K >= 4096 ? SRQ_DENSE_CASE1_M_THREADS
                                    : SRQ_DENSE_CASE2_M_THREADS;
    const int n_threads = K >= 4096 ? SRQ_DENSE_CASE1_N_THREADS
                                    : SRQ_DENSE_CASE2_N_THREADS;
    const int saved_threads = openblas_get_num_threads();
    openblas_set_num_threads(1);
#pragma omp parallel num_threads(m_threads * n_threads)
    {
        const int tid = omp_get_thread_num();
        const int m_part = tid / n_threads;
        const int n_part = tid % n_threads;
        const int first_m = (int)((long long)M * m_part / m_threads);
        const int last_m = (int)((long long)M * (m_part + 1) / m_threads);
        const int first_n = (int)((long long)N * n_part / n_threads);
        const int last_n = (int)((long long)N * (n_part + 1) / n_threads);
        if (last_m > first_m && last_n > first_n) {
            cblas_dgemm(101, 111, 111, last_m - first_m, last_n - first_n, K,
                        1.0, A + (size_t)first_m * K, K,
                        B + first_n, N, 0.0,
                        C + (size_t)first_m * N + first_n, N);
        }
    }
    openblas_set_num_threads(saved_threads);
    PROFILE_PRINT("PROFILE dense_blas=%.6f M=%d K=%d N=%d\n",
                  PROFILE_NOW() - t_start, M, K, N);
}

static void multiply_k8(const double* A, const double* B, double* C,
                        int M, int N)
{
    const double t_start = PROFILE_NOW();
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
    PROFILE_PRINT("PROFILE k8=%.6f M=%d N=%d\n",
                  PROFILE_NOW() - t_start, M, N);
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
        multiply_dense_blas(A, B, C, M, K, N);
        return;
    }
    if (M >= 4096 && N >= 4096 && K <= 1024) {
#if SRQ_SPARSE_N_BLOCKS > 1
        if (multiply_sparse_blocked(A, B, C, M, K, N))
            return;
#else
        if (multiply_sparse_b(A, B, C, M, K, N))
            return;
#endif
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

#if defined(__GNUC__) && defined(__x86_64__)
#pragma GCC pop_options
#endif
