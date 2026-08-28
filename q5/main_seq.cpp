#include <iostream>
#include <vector>
#include <chrono>


using namespace std;


int main(){
    int n,m; cin >> n >> m;
    vector<vector<int>> dist(n,vector<int>(n,1e6));

    for(int i=0;i<n;i++){
        dist[i][i] = 0;
    }

    for(int i=0;i<m;i++){
        int u,v,w; cin >> u >> v >> w;
        dist[u][v] = w; 
    }

    auto timeStart = chrono::high_resolution_clock::now();

    for(int i=0;i<n;i++){
        for(int u=0;u<n;u++){
            for(int v=0;v<n;v++){
                if(dist[u][i] != 1e6 && dist[i][v] != 1e6)
                dist[u][v] = min(dist[u][v],dist[u][i]+dist[i][v]);
            }
        }
    }

    auto timeEnd = chrono::high_resolution_clock::now();
    double totaltime = chrono::duration<double>(timeEnd - timeStart).count();

    for(int i=0;i<n;i++){
        for(int j=0;j<n;j++){
            cout << dist[i][j] << " ";
        }
        cout << endl;
    }

    cerr << "TIME_SECONDS " << totaltime << endl;

    return 0;
}