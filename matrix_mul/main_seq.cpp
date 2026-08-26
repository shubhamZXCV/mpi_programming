#include <iostream>
#include <vector>
#include <chrono>

using namespace std;

int main() {
    int m, n, p;

    // Read dimensions
    cin >> m >> n >> p;

    // Read matrix A (m x n)
    vector<int> A(m * n);
    for (int i = 0; i < m * n; i++) {
        cin >> A[i];
    }

    // Read matrix B (n x p)
    vector<int> B(n * p);
    for (int i = 0; i < n * p; i++) {
        cin >> B[i];
    }

    // Result matrix C (m x p)
    vector<int> C(m * p, 0);

    // ---- Timed region: matrix multiplication only ----
    auto t_start = chrono::high_resolution_clock::now();

    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            for (int k = 0; k < p; k++) {
                C[i * p + k] += A[i * n + j] * B[j * p + k];
            }
        }
    }

    auto t_end = chrono::high_resolution_clock::now();
    double elapsed = chrono::duration<double>(t_end - t_start).count();
    // ---------------------------------------------------

    // Print result to stdout (used for correctness checking)
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < p; j++) {
            cout << C[i * p + j];
            if (j != p - 1)
                cout << " ";
        }
        cout << "\n";
    }

    // Timing goes to stderr so it never pollutes the matrix output
    cerr << "TIME " << elapsed << endl;

    return 0;
}
