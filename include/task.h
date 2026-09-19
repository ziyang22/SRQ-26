#ifndef TASK_H
#define TASK_H

// 朴素矩阵乘法：C = A * B
// A 为 M x K，B 为 K x N，C 为 M x N，均为行主序（row-major）存储
void multiply_naive(const double* A, const double* B, double* C, int M, int K, int N);

#endif // TASK_H
