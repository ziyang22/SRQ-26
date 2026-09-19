#include <stddef.h>

/* Minimal CBLAS declaration so the experiment does not require system headers. */
extern void cblas_dgemm(const int order, const int trans_a, const int trans_b,
                        const int rows_a, const int cols_b, const int inner,
                        const double alpha, const double* a, const int lda,
                        const double* b, const int ldb, const double beta,
                        double* c, const int ldc);

void multiply_reference(const double* A, const double* B, double* C,
                        int M, int K, int N)
{
    cblas_dgemm(101, 111, 111, M, N, K, 1.0, A, K, B, N, 0.0, C, N);
}
