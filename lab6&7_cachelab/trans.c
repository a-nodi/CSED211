/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

/* 
    I accidently deleted the original file.
    I restored it as much as I can.
    :(
*/

void transpose_3232(int M, int N, int A[N][M], int B[M][N])
void transpose_6464(int M, int N, int A[N][M], int B[M][N])
void transpose_6167(int M, int N, int A[N][M], int B[M][N])

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    if (M == N && M == 32) // 32 x 32 matrix
        transpose_3232(M, N, A, B);
    else if (M == N && M == 64) // 64 x 64 matrix
        transpose_6464(M, N, A, B);
    else // 61 x 67 matrix
        transpose_6167(M, N, A, B);
    return;
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

void transpose_3232(int M, int N, int A[N][M], int B[M][N]){
    // Transpose 32 x 32 matrix A to 32 x 32 matrix B
    int i, j, i_, j_; // Index variables
    
    for(i = 0; i < N; i += 8){ // 8 -> 1 cache-line
        for (j = 0; j < M; j += 8) {
            // In-block operation
            for (i_ = 0; i_ < 8; i_++) {
                for (j_ = 0; j_ < 8; j_++) {
                    if (i + i_== j + j_) {
                        continue;
                    }
                B[j + j_][i + i_] = A[i + i_][j + j_];
            }
                if (i == j){
                    B[j + i_][i + i_] = A[i + i_][j + i_];
                }
            }
        }
    }

    return;
}

void transpose_6464(int M, int N, int A[N][M], int B[M][N]){
    // Transpose 64 x 64 matrix A to 64 x 64 matrix B
    int i, j, i_, j_; // Index variables
    for (i = 0; i < N; i += 8) { // 8 -> 1 cache-line
        for (j = 0; j < N; j += 8) {

            // In-block operation
            for (i_ = 0; i_ < 8 / 2; i_++) {
                for (j_ = 0; j_ < 8 / 2; j_++) {
                    if (i + i_== j + j_) {
                        continue;
                    }
                    B[j + j_][i + i_] = A[i + i_][j + j_];
                }
                if (i == j) {
                    B[j + i_][i + i_] = A[i + i_][j + i_];
                }
            }

            // In-block operation
            for (i_ = 8 / 2; i_ < 8; i_++) {
                for (j_ = 0; j_ < 8 / 2; j_++) {
                    if (i == j && i_ - j_ == 8 / 2) {
                        continue;
                    }
                    B[j + j_][i + i_] = A[i + i_][j + j_];
                }
                if (i == j) {
                    B[j + i_ - 8 / 2][i + i_] = A[i + i_][j + i_ - 8 / 2];
                }
            }

            // In-block operation
            for (i_ = 8 / 2; i_ < 8; i_++) {
                for (j_ = 8 / 2; j_ < 8; j_++) {
                    if (i + i_== j + j_) {
                        continue;
                    }
                    B[j + j_][i + i_] = A[i + i_][j + j_];
                }
                if (i == j) {
                    B[j + i_][i + i_] = A[i + i_][j + i_];
                }
            }

            // In-block operation
            for (i_ = 0; i_ < 8 / 2; i_++) {
                for (j_ = 8 / 2; j_ < 8; j_++) {
                    if (i == j && j_ - i_ == 8 / 2) {
                        continue;
                    }
                    B[j + j_][i + i_] = A[i + i_][j + j_];
                }
                if (i == j) {
                    B[j + i_ + 8 / 2][i + i_] = A[i + i_][j + i_ + 8 / 2];
                }
            }
        }
    }
    return;
}

void transpose_6167(int M, int N, int A[N][M], int B[M][N]){
    // Transpose 67 x 61 matrix A to 61 x 67 matrix B
    int i, j, i_, j_; // Index variable

    for(i = 0; i < N; i += 16){ // 16 -> 2 cache-line
        for(j = 0; j < M; j += 16){
            // In-block operation
            for(i_ = 0; i_ < 16 && i + i_ < N; i_++){
                for(j_ = 0; j_ < 16 && j + j_ < M; j_++){
                    if (i + i_ == j + j_ && i + i_ == 0){
                        continue;
                    }
                    B[j + j_][i + i_] = A[i + i_][j + j_];
                }
                if (i == j && i + i_ == 0){
                    B[0][0] = A[0][0];
                }
            }
        }
    }

    return;
}
