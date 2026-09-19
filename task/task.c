#include "../include/task.h"
#include <string.h>

/*
 * 矩阵乘法：C = A * B
 *   A: M x K，B: K x N，C: M x N，均为行主序（row-major）。
 */
void multiply_naive(const double* A, const double* B, double* C,
                    int M, int K, int N)
{
    memset(C, 0, (size_t)M * N * sizeof(double));
    for (int i = 0; i < M; ++i) {
        for (int k = 0; k < K; ++k) {
            double aik = A[(size_t)i * K + k];
            for (int j = 0; j < N; ++j) {
                C[(size_t)i * N + j] += aik * B[(size_t)k * N + j];
            }
        }
    }
}
