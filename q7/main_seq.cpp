#include <iostream>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <climits>
#include <chrono>

using namespace std;

struct EntityStats{
    int id;
    long long count;
    double responseTimeSum;
    long long totalBytes;
};

bool compareEntity(const EntityStats &a,const EntityStats &b){
    if(a.count != b.count) return a.count > b.count;
    return a.id < b.id;
}

int main(){
    int n,k,s; cin >> n >> k >> s;

    vector<long long> timestamp(n), bytesSent(n);
    vector<int> serverId(n), endpointId(n), statusCode(n);
    vector<double> responseTime(n);

    for(int i=0; i<n; i++){
        int userId; // read but not needed for any required output field
        cin >> timestamp[i] >> serverId[i] >> endpointId[i] >> userId >> statusCode[i] >> responseTime[i] >> bytesSent[i];
    }

    auto t_start = chrono::high_resolution_clock::now();

    long long successful=0, failed=0;
    double responseSum=0, minResp=0, maxResp=0;
    long long totalBytes=0;
    long long status2xx=0, status3xx=0, status4xx=0, status5xx=0;
    long long minTs=LLONG_MAX, maxTs=LLONG_MIN;

    for(int i=0; i<n; i++){
        if(statusCode[i] < 400) successful++;
        else failed++;

        responseSum += responseTime[i];
        if(i==0 || responseTime[i] < minResp) minResp = responseTime[i];
        if(i==0 || responseTime[i] > maxResp) maxResp = responseTime[i];

        totalBytes += bytesSent[i];

        int cls = statusCode[i]/100;
        if(cls==2) status2xx++;
        else if(cls==3) status3xx++;
        else if(cls==4) status4xx++;
        else if(cls==5) status5xx++;

        if(timestamp[i] < minTs) minTs = timestamp[i];
        if(timestamp[i] > maxTs) maxTs = timestamp[i];
    }
    double avgResp = (n > 0) ? responseSum/n : 0.0;

    // every server_id/endpoint_id is < s, so plain s-sized arrays work as a
    // zero-collision hash map (identity hash), O(1) lookup per record
    vector<EntityStats> serverStats(s), endpointStats(s);
    for(int id=0; id<s; id++){
        serverStats[id] = {id,0,0.0,0};
        endpointStats[id] = {id,0,0.0,0};
    }
    for(int i=0; i<n; i++){
        serverStats[serverId[i]].count++;
        serverStats[serverId[i]].responseTimeSum += responseTime[i];

        endpointStats[endpointId[i]].count++;
        endpointStats[endpointId[i]].totalBytes += bytesSent[i];
    }

    // busiest 60-second interval: offset interval ids by minTs/60 so the
    // histogram is sized to the log's actual span, not raw epoch time
    long long minInterval = (n > 0) ? minTs/60 : 0;
    long long maxInterval = (n > 0) ? maxTs/60 : 0;
    long long range = maxInterval - minInterval + 1;
    vector<long long> intervalCounts(range,0);
    for(int i=0; i<n; i++){
        intervalCounts[timestamp[i]/60 - minInterval]++;
    }
    long long busiestId=0, busiestCount=0;
    for(long long idx=0; idx<range; idx++){
        if(intervalCounts[idx] > busiestCount){
            busiestCount = intervalCounts[idx];
            busiestId = idx + minInterval;
        }
    }

    sort(serverStats.begin(),serverStats.end(),compareEntity);
    sort(endpointStats.begin(),endpointStats.end(),compareEntity);

    auto t_end = chrono::high_resolution_clock::now();
    double elapsed = chrono::duration<double>(t_end - t_start).count();

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
        if(serverStats[i].count == 0) break; // never appeared in the log, and neither does anything after it
        cout << serverStats[i].id << " " << serverStats[i].count << " " << serverStats[i].responseTimeSum/serverStats[i].count << "\n";
    }

    cout << "TOP_ENDPOINTS\n";
    for(int i=0; i<k && i<s; i++){
        if(endpointStats[i].count == 0) break;
        cout << endpointStats[i].id << " " << endpointStats[i].count << " " << endpointStats[i].totalBytes << "\n";
    }

    cerr << "TIME_SECONDS " << elapsed << endl;

    return 0;
}
