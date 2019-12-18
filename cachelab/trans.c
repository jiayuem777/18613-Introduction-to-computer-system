/*
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(size_t M, size_t N, double A[N][M], double B[M][N], double *tmp);
 * A is the source matrix, B is the destination
 * tmp points to a region of memory able to hold TMPCOUNT (set to 256) doubles as temporaries
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 2KB direct mapped cache with a block size of 64 bytes.
 *
 * Programming restrictions:
 *   No out-of-bounds references are allowed
 *   No alterations may be made to the source array A
 *   Data in tmp can be read or written
 *   This file cannot contain any local or global doubles or arrays of doubles
 *   You may not use unions, casting, global variables, or
 *     other tricks to hide array data in other forms of local or global memory.
 */
#include <stdio.h>
#include <stdbool.h>
#include "cachelab.h"
#include "contracts.h"

/* Forward declarations */
static bool is_transpose(size_t M, size_t N, const double A[N][M], const double B[M][N]);
static void trans(size_t M, size_t N, const double A[N][M], double B[M][N], double *tmp);
static void trans_tmp(size_t M, size_t N, const double A[N][M], double B[M][N], double *tmp);

/*
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded.
 */
static const char transpose_submit_desc[] = "Transpose submission";


static void transpose_submit(size_t M, size_t N, const double A[N][M], double B[M][N], double *tmp)
{
    /*
     * This is a good place to call your favorite transposition functions
     * It's OK to choose different functions based on array size, but
     * your code must be correct for all values of M and N
     */
    if (M == 32 && N == 32) {
        int i, j, k, l;
        for (i = 0; i < N; i += 8) {
            for (j = 0; j < M; j += 8) {
                for (k = j; k < j + 8; k++) {
                    for (l = i + k - j ; l >= i ; --l) {
                        B[k][l] = A[l][k];
                    }
                    for (l = i + k - j + 1 ; l < i + 8 ; ++l) {
                        B[k][l] = A[l][k];
                    }                   
                }
            }
        }
    } else if (M == 63 && N == 65) {
        int i, j, k, l;
        int size = 4;
        for (i = 0; i < N; i += size) {
            for (j = 0;  j < M; j += size) {
                for (k = i; k < i + size && k < N; k++) {
                    for (l = j; l < j + size && l < M; l++) {
                        B[l][k] = A[k][l];
                    }
                }
            }
        }
    } else {
        trans(M, N, A, B, tmp);
    }
    ENSURES(is_transpose(M, N, A, B));
}
/*
 * You can define additional transpose functions below. We've defined
 * some simple ones below to help you get started.
 */



/*
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
static const char trans_desc[] = "Simple row-wise scan transpose";

/*
 * The following shows an example of a correct, but cache-inefficient
 *   transpose function.  Note the use of macros (defined in
 *   contracts.h) that add checking code when the file is compiled in
 *   debugging mode.  See the Makefile for instructions on how to do
 *   this.
 *
 *   IMPORTANT: Enabling debugging will significantly reduce your
 *   cache performance.  Be sure to disable this checking when you
 *   want to measure performance.
 */
static void trans(size_t M, size_t N, const double A[N][M], double B[M][N], double *tmp)
{
    size_t i, j;

    REQUIRES(M > 0);
    REQUIRES(N > 0);

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            B[j][i] = A[i][j];
        }
    }

    ENSURES(is_transpose(M, N, A, B));
}

/*
 * This is a contrived example to illustrate the use of the temporary array
 */

static const char trans_tmp_desc[] =
    "Simple row-wise scan transpose, using a 2X2 temporary array";

static void trans_tmp(size_t M, size_t N, const double A[N][M], double B[M][N], double *tmp)
{
    size_t i, j;
    /* Use the first four elements of tmp as a 2x2 array with row-major ordering. */
    REQUIRES(M > 0);
    REQUIRES(N > 0);

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            int di = i%2;
            int dj = j%2;
            tmp[2*di+dj] = A[i][j];
            B[j][i] = tmp[2*di+dj];
        }
    }

    ENSURES(is_transpose(M, N, A, B));
}



/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions(void)
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc);

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc);
    registerTransFunction(trans_tmp, trans_tmp_desc);

}

/*
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
static bool is_transpose(size_t M, size_t N, const double A[N][M], const double B[M][N])
{
    size_t i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return false;
            }
        }
    }
    return true;
}

