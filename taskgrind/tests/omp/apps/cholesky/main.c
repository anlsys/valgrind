#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#ifdef MPC
#include <mpc_omp_task_label.h>
#else
# define MPC_OMP_TASK_SET_LABEL(X)
#endif

#include "kernels.h"
#include "omp.h"

/* PARALLEL (tasks) - CHOLESKY FACTORIZATION */
static void
cholesky_par(
        const int ts,
        const int nt,
        double * A[nt][nt],
        double * B,
        double * C[nt])
{
    for (int k = 0; k < nt; k++)
    {
        MPC_OMP_TASK_SET_LABEL("POTRF");
        #pragma omp task depend(out: A[k][k])
        {
            potrf(A[k][k], ts, ts);
        }
        for (int i = k + 1; i < nt; i++)
        {
            MPC_OMP_TASK_SET_LABEL("TRSM");
            # pragma omp task depend(in: A[k][k]) depend(out: A[k][i])
            {
                trsm(A[k][k], A[k][i], ts, ts);
            }
        }
        for (int i = k + 1; i < nt; i++)
        {
            for (int j = k + 1; j < i; j++)
            {
                MPC_OMP_TASK_SET_LABEL("GEMM");
                # pragma omp task depend(in: A[k][i], A[k][j]) depend(out: A[j][i])
                {
                    gemm(A[k][i], A[k][j], A[j][i], ts, ts);
                }
            }
            MPC_OMP_TASK_SET_LABEL("SYRK");
            # pragma omp task depend(in: A[k][i]) depend(out: A[i][i])
            {
                syrk(A[k][i], A[i][i], ts, ts);
            }
        }
    }
}

/* SEQUENTIAL - CHOLESKY FACTORIZATION */
static void
cholesky_seq(
        const int ts,
        const int nt,
        double * A[nt][nt],
        double * B,
        double * C[nt])
{
    for (int k = 0; k < nt; k++)
    {
        potrf(A[k][k], ts, ts);
        for (int i = k + 1; i < nt; i++)
        {
            trsm(A[k][k], A[k][i], ts, ts);
        }
        for (int i = k + 1; i < nt; i++)
        {
            for (int j = k + 1; j < i; j++)
            {
                gemm(A[k][i], A[k][j], A[j][i], ts, ts);
            }
            syrk(A[k][i], A[i][i], ts, ts);
	    }
    }
}

int
main(int argc, char ** argv)
{
    if (argc < 3)
    {
        printf("cholesky matrix_size block_size\n");
        return 1;
    }

    int  n = atoi(argv[1]); // matrix size
    int ts = atoi(argv[2]); // tile size
    const int nt = n / ts;
    double * A[nt][nt], * B, * C[nt], * Ans[nt][nt];

    /* MATRIX INITIALIZATION */
    for (int i = 0; i < nt; i++)
    {
        for (int j = 0; j < nt; j++)
        {
            A[i][j] = (double *) malloc(ts * ts * sizeof(double));
            Ans[i][j] = (double *) malloc(ts * ts * sizeof(double));
            initialize_tile(ts * ts, A[i][j]);
            memcpy(Ans[i][j], A[i][j], ts * ts * sizeof(double));
        }

        // diagonal dominante
        for (int k = 0 ; k < ts ; k++)
        {
            A[i][i][k * ts + k] = (double) n;
            Ans[i][i][k * ts + k] = (double) n;
        }
        C[i] = (double *) malloc(ts * ts * sizeof(double));
    }
    B = (double *) malloc(ts * ts * sizeof(double));

    # pragma omp parallel
    {
        # pragma omp single
        {
            int num_threads = omp_get_num_threads();
            printf("OpenMP num_threads = %d\n", num_threads);

            puts("Running sequential...");
            double t0 = omp_get_wtime();
            cholesky_seq(ts, nt, Ans, B, C);
            double t1 = omp_get_wtime();
            printf("sequential time : %lf s.\n", t1 - t0);

            puts("Running parallel...");
            cholesky_par(ts, nt, A, B, C);
            # pragma omp taskwait
            double t2 = omp_get_wtime();
            printf("parallel time : %lf s.\n", t2 - t1);
        }
    }

    /* Check */
    int wrong = 0;
    for (int i = 0; i < nt; i++)
    {
        for (int j = 0; j < nt; j++)
        {
            for (int k = 0; k < ts*ts; k++)
            {
                if (Ans[i][j][k] != A[i][j][k])
                {
                    wrong = 1;
                    break ;
                }
            }
        }
    }
    puts(wrong ? "ERROR IN PARALLEL FACTORIZATION" : "Parallel factorization is correct");

    /* FREE MEMORY */
    for (int i = 0; i < nt; i++)
    {
        for (int j = 0; j < nt; j++)
        {
            free(A[i][j]);
        }
        free(C[i]);
    }
    free(B);
    return 0;
}
