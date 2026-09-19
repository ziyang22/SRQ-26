#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>
#include "../include/task.h"
#include "../include/activation.h"

// -----------------------------------------------------------
// 生成随机数，线性同余法（MINSTD / Park-Miller），返回 [0.0, 1.0)
//
// -----------------------------------------------------------
static double rand_lcg(int* localSeed)
{
    int high = *localSeed / 127773;
    int low = *localSeed % 127773;
    int test = 16807 * low - 2836 * high;
    int value = test + ((test > 0) ? 0 : 2147483647);
    *localSeed = value;
    return (double)value / 2147483647.0;
}

// 全局随机种子，在 main 中用 time 播种（使每次运行的数据不同）
static int g_rng_seed = 1;

// -----------------------------------------------------------
// Reference matmul: OpenBLAS CBLAS dgemm implementation.
// -----------------------------------------------------------
void multiply_reference(const double* A, const double* B, double* C,
                        int M, int K, int N);

// -----------------------------------------------------------
// 矩阵定义和基本操作
// -----------------------------------------------------------
double* create_matrix(int rows, int cols) {
    double* m = (double*)malloc(sizeof(double) * (size_t)rows * cols);
    if (!m) {
        fprintf(stderr, "Memory allocation failed!\n");
        exit(1);
    }
    // 用线性同余法生成有正有负的元素，范围 [-1.0, 1.0)，覆盖 sigmoid 两侧
    for (int i = 0; i < rows * cols; ++i) {
        m[i] = rand_lcg(&g_rng_seed) * 2.0 - 1.0;
    }
    return m;
}


double* create_sparse_matrix(int rows, int cols, double density) {
    double* m = (double*)malloc(sizeof(double) * (size_t)rows * cols);
    if (!m) {
        fprintf(stderr, "Memory allocation failed!\n");
        exit(1);
    }
    for (int i = 0; i < rows * cols; ++i) {
        double r = rand_lcg(&g_rng_seed);
        if (r < density) {
            m[i] = rand_lcg(&g_rng_seed) * 2.0 - 1.0;   // 非零元，范围 [-1.0, 1.0)
        } else {
            m[i] = 0.0;
        }
    }
    return m;
}

void free_matrix(double* m) {
    free(m);
}

// -----------------------------------------------------------
// 正确性验证
// -----------------------------------------------------------
// 完整算子链：C = epilogue(A * B)，其中 epilogue(x) = sigmoid(x*scale+bias)
void run_correctness_test() {
    printf("--- Running correctness test ---\n");

    // 随机挑选一组较小的 M,K,N，避免验证过慢
    const int M = 200 + (int)(rand_lcg(&g_rng_seed) * 301);
    const int K = 200 + (int)(rand_lcg(&g_rng_seed) * 301);
    const int N = 200 + (int)(rand_lcg(&g_rng_seed) * 301);

    printf("Testing matmul + epilogue: %d x %d  *  %d x %d  =  %d x %d\n",
           M, K, K, N, M, N);

    double *A = create_matrix(M, K);
    double *B = create_matrix(K, N);
    double *C_ref = create_matrix(M, N);
    double *C_opt = create_matrix(M, N);

    // 参考链：参考 matmul + 参考尾处理链
    multiply_reference(A, B, C_ref, M, K, N);
    epilogue_baseline(C_ref, (size_t)M * N);

    // 被测链：task.c 的 matmul + optimized 尾处理
    multiply_naive(A, B, C_opt, M, K, N);
    epilogue_optimized(C_opt, (size_t)M * N);

    // 逐元素比对
    int passed = 1;
    for (int i = 0; i < M * N; ++i) {
        if (fabs(C_ref[i] - C_opt[i]) > 1e-6) {
            printf("\033[31mCorrectness test failed at index %d (row=%d, col=%d)!\n",
                   i, i / N, i % N);
            printf("Reference: %f, Optimized: %f\033[0m\n", C_ref[i], C_opt[i]);
            passed = 0;
            break;
        }
    }

    if (passed) {
        printf("\033[32mCorrectness test passed!\033[0m\n");
    }
    printf("--------------------------------\n");

    free_matrix(A);
    free_matrix(B);
    free_matrix(C_ref);
    free_matrix(C_opt);
}

// -----------------------------------------------------------
// 性能基准（多组 case + 端到端计时）
// -----------------------------------------------------------
void run_benchmark() {
    printf("--- Running performance benchmark ---\n");

    const char* name_list[] = {"case 1", "case 2", "case 3", "case 4"};
    int    M_list[]       = {1024,  2048, 10240, 8192};
    int    K_list[]       = {4096,  2048,  1024,    8};
    int    N_list[]       = {1024,  2048, 10240, 8192};
    double density_list[] = { 0.0,   0.0,  0.05,  0.0};
    double weight_list[]  = { 2.0,   2.0,   2.0,  4.0};
    int num_cases = sizeof(M_list) / sizeof(M_list[0]);

    struct timeval start, end;
    double total_weight = 0.0, weighted_speedup = 0.0;

    for (int i = 0; i < num_cases; ++i) {
        int M = M_list[i], K = K_list[i], N = N_list[i];
        double density = density_list[i];
        size_t size = (size_t)M * N;
        printf("[%s] matmul + epilogue: %d x %d  *  %d x %d  =  %d x %d",
               name_list[i], M, K, K, N, M, N);
        if (density > 0.0) printf("  (density %.0f%%)", density * 100.0);
        printf("\n");

        double *A, *B;
        if (density > 0.0) {
            A = create_sparse_matrix(M, K, density);
            B = create_sparse_matrix(K, N, density);
        } else {
            A = create_matrix(M, K);
            B = create_matrix(K, N);
        }
        double *C_base = (double*)calloc(size, sizeof(double));
        double *C_opt  = (double*)calloc(size, sizeof(double));

        // baseline 管线端到端计时：参考 matmul + 参考尾处理链
        gettimeofday(&start, NULL);
        multiply_reference(A, B, C_base, M, K, N);
        epilogue_baseline(C_base, size);
        gettimeofday(&end, NULL);
        double t_base = (double)(end.tv_sec - start.tv_sec) +
                        (double)(end.tv_usec - start.tv_usec) / 1e6;

        // optimized 管线端到端计时
        gettimeofday(&start, NULL);
        multiply_naive(A, B, C_opt, M, K, N);
        epilogue_optimized(C_opt, size);
        gettimeofday(&end, NULL);
        double t_opt = (double)(end.tv_sec - start.tv_sec) +
                       (double)(end.tv_usec - start.tv_usec) / 1e6;

        double speedup = t_base / t_opt;
        printf("Baseline  (ref matmul + epilogue): %f s\n", t_base);
        printf("Optimized (task matmul + epilogue): %f s   (Speedup: %.3fx)\n", t_opt, speedup);

        weighted_speedup += speedup * weight_list[i];
        total_weight += weight_list[i];

        // 端到端正确性校验：两个管线的最终输出逐元素比对
        int passed = 1;
        for (size_t j = 0; j < size; ++j) {
            if (fabs(C_base[j] - C_opt[j]) > 1e-6) {
                printf("\033[31mMismatch at index %zu: baseline %f, optimized %f\033[0m\n",
                       j, C_base[j], C_opt[j]);
                passed = 0;
                break;
            }
        }
        if (passed) printf("\033[32mCorrectness passed!\033[0m\n");
        printf("--------------------------------\n");

        free_matrix(A);
        free_matrix(B);
        free_matrix(C_base);
        free_matrix(C_opt);
    }

    printf("\033[1;34mWeighted end-to-end speedup (2:2:2:4): %.3fx\033[0m\n",
           weighted_speedup / total_weight);
}

// -----------------------------------------------------------
// 主函数
// -----------------------------------------------------------
int main(int argc, char* argv[]) {
    // SRQ_SEED is optional; without it preserve the original time-based behavior.
    const char* seed_text = getenv("SRQ_SEED");
    if (seed_text != NULL && seed_text[0] != '\0') {
        char* end = NULL;
        long parsed = strtol(seed_text, &end, 10);
        if (*end != '\0' || parsed < 1 || parsed >= 2147483647L) {
            fprintf(stderr, "SRQ_SEED must be an integer in [1, 2147483646]\n");
            return 2;
        }
        g_rng_seed = (int)parsed;
    } else {
        g_rng_seed = (int)(time(NULL) % 2147483647);
        if (g_rng_seed <= 0) g_rng_seed = 1;
    }
    // Warm up the generator as in the original harness.
    for (int w = 0; w < 8; ++w) (void)rand_lcg(&g_rng_seed);

    if (argc > 1 && strcmp(argv[1], "test") == 0) {
        run_correctness_test();
    } else {
        run_benchmark();
    }
    return 0;
}
