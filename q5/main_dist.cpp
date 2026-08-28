#include <iostream>
#include <vector>
#include <algorithm>
#include <mpi.h>

using namespace std;

const int INF = 1000000;

struct RankTime{
    double val;
    int rank;
};

void get_input(int &n, int &m, vector<int> &dist){
    cin >> n >> m; dist.assign(n*n,INF);

    for(int i=0; i<n; i++) dist[i*n + i] = 0;

    for(int i=0; i<m; i++){
        int u,v,w; cin >> u >> v >> w;
        dist[u*n + v] = w;
    }
}

void print_matrix(const vector<int> &matrix, int n){
    for(int i=0; i<n; i++){
        for(int j=0; j<n; j++){
            cout << matrix[i*n + j] << " ";
        }
        cout << endl;
    }
}

int main(int argc,char** argv){

    MPI_Init(&argc,&argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD,&rank); MPI_Comm_size(MPI_COMM_WORLD,&size);

    int n, m; vector<int> dist;
    if(rank == 0){
        get_input(n,m,dist);
    }


    MPI_Bcast(&n,1,MPI_INT,0,MPI_COMM_WORLD);


    vector<int> num_rows(size);     // stores the number of rows assigned to each process
    vector<int> ind(size);          // displacements for scattering/gathering

    int rem = n % size;

    for(int i=0; i<size; i++){
        num_rows[i] = n/size;
        if(i < rem) num_rows[i]++;
        
        if(i==0){
            ind[0] = 0;
        }
        else{
            ind[i] = ind[i-1] + num_rows[i-1];
        }
    }


    int my_num_rows = num_rows[rank];
    vector<int> my_dist(my_num_rows*n);

    vector<int> cnts(size); // number of elements to send to each process

    for(int i=0; i<size; i++){
        cnts[i] = num_rows[i] * n;
        ind[i] *= n;
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double t_scatter_start = MPI_Wtime();

    MPI_Scatterv(dist.data(),cnts.data(),ind.data(),MPI_INT,my_dist.data(),my_num_rows * n,MPI_INT,0,MPI_COMM_WORLD);

    double comm_time = MPI_Wtime() - t_scatter_start;
    double compute_time = 0.0;

    vector<int> row_k(n);

    for(int k=0; k<n; k++){

        int owner = 0;

        while(k >= ind[owner]/n + num_rows[owner]) owner++;

        if(rank == owner){
            int local_index = k - ind[rank] / n;
            for(int j=0; j<n; j++){
                row_k[j] = my_dist[local_index*n + j];
            }
        }

        double t_bcast_start = MPI_Wtime();
        MPI_Bcast(row_k.data(),n,MPI_INT,owner,MPI_COMM_WORLD);
        comm_time += MPI_Wtime() - t_bcast_start;

        double t_compute_start = MPI_Wtime();
        for(int i=0; i<my_num_rows; i++) {
            if(my_dist[i*n + k] == INF) continue;
            for(int j=0; j<n; j++){
                if(row_k[j] == INF) continue;
                my_dist[i*n + j] = min(my_dist[i*n + j],my_dist[i*n + k] + row_k[j]);
            }
        }
        compute_time += MPI_Wtime() - t_compute_start;
    }

    vector<int> result;
    if(rank == 0){
        result.resize(n*n);
    }

    double t_gather_start = MPI_Wtime();
    MPI_Gatherv(my_dist.data(),my_num_rows * n,MPI_INT,result.data(),cnts.data(),ind.data(),MPI_INT,0,MPI_COMM_WORLD);
    comm_time += MPI_Wtime() - t_gather_start;

    double local_total = comm_time + compute_time;

    // rank with the largest total time is the actual bottleneck for the run
    RankTime local_pair = {local_total, rank}, global_pair;
    MPI_Allreduce(&local_pair,&global_pair,1,MPI_DOUBLE_INT,MPI_MAXLOC,MPI_COMM_WORLD);

    double reported_comm = 0, reported_compute = 0, reported_total = 0;

    if(rank == global_pair.rank && rank != 0){
        MPI_Send(&comm_time,1,MPI_DOUBLE,0,0,MPI_COMM_WORLD);
        MPI_Send(&compute_time,1,MPI_DOUBLE,0,1,MPI_COMM_WORLD);
    }

    if(rank == 0){
        if(global_pair.rank == 0){
            reported_comm = comm_time;
            reported_compute = compute_time;
        }
        else{
            MPI_Recv(&reported_comm,1,MPI_DOUBLE,global_pair.rank,0,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
            MPI_Recv(&reported_compute,1,MPI_DOUBLE,global_pair.rank,1,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
        }
        reported_total = reported_comm + reported_compute;
    }

    if(rank == 0){
        print_matrix(result,n);
        cerr << "TIME_SECONDS " << reported_total << endl;
        cerr << "COMPUTE_SECONDS " << reported_compute << endl;
        cerr << "COMM_SECONDS " << reported_comm << endl;
        cerr << "NUM_PROCS " << size << endl;
    }

    MPI_Finalize();

    return 0;
}