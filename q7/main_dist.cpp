#include <iostream>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <climits>
#include <cfloat>
#include <mpi.h>

using namespace std;

struct EntityStats{
    int id;
    long long count;
    double responseTimeSum;
    long long totalBytes;
};

struct RankTime{
    double val;
    int rank;
};

bool compareEntity(const EntityStats &a,const EntityStats &b){
    if(a.count != b.count) return a.count > b.count;
    return a.id < b.id;
}

int main(int argc,char** argv){
    MPI_Init(&argc,&argv);

    int rank,size;
    MPI_Comm_rank(MPI_COMM_WORLD,&rank); MPI_Comm_size(MPI_COMM_WORLD,&size);

    int n,k,s;
    vector<long long> timestamp, bytesSent;
    vector<int> serverId, endpointId, statusCode;
    vector<double> responseTime;

    if(rank == 0){
        cin >> n >> k >> s;
        timestamp.resize(n); bytesSent.resize(n);
        serverId.resize(n); endpointId.resize(n); statusCode.resize(n);
        responseTime.resize(n);

        for(int i=0; i<n; i++){
            int userId;
            cin >> timestamp[i] >> serverId[i] >> endpointId[i] >> userId >> statusCode[i] >> responseTime[i] >> bytesSent[i];
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double t_scatter_start = MPI_Wtime();

    int header[3];
    if(rank == 0){ header[0]=n; header[1]=k; header[2]=s; }
    MPI_Bcast(header,3,MPI_INT,0,MPI_COMM_WORLD);
    n = header[0]; k = header[1]; s = header[2];

    vector<int> num_rows(size), ind(size);
    int rem = n % size;

    for(int i=0; i<size; i++){
        num_rows[i] = n/size;
        if(i < rem) num_rows[i]++;

        if(i==0) ind[0] = 0;
        else ind[i] = ind[i-1] + num_rows[i-1];
    }
    int my_n = num_rows[rank];

    vector<long long> myTimestamp(my_n), myBytesSent(my_n);
    vector<int> myServerId(my_n), myEndpointId(my_n), myStatusCode(my_n);
    vector<double> myResponseTime(my_n);

    MPI_Scatterv(timestamp.data(),num_rows.data(),ind.data(),MPI_LONG_LONG,myTimestamp.data(),my_n,MPI_LONG_LONG,0,MPI_COMM_WORLD);
    MPI_Scatterv(serverId.data(),num_rows.data(),ind.data(),MPI_INT,myServerId.data(),my_n,MPI_INT,0,MPI_COMM_WORLD);
    MPI_Scatterv(endpointId.data(),num_rows.data(),ind.data(),MPI_INT,myEndpointId.data(),my_n,MPI_INT,0,MPI_COMM_WORLD);
    MPI_Scatterv(statusCode.data(),num_rows.data(),ind.data(),MPI_INT,myStatusCode.data(),my_n,MPI_INT,0,MPI_COMM_WORLD);
    MPI_Scatterv(responseTime.data(),num_rows.data(),ind.data(),MPI_DOUBLE,myResponseTime.data(),my_n,MPI_DOUBLE,0,MPI_COMM_WORLD);
    MPI_Scatterv(bytesSent.data(),num_rows.data(),ind.data(),MPI_LONG_LONG,myBytesSent.data(),my_n,MPI_LONG_LONG,0,MPI_COMM_WORLD);

    double comm_time = MPI_Wtime() - t_scatter_start;
    double compute_time = 0.0;

    double t_compute_start = MPI_Wtime();

    long long localSuccessful=0, localFailed=0;
    double localRespSum=0.0, localMinResp=DBL_MAX, localMaxResp=-DBL_MAX;
    long long localTotalBytes=0;
    long long localStatus2xx=0, localStatus3xx=0, localStatus4xx=0, localStatus5xx=0;
    long long localMinTs=LLONG_MAX, localMaxTs=LLONG_MIN;

    // sentinels above matter here: some ranks can get my_n==0 (P > N), and
    // an idle rank must not contribute a spurious 0 into the MIN/MAX reduce

    for(int i=0; i<my_n; i++){
        if(myStatusCode[i] < 400) localSuccessful++;
        else localFailed++;

        localRespSum += myResponseTime[i];
        if(myResponseTime[i] < localMinResp) localMinResp = myResponseTime[i];
        if(myResponseTime[i] > localMaxResp) localMaxResp = myResponseTime[i];

        localTotalBytes += myBytesSent[i];

        int cls = myStatusCode[i]/100;
        if(cls==2) localStatus2xx++;
        else if(cls==3) localStatus3xx++;
        else if(cls==4) localStatus4xx++;
        else if(cls==5) localStatus5xx++;

        if(myTimestamp[i] < localMinTs) localMinTs = myTimestamp[i];
        if(myTimestamp[i] > localMaxTs) localMaxTs = myTimestamp[i];
    }

    compute_time += MPI_Wtime() - t_compute_start;

    double t_reduce_start = MPI_Wtime();

    long long successful=0, failed=0;
    double respSum=0.0, minResp=0.0, maxResp=0.0;
    long long totalBytes=0;
    long long status2xx=0, status3xx=0, status4xx=0, status5xx=0;

    MPI_Reduce(&localSuccessful,&successful,1,MPI_LONG_LONG,MPI_SUM,0,MPI_COMM_WORLD);
    MPI_Reduce(&localFailed,&failed,1,MPI_LONG_LONG,MPI_SUM,0,MPI_COMM_WORLD);
    MPI_Reduce(&localRespSum,&respSum,1,MPI_DOUBLE,MPI_SUM,0,MPI_COMM_WORLD);
    MPI_Reduce(&localMinResp,&minResp,1,MPI_DOUBLE,MPI_MIN,0,MPI_COMM_WORLD);
    MPI_Reduce(&localMaxResp,&maxResp,1,MPI_DOUBLE,MPI_MAX,0,MPI_COMM_WORLD);
    MPI_Reduce(&localTotalBytes,&totalBytes,1,MPI_LONG_LONG,MPI_SUM,0,MPI_COMM_WORLD);
    MPI_Reduce(&localStatus2xx,&status2xx,1,MPI_LONG_LONG,MPI_SUM,0,MPI_COMM_WORLD);
    MPI_Reduce(&localStatus3xx,&status3xx,1,MPI_LONG_LONG,MPI_SUM,0,MPI_COMM_WORLD);
    MPI_Reduce(&localStatus4xx,&status4xx,1,MPI_LONG_LONG,MPI_SUM,0,MPI_COMM_WORLD);
    MPI_Reduce(&localStatus5xx,&status5xx,1,MPI_LONG_LONG,MPI_SUM,0,MPI_COMM_WORLD);

    // every process needs the global timestamp range before it can size its
    // own local busiest-interval histogram, so this one needs Allreduce
    long long globalMinTs, globalMaxTs;
    MPI_Allreduce(&localMinTs,&globalMinTs,1,MPI_LONG_LONG,MPI_MIN,MPI_COMM_WORLD);
    MPI_Allreduce(&localMaxTs,&globalMaxTs,1,MPI_LONG_LONG,MPI_MAX,MPI_COMM_WORLD);

    comm_time += MPI_Wtime() - t_reduce_start;

    double t_hist_start = MPI_Wtime();

    long long minInterval = globalMinTs/60, maxInterval = globalMaxTs/60;
    long long range = maxInterval - minInterval + 1;
    vector<long long> localIntervalCounts(range,0);
    for(int i=0; i<my_n; i++){
        localIntervalCounts[myTimestamp[i]/60 - minInterval]++;
    }

    compute_time += MPI_Wtime() - t_hist_start;

    vector<long long> globalIntervalCounts;
    if(rank == 0) globalIntervalCounts.resize(range);

    double t_hist_reduce_start = MPI_Wtime();
    MPI_Reduce(localIntervalCounts.data(),globalIntervalCounts.data(),range,MPI_LONG_LONG,MPI_SUM,0,MPI_COMM_WORLD);
    comm_time += MPI_Wtime() - t_hist_reduce_start;

    long long busiestId=0, busiestCount=0;
    if(rank == 0){
        for(long long idx=0; idx<range; idx++){
            if(globalIntervalCounts[idx] > busiestCount){
                busiestCount = globalIntervalCounts[idx];
                busiestId = idx + minInterval;
            }
        }
    }

    double t_perid_start = MPI_Wtime();

    vector<long long> localServerCount(s,0), localEndpointCount(s,0), localEndpointBytes(s,0);
    vector<double> localServerRespSum(s,0.0);
    for(int i=0; i<my_n; i++){
        localServerCount[myServerId[i]]++;
        localServerRespSum[myServerId[i]] += myResponseTime[i];

        localEndpointCount[myEndpointId[i]]++;
        localEndpointBytes[myEndpointId[i]] += myBytesSent[i];
    }

    compute_time += MPI_Wtime() - t_perid_start;

    vector<long long> globalServerCount, globalEndpointCount, globalEndpointBytes;
    vector<double> globalServerRespSum;
    if(rank == 0){
        globalServerCount.resize(s); globalEndpointCount.resize(s); globalEndpointBytes.resize(s);
        globalServerRespSum.resize(s);
    }

    double t_perid_reduce_start = MPI_Wtime();
    MPI_Reduce(localServerCount.data(),globalServerCount.data(),s,MPI_LONG_LONG,MPI_SUM,0,MPI_COMM_WORLD);
    MPI_Reduce(localServerRespSum.data(),globalServerRespSum.data(),s,MPI_DOUBLE,MPI_SUM,0,MPI_COMM_WORLD);
    MPI_Reduce(localEndpointCount.data(),globalEndpointCount.data(),s,MPI_LONG_LONG,MPI_SUM,0,MPI_COMM_WORLD);
    MPI_Reduce(localEndpointBytes.data(),globalEndpointBytes.data(),s,MPI_LONG_LONG,MPI_SUM,0,MPI_COMM_WORLD);
    comm_time += MPI_Wtime() - t_perid_reduce_start;

    // rank with the largest total time is the actual bottleneck for the run
    double local_total = comm_time + compute_time;
    RankTime local_pair = {local_total, rank}, global_pair;
    MPI_Allreduce(&local_pair,&global_pair,1,MPI_DOUBLE_INT,MPI_MAXLOC,MPI_COMM_WORLD);

    double reported_comm=0, reported_compute=0, reported_total=0;

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
        double avgResp = (n > 0) ? respSum/n : 0.0;

        vector<EntityStats> serverStats(s), endpointStats(s);
        for(int id=0; id<s; id++){
            serverStats[id] = {id, globalServerCount[id], globalServerRespSum[id], 0};
            endpointStats[id] = {id, globalEndpointCount[id], 0.0, globalEndpointBytes[id]};
        }
        sort(serverStats.begin(),serverStats.end(),compareEntity);
        sort(endpointStats.begin(),endpointStats.end(),compareEntity);

        cout << "TOTAL_REQUESTS " << n << "\n";
        cout << "SUCCESSFUL_REQUESTS " << successful << "\n";
        cout << "FAILED_REQUESTS " << failed << "\n";
        cout << fixed << setprecision(6);
        cout << "AVERAGE_RESPONSE_TIME " << avgResp << "\n";
        cout << "MIN_RESPONSE_TIME " << minResp << "\n";
        cout << "MAX_RESPONSE_TIME " << maxResp << "\n";
        cout << "TOTAL_BYTES " << totalBytes << "\n";
        cout << "STATUS_2XX " << status2xx << "\n";
        cout << "STATUS_3XX " << status3xx << "\n";
        cout << "STATUS_4XX " << status4xx << "\n";
        cout << "STATUS_5XX " << status5xx << "\n";
        cout << "BUSIEST_INTERVAL " << busiestId << " " << busiestCount << "\n";

        cout << "TOP_SERVERS\n";
        for(int i=0; i<k && i<s; i++){
            if(serverStats[i].count == 0) break;
            cout << serverStats[i].id << " " << serverStats[i].count << " " << serverStats[i].responseTimeSum/serverStats[i].count << "\n";
        }

        cout << "TOP_ENDPOINTS\n";
        for(int i=0; i<k && i<s; i++){
            if(endpointStats[i].count == 0) break;
            cout << endpointStats[i].id << " " << endpointStats[i].count << " " << endpointStats[i].totalBytes << "\n";
        }

        cerr << "TIME_SECONDS " << reported_total << endl;
        cerr << "COMPUTE_SECONDS " << reported_compute << endl;
        cerr << "COMM_SECONDS " << reported_comm << endl;
        cerr << "NUM_PROCS " << size << endl;
    }

    MPI_Finalize();

    return 0;
}
