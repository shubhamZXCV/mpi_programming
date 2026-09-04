#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <iomanip>
#include <cstdlib>

using namespace std;

int main(int argc,char** argv){
    if(argc < 6){
        cerr << "Usage: " << argv[0] << " <N> <K> <S> <seed> <output_file>\n";
        return 1;
    }

    long long N = atoll(argv[1]);
    int K = atoi(argv[2]);
    int S = atoi(argv[3]);
    unsigned seed = (unsigned)atoll(argv[4]);
    string out_file = argv[5];

    mt19937_64 rng(seed);
    uniform_int_distribution<int> id_dist(0,S-1);
    uniform_int_distribution<int> user_dist(0,99999);
    uniform_int_distribution<int> jitter_dist(0,2);
    uniform_int_distribution<int> resp_dist(0,49999);
    uniform_int_distribution<int> bytes_dist(0,199999);

    int statusPool[] = {200,200,200,200,201,301,302,404,404,500,503};
    int statusPoolSize = 11;
    uniform_int_distribution<int> status_dist(0,statusPoolSize-1);

    ofstream out(out_file);
    if(!out){
        cerr << "Failed to open output file: " << out_file << "\n";
        return 1;
    }

    out << N << " " << K << " " << S << "\n";
    out << fixed << setprecision(2);

    long long timestamp = 1700000000LL; // an arbitrary but realistic-looking unix timestamp
    for(long long i=0; i<N; i++){
        timestamp += jitter_dist(rng); // timestamps mostly increase, occasionally repeat, like a real log

        int serverId = id_dist(rng);
        int endpointId = id_dist(rng);
        int userId = user_dist(rng);
        int statusCode = statusPool[status_dist(rng)];
        double responseTime = 5.0 + resp_dist(rng)/100.0; // 5.00 to 504.99 ms
        long long bytesSent = 100 + bytes_dist(rng);

        out << timestamp << " " << serverId << " " << endpointId << " " << userId << " "
            << statusCode << " " << responseTime << " " << bytesSent << "\n";
    }

    cerr << "Generated " << out_file << "\n";
    cerr << "N=" << N << " K=" << K << " S=" << S << " seed=" << seed << "\n";

    return 0;
}
