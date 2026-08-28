#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <set>
#include <cstdlib>

using namespace std;

int main(int argc,char** argv){
    if(argc < 6){
        cerr << "Usage: " << argv[0] << " <V> <E> <max_weight> <seed> <output_file> [allow_negative:0/1]\n";
        return 1;
    }

    long long V = atoll(argv[1]);
    long long E = atoll(argv[2]);
    int max_weight = atoi(argv[3]);
    unsigned seed = (unsigned)atoll(argv[4]);
    string out_file = argv[5];
    bool allow_negative = (argc >= 7) && (atoi(argv[6]) != 0);

    if(V < 2){
        cerr << "V must be >= 2\n";
        return 1;
    }

    long long max_edges = V*(V-1); // no self loops, directed, no duplicates
    if(E > max_edges){
        cerr << "E too large for V (max " << max_edges << " without duplicates/self-loops), clamping.\n";
        E = max_edges;
    }

    mt19937_64 rng(seed);
    uniform_int_distribution<long long> vertex_dist(0,V-1);

    // random potentials, used only when allow_negative is set
    int half = max(1,max_weight/2);
    uniform_int_distribution<int> pot_dist(0,half);
    uniform_int_distribution<int> base_dist(0,half);
    uniform_int_distribution<int> weight_dist(0,max_weight);

    vector<int> potential(V);
    if(allow_negative){
        for(long long i=0; i<V; i++) potential[i] = pot_dist(rng);
    }

    set<pair<long long,long long>> used_edges;
    vector<pair<long long,long long>> edges;
    edges.reserve(E);

    // sample distinct directed edges (u != v, no duplicates)
    while((long long)edges.size() < E){
        long long u = vertex_dist(rng);
        long long v = vertex_dist(rng);
        if(u == v) continue;
        if(used_edges.count({u,v})) continue;
        used_edges.insert({u,v});
        edges.push_back({u,v});
    }

    ofstream out(out_file);
    if(!out){
        cerr << "Failed to open output file: " << out_file << "\n";
        return 1;
    }

    out << V << " " << edges.size() << "\n";
    for(auto &e : edges){
        long long u = e.first, v = e.second;
        int w;
        if(allow_negative){
            w = base_dist(rng) + potential[v] - potential[u];
        }
        else{
            w = weight_dist(rng);
        }
        out << u << " " << v << " " << w << "\n";
    }

    cerr << "Generated " << out_file << "\n";
    cerr << "V=" << V << " E=" << edges.size() << " max_weight=" << max_weight
         << " allow_negative=" << allow_negative << " seed=" << seed << "\n";

    return 0;
}
