#include <mpi.h>
#include <iostream>
#include <vector>

using namespace std;

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int m, n, p;
    vector<int> A, B, C;

    // Rank 0 reads the input
    if (rank == 0) {
        cin >> m >> n >> p;

        A.resize(m * n);
        B.resize(n * p);

        for (int i = 0; i < m * n; i++) {
            cin >> A[i];
        }

        for (int i = 0; i < n * p; i++) {
            cin >> B[i];
        }

        C.resize(m * p);
    }

    // Broadcast matrix dimensions
    MPI_Bcast(&m, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&p, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Every process needs the complete matrix B
    if (rank != 0) {
        B.resize(n * p);
    }

    // Make sure every rank has finished reading input / resizing
    // before we start the timer, so I/O time isn't counted.
    MPI_Barrier(MPI_COMM_WORLD);
    double t_start = MPI_Wtime();

    MPI_Bcast(B.data(), n * p, MPI_INT, 0, MPI_COMM_WORLD);

    // --------------------------------------------------
    // Calculate how many rows of A each process gets
    // --------------------------------------------------

    int baseRows = m / size;
    int remainder = m % size;

    vector<int> rowCounts(size);
    vector<int> rowDisplacements(size);

    int offset = 0;

    for (int i = 0; i < size; i++) {
        rowCounts[i] = baseRows;

        if (i < remainder) {
            rowCounts[i]++;
        }

        rowDisplacements[i] = offset;
        offset += rowCounts[i];
    }

    int localRows = rowCounts[rank];

    // Convert row counts into element counts for matrix A
    vector<int> sendCountsA(size);
    vector<int> displacementsA(size);

    for (int i = 0; i < size; i++) {
        sendCountsA[i] = rowCounts[i] * n;
        displacementsA[i] = rowDisplacements[i] * n;
    }

    // Local portion of A
    vector<int> localA(localRows * n);

    // Scatter rows of A
    MPI_Scatterv(
        A.data(),
        sendCountsA.data(),
        displacementsA.data(),
        MPI_INT,
        localA.data(),
        localRows * n,
        MPI_INT,
        0,
        MPI_COMM_WORLD
    );

    // --------------------------------------------------
    // Compute local rows of C
    // --------------------------------------------------

    vector<int> localC(localRows * p, 0);

    for (int i = 0; i < localRows; i++) {
        for (int j = 0; j < n; j++) {
            for (int k = 0; k < p; k++) {
                localC[i * p + k] += localA[i * n + j] * B[j * p + k];
            }
        }
    }

    // --------------------------------------------------
    // Gather C back to rank 0
    // --------------------------------------------------

    vector<int> recvCountsC(size);
    vector<int> displacementsC(size);

    for (int i = 0; i < size; i++) {
        recvCountsC[i] = rowCounts[i] * p;
        displacementsC[i] = rowDisplacements[i] * p;
    }

    MPI_Gatherv(
        localC.data(),
        localRows * p,
        MPI_INT,
        C.data(),
        recvCountsC.data(),
        displacementsC.data(),
        MPI_INT,
        0,
        MPI_COMM_WORLD
    );

    double t_end = MPI_Wtime();

    // Rank 0 prints the result
    if (rank == 0) {
        for (int i = 0; i < m; i++) {
            for (int j = 0; j < p; j++) {
                cout << C[i * p + j];
                if (j != p - 1)
                    cout << " ";
            }
            cout << "\n";
        }
        // Timing goes to stderr so it never pollutes the matrix output.
        // Take the max elapsed time across ranks so stragglers count.
        double elapsed = t_end - t_start;
        double maxElapsed;
        MPI_Reduce(&elapsed, &maxElapsed, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
        cerr << "TIME " << maxElapsed << endl;
    } else {
        double elapsed = t_end - t_start;
        MPI_Reduce(&elapsed, nullptr, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    }

    MPI_Finalize();
    return 0;
}
