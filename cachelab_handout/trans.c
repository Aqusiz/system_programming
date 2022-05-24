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
	if (M <= 32 && N <= 32) {
		int i, j, k, l;

		for (i = 0; i < N; i+= 8) {
			for (j = 0; j < M; j+= 8) {
				for (k = i; k < i + 8; k++) {
					for (l = j; l < j + 8; l++) {
						if(k != l) B[l][k] = A[k][l];
					}
					if (i == j) B[k][k] = A[k][k];
				}
			}
		}
	}
	else if (M == 64 && N == 64) {
		// A0 A1 : A0, A1, A2, A3 -> 4x4 block
		// A2 A3
		int i, j, k, l;
		int buf0, buf1, buf2, buf3, buf4, buf5, buf6, buf7;

		for (i = 0; i < N; i += 8) {
			for (j = 0; j < M; j += 8) {
				for (k = i; k < i + 4; k++) {
					// buf A0
					buf0 = A[k][j];
					buf1 = A[k][j+1];
					buf2 = A[k][j+2];
					buf3 = A[k][j+3];
					// buf A1
					buf4 = A[k][j+4];
					buf5 = A[k][j+5];
					buf6 = A[k][j+6];
					buf7 = A[k][j+7];
					// B0 = A0^T
					B[j][k] = buf0;
					B[j+1][k] = buf1;
					B[j+2][k] = buf2;
					B[j+3][k] = buf3;
					// B1 = A1^T(reversed)
					B[j][k+4] = buf7;
					B[j+1][k+4] = buf6;
					B[j+2][k+4] = buf5;
					B[j+3][k+4] = buf4;
				}
				for (l = 0; l < 4; l++) {
					// buf A2(reversed)
					buf0 = A[i+4][j+3-l];
					buf1 = A[i+5][j+3-l];
					buf2 = A[i+6][j+3-l];
					buf3 = A[i+7][j+3-l];
					// buf A4
					buf4 = A[i+4][j+4+l];
					buf5 = A[i+5][j+4+l];
					buf6 = A[i+6][j+4+l];
					buf7 = A[i+7][j+4+l];
					// B2 = B1^T
					B[j+4+l][i] = B[j+3-l][i+4];
					B[j+4+l][i+1] = B[j+3-l][i+5];
					B[j+4+l][i+2] = B[j+3-l][i+6];
					B[j+4+l][i+3] = B[j+3-l][i+7];
					// B1 = A2^T
					B[j+3-l][i+4] = buf0;
					B[j+3-l][i+5] = buf1;
					B[j+3-l][i+6] = buf2;
					B[j+3-l][i+7] = buf3;
					// B3 = A3^T
					B[j+4+l][i+4] = buf4;
					B[j+4+l][i+5] = buf5;
					B[j+4+l][i+6] = buf6;
					B[j+4+l][i+7] = buf7;
				}
			}
		}
	}
	else {
		int i, j, k, l;
		
		for (i = 0; i < N; i+= 16) {
			for (j = 0; j < M; j+= 16) {
				for (k = i; (k < i + 16) && k < N; k++) {
					for (l = j; (l < j + 16) && l < M; l++) {
						B[l][k] = A[k][l];
					}
				}
			}
		}
	}
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

