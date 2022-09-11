#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <set>
#include <bitset>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <climits>
#include <cmath>
#include "rapidxml.hpp"

#define RSU_RANGE 300
#define RSU_RATE 10
#define VEHICLE_RANGE 300
#define VEHICLE_RATE 5
#define RSU_MAX 2
#define CELL_RATE 10
#define W_SAT 1.5
#define W_NSAT 1
#define RECURSION_CUTOFF 0.4

#define RSU -2
#define CELLULAR -1
#define VEHICLE 0

#define V2V_SUCCESS_RATE 80
#define V2R_SUCCESS_RATE 85
using namespace std;

// A vehicle transfer
typedef struct vtransfer {
    int id;
    bool isrecv;
    int type;
    int item;
    int amount;
    vtransfer() : type(VEHICLE), item(-1), isrecv(false), id(-1), amount(0) {
    }
    void clear() {
        this->id = -1;
        this->amount = 0;
        this->isrecv = false;
        this->item = -1;
        this->type = VEHICLE;
    }
    void update(int id, bool isrecv, int type, int item, int amount) {
        this->id = id;
        this->isrecv = isrecv;
        this->type = type;
        this->item = item;
        this->amount = amount;
        //cout << "Storage " <<  amount << endl;
    }
} vtransfer;

//a candidate vehicle at an RSU
typedef struct vehicle_score {
    int id;
    int item;
    float score;
    int t_0;
    vehicle_score() : id(-1), item(-1), score(0), t_0(0) {
    }
    vehicle_score(int id, int item, float score, int t_0) : id(id), item(item), score(score), t_0(t_0) {
    }
} vehicle_score;

//a candidate vehicle for a V2V transfer
typedef struct sel_vehicle {
    int id;
    int item;
    int amount;
    sel_vehicle() : id(-1), item(-1), amount(0) {
    }
    sel_vehicle(int id, int item, int amount) : id(id), item(item), amount(amount) {
    }
} sel_vehicle;

//select a vehicle for a V2V transfer
sel_vehicle select_vehicle(set<pair<int,int>> &candidates, vector<vector<pair<int,int>>> &storage, int srcvehicle, vector<int> &datasizes) {
    int mingap = INT_MAX;
    sel_vehicle selv = sel_vehicle();
    for(set<pair<int,int>>::iterator itr=candidates.begin(); itr!=candidates.end(); itr++) {
        int recv = itr->first;
        int item = itr->second;
        if(datasizes[item]-storage[recv][item].second > 0 && datasizes[item]-storage[recv][item].second < mingap) {
            mingap = datasizes[item]-storage[recv][item].second;
            selv.id = recv;
            selv.item = item;
            selv.amount = min(storage[srcvehicle][item].second-storage[recv][item].second, VEHICLE_RATE);
        }
    }
    return selv;
}

//select a vehicle for an RSU transfer
vehicle_score rsu_select_vehicle(vector<vehicle_score> &scores) {
    vehicle_score vbest = vehicle_score();
    vbest.score = INT_MIN;
    for(int i=0; i<scores.size(); i++) {
        if(scores[i].score >= vbest.score) {
            vbest = scores[i];
        }
    }
    return vbest;
}

//get received data amount over an interval start->end
int getStorage(vector<vtransfer> &transfers, int start, int end, int item, int datasize) {
    int storage=0;
    for(int t=start; t<end; t++) {
        if(transfers[t].item==item && transfers[t].isrecv) {
            storage+=transfers[t].amount;
        }
    }
    if(storage>datasize) {
        return datasize;
    }
    return storage;
}

//update the storage state to a target time
void updateStorage(vector<vector<vtransfer>> &vehicle_transfers, vector<vector<pair<int,int>>> &storage, int vehicle, int item, int datasize, int time) {
    storage[vehicle][item].second = storage[vehicle][item].second+getStorage(vehicle_transfers[vehicle], storage[vehicle][item].first, time, item, datasize);
    if(storage[vehicle][item].second > datasize)
        storage[vehicle][item].second = datasize;
    storage[vehicle][item].first = time;
    return;
}

//add a vehicle to the set Vnew for updateTransfers
void insertVnew(map<int,unordered_set<int>> &vnew, int vehicle, int item) {
    map<int,unordered_set<int>>::iterator pos = vnew.find(vehicle);
    if(pos==vnew.end()) {
        vnew.insert(make_pair(vehicle, unordered_set<int>({item})));
    } else {
        pos->second.insert(item);
    }
}

//calculate the score of a given (vehicle,item) pair
float get_score(vector<vector<vtransfer>> temp_transfers, vector<vector<pair<int,int>>> storage, vector<vector<int>> &requests, vector<vector<vector<bool>>> &v2v_contacts, int vehicle, int rsu, int start, int t_0, int time_units, int item, vector<int> &datasizes)
{
    float score = 0;
    //vehicle receives data from the rsu in temporary transfers
    for(int t=start; t<t_0; t++) {
        if(temp_transfers[vehicle][t].id!=-1 && !temp_transfers[vehicle][t].isrecv) {
            //TODO: Score may be decremented here to reflect lost transfers
            //score -= temp_transfers[vehicle][t].amount;
        }
        temp_transfers[vehicle][t].update(rsu, true, RSU, item, RSU_RATE);
    }
    vector<int> data_transfer = vector<int>(temp_transfers.size(), 0);
    set<pair<int,int>> candidates;
    for(int t=t_0; t<time_units; t++) {
        candidates.clear();
        if(temp_transfers[vehicle][t].id!=-1) {
            //if there is already a transfer, check if more can be transferred than before
            int k=temp_transfers[vehicle][t].id;
            if(temp_transfers[vehicle][t].type==VEHICLE && temp_transfers[vehicle][t].isrecv==false && temp_transfers[vehicle][t].item == item
                 && temp_transfers[vehicle][t].amount < VEHICLE_RATE) {
                //update temporary storage
                updateStorage(temp_transfers, storage, vehicle, item, datasizes[item], t);
                updateStorage(temp_transfers, storage, k, item, datasizes[item], t);
                int amount = storage[vehicle][item].second-storage[k][item].second;
                if(amount>temp_transfers[vehicle][t].amount) {
                    data_transfer[k]+= min(VEHICLE_RATE, amount)-temp_transfers[vehicle][t].amount;
                    //amount to be transferred has changed
                    temp_transfers[vehicle][t].amount = min(VEHICLE_RATE,amount);
                    temp_transfers[k][t].amount = min(VEHICLE_RATE,amount);
                }
            }
            continue;
        }
        for(int k=0; k<temp_transfers.size(); k++) {
            if(k!=vehicle) {
                //contacted vehicle requested the item, in contact and free
                if((requests[k][item]<=t) && v2v_contacts[k][vehicle][t] && temp_transfers[k][t].item==-1) {
                    //update temporary storage
                    updateStorage(temp_transfers, storage, vehicle, item, datasizes[item], t);
                    updateStorage(temp_transfers, storage, k, item, datasizes[item], t);
                    //update candidates
                    if(storage[vehicle][item].second-storage[k][item].second > 0)
                        candidates.insert(make_pair(k,item));
                }
            }
        }
        //select a vehicle for the free time instant
        sel_vehicle selv = select_vehicle(candidates, storage, vehicle, datasizes);
        if(selv.id!=-1) {
            //update temp transfers
            temp_transfers[vehicle][t].update(selv.id, false, VEHICLE, selv.item, selv.amount);
            temp_transfers[selv.id][t].update(vehicle, true, VEHICLE, selv.item, selv.amount);
            data_transfer[selv.id] += selv.amount;
        }
    }
    //calculate score depending on whether the request was satisfied
    for(int k=0; k<temp_transfers.size(); k++) {
        if(storage[k][item].first < time_units) {
            updateStorage(temp_transfers, storage, k, item, datasizes[item], time_units);
        }
        if(data_transfer[k] >= datasizes[item]-storage[k][item].second) {
            score += W_SAT*(datasizes[item]-storage[k][item].second);
        } else {
            score += W_NSAT*data_transfer[k];
        }
    }
    return score;
}

//update V2V transfers given a set of vehicles Vnew
void updateTransfers(vector<vector<vtransfer>> &vehicle_transfers, vector<vector<int>> &data_lost, vector<vector<int>> &data_gained, set<pair<int,int>> &affectedVehicles, vector<vector<pair<int,int>>> &mainstorage, vector<vector<pair<int,int>>> &init_storage, vector<vector<int>> &requests, map<int,unordered_set<int>> &vnew, vector<vector<vector<bool>>> &v2v_contacts, vector<int> &datasizes, int curTime, int time_units)
{
    //remember how much data was lost/gained by each vehicle between calls to updateTransfers
    while(!(vnew.empty())) {
        int current = vnew.begin()->first;
        unordered_set<int> items = vnew.begin()->second;
        for(int i=0; i<datasizes.size(); i++) {
            data_lost[current][i] = 0;
        }
        for(unordered_set<int>::iterator item=items.begin(); item!=items.end(); item++) {
            data_gained[current][*item] = 0;
        }
        set<pair<int,int>> candidates;
        //cout << current << endl;
        vector<vector<pair<int,int>>> storage(init_storage);
        for(int t=0; t<time_units; t++) {
            candidates.clear();
            //if a transfer is already scheduled, check for partial utilization/transfers to be cancelled
            if(vehicle_transfers[current][t].id!=-1) {
                int transfer_item = vehicle_transfers[current][t].item;
                int k=vehicle_transfers[current][t].id;
                if(vehicle_transfers[current][t].type==VEHICLE) {
                    //update temporary storage
                    updateStorage(vehicle_transfers, storage, current, transfer_item, datasizes[transfer_item], t);
                    updateStorage(vehicle_transfers, storage, k, transfer_item, datasizes[transfer_item], t);
                    int diff = storage[current][transfer_item].second-storage[k][transfer_item].second;
                    if(vehicle_transfers[current][t].isrecv) {
                        diff *= -1;
                    }
                    if(diff<=0) {
                        if(!vehicle_transfers[current][t].isrecv) {
                            data_lost[k][transfer_item]+=abs(diff);
                        }
                        vehicle_transfers[current][t].clear();
                        vehicle_transfers[k][t].clear();
                        affectedVehicles.insert(make_pair(k, transfer_item));
                    } else {
                        int newamount = min(VEHICLE_RATE, diff);
                        int oldamount = vehicle_transfers[current][t].amount;
                        if(!vehicle_transfers[current][t].isrecv) {
                            if(newamount > oldamount) {
                                data_gained[k][transfer_item]+=newamount-oldamount;
                            } else if(newamount < oldamount) {
                                data_lost[k][transfer_item]+=oldamount-newamount;
                            }
                        }
                        vehicle_transfers[current][t].amount = newamount;
                        vehicle_transfers[k][t].amount = newamount;
                        affectedVehicles.insert(make_pair(k, transfer_item));
                        continue;
                    }
                } else {
                    continue;
                }
            }
            for(unordered_set<int>::iterator item=items.begin(); item!=items.end(); item++) {
                for(int k=0; k<vehicle_transfers.size(); k++) {
                    if(k!=current) {
                        //candidate vehicle requested the item, in contact and free
                        if((requests[k][*item]<=t) && v2v_contacts[k][current][t] && vehicle_transfers[k][t].id==-1) {
                            //update temporary storage
                            updateStorage(vehicle_transfers, storage, current, *item, datasizes[*item], t);
                            updateStorage(vehicle_transfers, storage, k, *item, datasizes[*item], t);
                            //update candidates
                            int amount = storage[current][*item].second-storage[k][*item].second;
                            if(amount>0 && storage[current][*item].second < datasizes[*item])
                                candidates.insert(make_pair(k,*item));
                        }
                    }
                }
            }
            sel_vehicle selv = select_vehicle(candidates,storage,current,datasizes);
            if(selv.amount < 0) {
                cout << "update transfers fail: " << selv.amount << endl;
            }
            if(selv.id!=-1) {
                vehicle_transfers[current][t].update(selv.id, false, VEHICLE, selv.item, selv.amount);
                vehicle_transfers[selv.id][t].update(current, true, VEHICLE, selv.item, selv.amount);
                data_gained[selv.id][selv.item] += selv.amount;
                affectedVehicles.insert(make_pair(selv.id, selv.item));
                //recursion with bound
                //if(getStorage(vehicle_transfers[selv.id], storage[selv.id][selv.item].first, time_units, selv.item, datasizes[selv.item]) > RECURSION_CUTOFF*datasizes[selv.item]) {
                // if(data_gained[selv.id][selv.item] > RECURSION_CUTOFF*datasizes[selv.item]) {
                //     insertVnew(vnew, selv.id, selv.item);
                // }
            }
            if(t==curTime)
                mainstorage = storage;
        }
        for(set<pair<int,int>>::iterator itr=affectedVehicles.begin(); itr!=affectedVehicles.end(); itr++) {
            int v = itr->first;
            int d = itr->second;
            //change in data since last call greater than a certain threshold
            if(abs(data_gained[v][d]-data_lost[v][d]) > RECURSION_CUTOFF*datasizes[d]) {
                //if(v==243)
                //cout << v << ", " << d << ", " << data_gained[v][d] << ", " << data_lost[v][d] << endl;
                insertVnew(vnew, itr->first, itr->second);
            }
        }
        affectedVehicles.clear();
        vnew.erase(current);
    }
    return;
}

//remove illegal transfers
void cleanup(vector<vector<vtransfer>> &vehicle_transfers, map<int,unordered_set<int>> &vnew, vector<vector<int>> &requests, vector<vector<pair<int,int>>> &mainstorage, vector<vector<pair<int,int>>> &init_storage, vector<vector<vector<bool>>> &v2v_contacts, vector<int> &datasizes, int time_units)
{
    float storagecleared = 0;
    while(!(vnew.empty())) {
        int current = vnew.begin()->first;
        unordered_set<int> items = vnew.begin()->second;
        set<pair<int,int>> candidates;
        vector<vector<pair<int,int>>> storage(init_storage);
        for(int t=0; t<time_units; t++) {
            candidates.clear();
            //if a transfer is already scheduled, check for partial utilization/transfers to be cancelled
            if(vehicle_transfers[current][t].id!=-1) {
                int transfer_item = vehicle_transfers[current][t].item;
                int k=vehicle_transfers[current][t].id;
                if(vehicle_transfers[current][t].type==VEHICLE) {
                    //update temporary storage
                    updateStorage(vehicle_transfers, storage, current, transfer_item, datasizes[transfer_item], t);
                    updateStorage(vehicle_transfers, storage, k, transfer_item, datasizes[transfer_item], t);
                    int amount = storage[current][transfer_item].second-storage[k][transfer_item].second;
                    if(vehicle_transfers[current][t].isrecv) {
                        amount *= -1;
                    }
                    if(amount<=0) {
                        if(!vehicle_transfers[current][t].isrecv){
                            //if receiving vehicle loses data, add to vnew
                            insertVnew(vnew, k, transfer_item);
                        }
                        storagecleared += abs(amount);
                        vehicle_transfers[current][t].clear();
                        vehicle_transfers[k][t].clear();
                    } else if(amount<vehicle_transfers[current][t].amount) {
                        storagecleared += abs(vehicle_transfers[current][t].amount - amount);
                        if(!vehicle_transfers[current][t].isrecv) {
                            insertVnew(vnew, k, transfer_item);
                        }
                        vehicle_transfers[current][t].amount = min(VEHICLE_RATE,amount);
                        vehicle_transfers[k][t].amount = min(VEHICLE_RATE,amount);
                        continue;
                    }
                } else {
                    continue;
                }
            }
            // for(unordered_set<int>::iterator item=items.begin(); item!=items.end(); item++) {
            //     for(int k=0; k<vehicle_transfers.size(); k++) {
            //         if(k!=current) {
            //             //candidate vehicle requested the item, in contact and free
            //             if((requests[k][*item]<=t) && v2v_contacts[k][current][t] && vehicle_transfers[k][t].id==-1) {
            //                 //update temporary storage
            //                 updateStorage(vehicle_transfers, storage, current, *item, datasizes[*item], t);
            //                 updateStorage(vehicle_transfers, storage, k, *item, datasizes[*item], t);
            //                 //update candidates
            //                 int amount = storage[current][*item].second-storage[k][*item].second;
            //                 if(amount>0) {
            //                     candidates.insert(make_pair(k,*item));
            //                 }
            //             }
            //         }
            //     }
            // }
            // sel_vehicle selv = select_vehicle(candidates,storage,current,datasizes);
            // if(selv.id!=-1) {
            //     vehicle_transfers[current][t].update(selv.id, false, VEHICLE, selv.item, selv.amount);
            //     vehicle_transfers[selv.id][t].update(current, true, VEHICLE, selv.item, selv.amount);
            //     //insertVnew(vnew, selv.id, selv.item);
            // }
        }
        mainstorage = storage;
        vnew.erase(current);
    }
    cout << "Data cleared: " << storagecleared << endl;
    return;
}

void cleanup_all(vector<vector<vtransfer>> &vehicle_transfers, vector<vector<map<int, vehicle_score>>> &rsu_transfers,
    map<int,unordered_set<int>> &vnew, vector<vector<int>> &requests, 
    vector<vector<pair<int,int>>> &mainstorage, vector<vector<pair<int,int>>> &init_storage, vector<vector<vector<bool>>> &v2v_contacts_real,
    vector<vector<vector<bool>>> &v2r_contacts_real, vector<int> &datasizes, int time_units)
{
    float storagecleared = 0;
    while(!(vnew.empty())) {
        int current = vnew.begin()->first;
        unordered_set<int> items = vnew.begin()->second;
        set<pair<int,int>> candidates;
        vector<vector<pair<int,int>>> storage(init_storage);
        for(int t=0; t<time_units; t++) {
            candidates.clear();
            //if a transfer is already scheduled, check for partial utilization/transfers to be cancelled
            if(vehicle_transfers[current][t].id!=-1) {
                int transfer_item = vehicle_transfers[current][t].item;
                int k=vehicle_transfers[current][t].id;
                if(vehicle_transfers[current][t].type==VEHICLE) {
                    //update temporary storage
                    updateStorage(vehicle_transfers, storage, current, transfer_item, datasizes[transfer_item], t);
                    updateStorage(vehicle_transfers, storage, k, transfer_item, datasizes[transfer_item], t);
                    int amount = storage[current][transfer_item].second-storage[k][transfer_item].second;
                    if(vehicle_transfers[current][t].isrecv) {
                        amount *= -1;
                    }

                    if(amount<=0) {
                        if(!vehicle_transfers[current][t].isrecv){
                            //if receiving vehicle loses data, add to vnew
                            insertVnew(vnew, k, transfer_item);
                        }
                        storagecleared += abs(vehicle_transfers[current][t].amount);
                        vehicle_transfers[current][t].clear();
                        vehicle_transfers[k][t].clear();
                    } else if(!v2v_contacts_real[current][k][t]) {
                        if(!vehicle_transfers[current][t].isrecv){
                            //if receiving vehicle loses data, add to vnew
                            insertVnew(vnew, k, transfer_item);
                        }
                        storagecleared += abs(vehicle_transfers[current][t].amount);
                        vehicle_transfers[current][t].clear();
                        vehicle_transfers[k][t].clear();
                    } else if(amount<vehicle_transfers[current][t].amount) {
                        storagecleared += abs(vehicle_transfers[current][t].amount - amount);
                        if(!vehicle_transfers[current][t].isrecv) {
                            insertVnew(vnew, k, transfer_item);
                        }
                        vehicle_transfers[current][t].amount = min(VEHICLE_RATE,amount);
                        vehicle_transfers[k][t].amount = min(VEHICLE_RATE,amount);
                        continue;
                    }
                } else if(vehicle_transfers[current][t].type==RSU) {
                    if(!v2r_contacts_real[current][k][t]) {
                        storagecleared += abs(vehicle_transfers[current][t].amount);
                        vehicle_transfers[current][t].clear();
                        rsu_transfers[k][t].erase(current);
                    }
                }
            }
            // for(unordered_set<int>::iterator item=items.begin(); item!=items.end(); item++) {
            //     for(int k=0; k<vehicle_transfers.size(); k++) {
            //         if(k!=current) {
            //             //candidate vehicle requested the item, in contact and free
            //             if((requests[k][*item]<=t) && v2v_contacts[k][current][t] && vehicle_transfers[k][t].id==-1) {
            //                 //update temporary storage
            //                 updateStorage(vehicle_transfers, storage, current, *item, datasizes[*item], t);
            //                 updateStorage(vehicle_transfers, storage, k, *item, datasizes[*item], t);
            //                 //update candidates
            //                 int amount = storage[current][*item].second-storage[k][*item].second;
            //                 if(amount>0) {
            //                     candidates.insert(make_pair(k,*item));
            //                 }
            //             }
            //         }
            //     }
            // }
            // sel_vehicle selv = select_vehicle(candidates,storage,current,datasizes);
            // if(selv.id!=-1) {
            //     vehicle_transfers[current][t].update(selv.id, false, VEHICLE, selv.item, selv.amount);
            //     vehicle_transfers[selv.id][t].update(current, true, VEHICLE, selv.item, selv.amount);
            //     //insertVnew(vnew, selv.id, selv.item);
            // }
        }
        mainstorage = storage;
        vnew.erase(current);
    }
    cout << "Data cleared (cleanup_all): " << storagecleared << endl;
    return;
}

//used to filter out integer timesteps
bool isNumber(string s) {
    bool flag = false;
    for (int i = 0; i < s.length(); i++)
    {
        if(flag && s[i]!='0') {
            return false;
        }
        if(s[i]=='.')
            flag=true;
    }
    return true;
}

float getDistance(float x, float y) {
    return sqrt(x*x + y*y);
}

//vehicles in contact if in range
void updateContact(string id, string conid, float x, float y, float conx, float cony, const set<string> &vehicle_ids, int time, vector<vector<vector<bool>>> &v2v_contacts)
{
    int index = distance(vehicle_ids.begin(), vehicle_ids.find(id));
    int conindex = distance(vehicle_ids.begin(), vehicle_ids.find(conid));
    //cout << index << ", " << conindex;
    if(getDistance(abs(x-conx), abs(y-cony)) <= VEHICLE_RANGE) {
        //cout << "yes" << endl;
        v2v_contacts[index][conindex][time] = true;
    } //else cout << endl;
}

//rsu in contact if in range
void updateRsuContact(string id, float x, float y, const vector<pair<float, float>> &rsu, const set<string> &vehicle_ids, int time, vector<vector<vector<bool>>> &v2r_contacts) {
    int index = distance(vehicle_ids.begin(), vehicle_ids.find(id));
    for(int j=0; j<rsu.size(); j++) {
        if(getDistance(abs(x-rsu[j].first), abs(y-rsu[j].second)) <= RSU_RANGE) {
            v2r_contacts[index][j][time] = true;
        }
    }
}

void updateRealContact(string id, string conid, float x, float y, float conx, float cony, const set<string> &vehicle_ids, int time, vector<vector<vector<bool>>> &v2v_contacts_real)
{
    int index = distance(vehicle_ids.begin(), vehicle_ids.find(id));
    int conindex = distance(vehicle_ids.begin(), vehicle_ids.find(conid));
    //cout << index << ", " << conindex;
    if(getDistance(abs(x-conx), abs(y-cony)) <= VEHICLE_RANGE) {
        //cout << "yes" << endl;
        if(getDistance(abs(x-conx), abs(y-cony)) > 0.8*VEHICLE_RANGE) {
            if((int)(100.0 * rand() / (RAND_MAX + 1.0)) + 1 < V2V_SUCCESS_RATE) {
                v2v_contacts_real[index][conindex][time] = true;
                v2v_contacts_real[conindex][index][time] = true;
            }
        } else {
            v2v_contacts_real[index][conindex][time] = true;
        }
    } //else cout << endl;
}

void updateRealRsuContact(string id, float x, float y, const vector<pair<float, float>> &rsu, const set<string> &vehicle_ids, int time, vector<vector<vector<bool>>> &v2r_contacts_real) {
    int index = distance(vehicle_ids.begin(), vehicle_ids.find(id));
    for(int j=0; j<rsu.size(); j++) {
        if(getDistance(abs(x-rsu[j].first), abs(y-rsu[j].second)) <= RSU_RANGE) {
            if(getDistance(abs(x-rsu[j].first), abs(y-rsu[j].second)) > 0.8*RSU_RANGE) {
                if((int)(100.0 * rand() / (RAND_MAX + 1.0)) + 1 < V2R_SUCCESS_RATE) {
                    v2r_contacts_real[index][j][time] = true;    
                }
            } else {
                v2r_contacts_real[index][j][time] = true;
            }
        }
    }
}

//post-scheduling: get the amount of data actually transferred
float getRealScore(vector<vtransfer> &transfers, int item, int start, int time_units) {
    float score = 0;
    for(int t=start; t<time_units; t++) {
        if(transfers[t].id!=-1 && !transfers[t].isrecv && transfers[t].item==item) {
            score+=transfers[t].amount;
        }
    }
    return score;
}

int main(void) {
    vector<vector<vector<bool>>> v2v_contacts;
    vector<vector<vector<bool>>> v2r_contacts;
    vector<vector<vector<bool>>> v2v_contacts_real;
    vector<vector<vector<bool>>> v2r_contacts_real;
    vector<vector<int>> requests;
    vector<vector<map<int, vehicle_score>>> rsu_transfers;
    map<int, unordered_set<int>> vnew;
    vector<set<pair<int,int>>> rsu_inrange;
    vector<vector<vtransfer>> vehicle_transfers;
    vector<int> datasizes = {300, 500};
    vector<int> scoreTotal = vector<int>(datasizes.size(), 0);

    float distance;
    int time_units;
    int PERIOD;
    vector<pair<float, float>> rsu;

    //cout << "Enter an input file: ";
    string filename;
    cin >> filename;

    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(size+1);
    if (!(file.read(buffer.data(), size)))
    {
        cout << "Could not read file.";
        exit(1);
    }
    buffer[size+1] = '\0';

    string rsufilename;
    //cout << "Enter rsu position file: " << endl;
    cin >> rsufilename;
    //cout << "Enter period duration: " << endl;
    cin >> PERIOD;
    ifstream rsufile(rsufilename);
    float rsux, rsuy;
    while(rsufile >> rsux >> rsuy) {
        rsu.push_back(make_pair(rsux, rsuy));
        //cout << rsux << ", " << rsuy << endl;
    }
    
    rapidxml::xml_document<> doc;
    try {
        doc.parse<0>(buffer.data());
    } catch(rapidxml::parse_error) {
        return 0;
    }

    rapidxml::xml_node<> *fcd = doc.first_node();

    time_units = 0;
    set<string> vehicle_ids;
    for(rapidxml::xml_node<>* timestep = fcd->first_node(); timestep != fcd->last_node(); timestep = timestep->next_sibling()) {
        rapidxml::xml_attribute<>* time = timestep->first_attribute("time");
        if(isNumber(time->value())) {
            time_units++;
            for(rapidxml::xml_node<>* vehicle = timestep->first_node(); vehicle != timestep->last_node(); vehicle = vehicle->next_sibling()) {
                rapidxml::xml_attribute<>* id = vehicle->first_attribute("id");
                string strId = string(id->value());
                vehicle_ids.insert(strId);
            }
        }
    }

    v2v_contacts.resize(vehicle_ids.size(), vector<vector<bool>>(vehicle_ids.size(), vector<bool>(time_units)));
    v2r_contacts.resize(vehicle_ids.size(), vector<vector<bool>>(rsu.size(), vector<bool>(time_units)));
    v2v_contacts_real = v2v_contacts;
    v2r_contacts_real = v2r_contacts;
    int timec = 0;
    for(rapidxml::xml_node<>* timestep = fcd->first_node(); timestep != fcd->last_node(); timestep = timestep->next_sibling()) {
        rapidxml::xml_attribute<>* time = timestep->first_attribute("time");
        if(isNumber(time->value())) {
            int curTime = stoi(time->value());
            //cout << curTime << endl;
            timec++;
            for(rapidxml::xml_node<>* vehicle = timestep->first_node(); vehicle != timestep->last_node(); vehicle = vehicle->next_sibling()) {
                rapidxml::xml_attribute<>* id = vehicle->first_attribute("id");
                rapidxml::xml_attribute<>* x = id->next_attribute("x");
                rapidxml::xml_attribute<>* y = x->next_attribute("y");
                for(rapidxml::xml_node<>* convehicle = timestep->first_node(); convehicle != timestep->last_node(); convehicle = convehicle->next_sibling()) {
                    rapidxml::xml_attribute<>* conid = convehicle->first_attribute("id");
                    rapidxml::xml_attribute<>* conx = conid->next_attribute("x");
                    rapidxml::xml_attribute<>* cony = conx->next_attribute("y");
                    updateContact(string(id->value()), string(conid->value()), stof(string(x->value())), stof(string(y->value())), stof(string(conx->value())), stof(string(cony->value())), vehicle_ids, timec, v2v_contacts);
                    updateRealContact(string(id->value()), string(conid->value()), stof(string(x->value())), stof(string(y->value())), stof(string(conx->value())), stof(string(cony->value())), vehicle_ids, timec, v2v_contacts_real);
                }
                updateRsuContact(string(id->value()), stof(string(x->value())), stof(string(y->value())), rsu, vehicle_ids, timec, v2r_contacts);
                updateRealRsuContact(string(id->value()), stof(string(x->value())), stof(string(y->value())), rsu, vehicle_ids, timec, v2r_contacts_real);
            }
        }
    }
    
    srand(0);
    vector<char>().swap(buffer);

    rsu_transfers.resize(rsu.size(), vector<map<int, vehicle_score>>(time_units, map<int,vehicle_score>()));
    rsu_inrange.resize(rsu.size(), set<pair<int,int>>());
    vehicle_transfers.resize(vehicle_ids.size(), vector<vtransfer>(time_units, vtransfer()));
    requests.resize(vehicle_ids.size(), vector<int>(datasizes.size(), INT_MAX));
    vector<vector<int>> data_lost = vector<vector<int>>(vehicle_transfers.size(), vector<int>(datasizes.size(), 0));
    vector<vector<int>> data_gained = vector<vector<int>>(vehicle_transfers.size(), vector<int>(datasizes.size(), 0));
    vector<float> avg_load = vector<float>(rsu.size(), 0);

    vector<int> ids, ids2, idsall;
    for(int i=0; i<vehicle_ids.size(); i++) {
        idsall.push_back(i);
    }
    random_shuffle(idsall.begin(), idsall.end());
    ids2 = idsall;
    for(int i=0; i<0.5*idsall.size(); i++) {
        //cout << idsall[i] << endl;
        requests[idsall[i]][0] = 0;
        ids.push_back(idsall[i]);
        ids2.erase(ids2.begin());
    }
    for(int i=0; i<ids2.size(); i++) {
        requests[ids2[i]][1] = 0;
    }
    //cout << ids.size() << ", " << ids2.size() << endl;
    random_shuffle(ids.begin(), ids.end());
    random_shuffle(ids2.begin(), ids2.end());
    for(int i=0; i<(0.2)*(ids.size()); i++) {
        requests[ids[i]][1] = 0;
    }
    for(int i=0; i<(0.2)*(ids2.size()); i++) {
        requests[ids2[i]][0] = 0;
    }

    int totalv2v=0, realv2v=0, totalv2r=0, realv2r=0;
    for(int i=0; i<vehicle_ids.size(); i++) {
        for(int t=0; t<time_units; t++) {
            for(int j=0; j<i; j++) {
                if(v2v_contacts[i][j][t]) {
                    totalv2v++;
                } 
                if(v2v_contacts_real[i][j][t]){
                    realv2v++;
                }
            }
            for(int j=0; j<rsu.size(); j++) {
                if(v2r_contacts[i][j][t]) {
                    totalv2r++;
                }
                if(v2r_contacts_real[i][j][t]){
                    realv2r++;
                }
            }
        }
    }
    cout << "V2V ratio: " << realv2v/(float)totalv2v << endl;
    cout << "V2R ratio: " << realv2r/(float)totalv2r << endl;
    // float totalContact = 0, contactCount = 0;
    // for(int i=0; i<vehicle_ids.size(); i++) {
    //     for(int j=0; j<rsu.size(); j++) {
    //         bool flag = false;
    //         for(int t=0; t<time_units; t++) {
    //             if(v2r_contacts[i][j][t]) {
    //                 totalContact++;
    //                 if(!flag) {
    //                     flag = true;
    //                     contactCount++;
    //                 }
    //             } else {
    //                 if(flag)
    //                     flag = false;
    //             }
    //         }
    //     }
    // }
    //cout << "Average contact duration: " << totalContact/contactCount << endl;
    //insert initial storage values and update vnew here
    // init_storage[0][0] = 300;
    // init_storage[0][1] = 500;
    // init_storage[1][1] = 500;
    // map<int,unordered_set<int>>::iterator itr = vnew.insert(make_pair(0, unordered_set<int>({0,1}))).first;
    // vnew.insert(make_pair(1, unordered_set<int>({1})));

    set<pair<int,int>> affectedVehicles;
    map<int,int> rsuCongestion;
    for(int i=0; i<rsu.size(); i++) {
        rsuCongestion.insert(make_pair(0,i));
    }
    //cout << "Initial caches (vehicle ids): ";
    vector<vector<pair<int,int>>> init_storage = vector<vector<pair<int,int>>>(vehicle_transfers.size(), vector<pair<int,int>>(datasizes.size(), make_pair(0,0)));
    //INITIALIZE init_storage HERE

    vector<vector<pair<int,int>>> storage(init_storage);

    updateTransfers(vehicle_transfers, data_lost, data_gained, affectedVehicles, storage, init_storage, requests, vnew, v2v_contacts, datasizes, 0, time_units);
    vector<vector<int>> nextPeriod = vector<vector<int>>(rsu.size(), vector<int>(RSU_MAX, 0));
    vector<int> periodLength = vector<int>(rsu.size(), PERIOD);
    for(int t=0; t<time_units; t++) {
        for(int x=0; x<vehicle_ids.size(); x++) {
            for(int y=0; y<vehicle_ids.size(); y++) {
                //algo learns actual contacts
                v2v_contacts[x][y][t] = v2v_contacts_real[x][y][t];
            }
            for(int j=0; j<rsu.size(); j++) {
                v2r_contacts[x][j][t] = v2r_contacts_real[x][j][t];
            }
        }
        //cout << "Scheduling " << t << endl;
        rsuCongestion.clear();
        for(int j=0; j<rsu.size(); j++) {
            //copy previous transfer list
            if(t>0) {
                rsu_transfers[j][t] = rsu_transfers[j][t-1];
            }
            //remove completed transfers
            for(map<int,vehicle_score>::iterator itr=rsu_transfers[j][t].begin(); itr!=rsu_transfers[j][t].end();) {
                vehicle_score v = itr->second;
                if(v.t_0<=t) {
                    itr = rsu_transfers[j][t].erase(itr);
                } else {
                    itr++;
                }
            }
            //check for nextPeriod of any slot
            bool schedule = false;
            for(int slot=0; slot<RSU_MAX; slot++) {
                if(nextPeriod[j][slot]==t)
                    schedule = true; 
            }
            //update rsu_inrange based on contacts
            for(set<pair<int,int>>::iterator v=rsu_inrange[j].begin(); v!=rsu_inrange[j].end();) {
                if(!v2r_contacts[v->first][j][t]) {
                    v = rsu_inrange[j].erase(v);
                } else {
                    v++;
                }
            }
            //find vehicles that are in range, have open requests
            for(int z=0; z<vehicle_ids.size(); z++) {
                if(v2r_contacts[z][j][t]) {
                    for(int item=0; item<datasizes.size(); item++) {
                        if(requests[z][item]<=t && (rsu_transfers[j][t].find(z)==rsu_transfers[j][t].end())){
                            //if a new vehicle appears that needs data, scheduling is required
                            updateStorage(vehicle_transfers, storage, z, item, datasizes[item], t);
                            if(storage[z][item].second < datasizes[item]) {//&& (storage[z][item].second+getStorage(vehicle_transfers[z], storage[z][item].first, time_units, item, datasizes[item]) < datasizes[item])) {
                                //if vehicle already in set, no need to schedule immediately
                                if(rsu_inrange[j].insert(make_pair(z,item)).second) {
                                    //cout << "schedule at: " << t << endl;
                                    schedule = true;
                                }
                            }
                        }
                    }
                }
            }
            avg_load[j] += rsu_inrange[j].size();
            if(schedule)
                rsuCongestion.insert(make_pair(rsu_inrange[j].size(), j));
        }
        for(map<int,int>::iterator r=rsuCongestion.begin(); r!=rsuCongestion.end(); r++) {
            int j = r->second;
            for(int slot=0; slot<RSU_MAX; slot++) {
                if(nextPeriod[j][slot]<=t) {
                    vector<vehicle_score> scores;
                    //schedule normally
                    for(set<pair<int,int>>::iterator itr = rsu_inrange[j].begin(); itr!=rsu_inrange[j].end(); itr++) {
                        int z = itr->first;
                        int item = itr->second;
                        if(storage[z][item].first!=t) {
                            updateStorage(vehicle_transfers, storage, z, item, datasizes[item], t);
                        }
                        //find t_0: complete the request/complete the period/leave rsu range
                        int t_0 = min({t + ceil((datasizes[item]-storage[z][item].second)/(double)RSU_RATE), (double) t+periodLength[j], (double) time_units});
                        for(int c = t; c<t_0; c++) {
                            if(!v2r_contacts[z][j][c] || vehicle_transfers[z][c].type==RSU) {
                                t_0 = c;
                                break;
                            }
                        }
                        //get the vehicle score and add it to list of candidates
                        if(t_0 > t) {
                            float score = get_score(vehicle_transfers, storage, requests, v2v_contacts, z, j, t, t_0, time_units, item, datasizes);
                            scores.push_back(vehicle_score(z, item, score, t_0));
                        }
                    }
                    vehicle_score v_best = rsu_select_vehicle(scores);
                    if(v_best.id == -1) {
                        continue;
                    } else {
                        scoreTotal[v_best.item] += v_best.score;
                        rsu_transfers[j][t].insert(make_pair(v_best.id, v_best));
                        for(set<pair<int,int>>::iterator itr=rsu_inrange[j].upper_bound(make_pair(v_best.id, -1)); itr!=rsu_inrange[j].end() && itr->first==v_best.id;) {
                            itr = rsu_inrange[j].erase(itr);
                        }
                        vnew.insert(make_pair(v_best.id, unordered_set<int>({v_best.item})));
                        for(int rsut=t; rsut<v_best.t_0; rsut++) {
                            //if a vehicular transfer exists, clear it
                            int other = vehicle_transfers[v_best.id][rsut].id;
                            int item = vehicle_transfers[v_best.id][rsut].item;
                            if(other!=-1) {
                                if(vehicle_transfers[v_best.id][rsut].type==VEHICLE) {
                                    if(!vehicle_transfers[v_best.id][rsut].isrecv) {
                                        data_lost[other][item] += vehicle_transfers[v_best.id][rsut].amount;
                                    }
                                    vehicle_transfers[other][rsut].clear();
                                    affectedVehicles.insert(make_pair(other, item));
                                }
                            }
                            //set this vehicle to receive from rsu
                            if(rsut==v_best.t_0-1) {
                                int lastStorage = storage[v_best.id][v_best.item].second+getStorage(vehicle_transfers[v_best.id], t, rsut, v_best.item, datasizes[v_best.item]);
                                if(datasizes[v_best.item]-lastStorage < RSU_RATE) {
                                    vehicle_transfers[v_best.id][rsut].update(j, true, RSU, v_best.item, datasizes[v_best.item]-lastStorage);
                                    break;
                                }
                            }
                            vehicle_transfers[v_best.id][rsut].update(j, true, RSU, v_best.item, RSU_RATE);
                        }
                        updateTransfers(vehicle_transfers, data_lost, data_gained, affectedVehicles, storage, init_storage,requests, vnew, v2v_contacts, datasizes, t, time_units);
                    }
                    nextPeriod[j][slot]=v_best.t_0;
                }
            }
        }
    }

    for(int i=0; i<vehicle_ids.size(); i++) {
        for(int d=0; d<datasizes.size(); d++) {
            if(requests[i][d]<time_units) {
                insertVnew(vnew, i, d);
            }
        }
    }
    //cleanup(vehicle_transfers, vnew, requests, storage, init_storage, v2v_contacts, datasizes, time_units);
    cleanup_all(vehicle_transfers, rsu_transfers, vnew, requests, storage, init_storage, v2v_contacts_real, v2r_contacts_real, datasizes, time_units);
    vector<float> avg = vector<float>(datasizes.size(), 0);
    vector<int> counts = vector<int>(datasizes.size(), 0);
    vector<float> remaining_data = vector<float>(datasizes.size(), 0);
    vector<float> required_data = vector<float>(datasizes.size(), 0);
    vector<int> satrequests = vector<int>(datasizes.size(), 0);
    vector<int> reqnear = vector<int>(datasizes.size(), 0);
    vector<int> numrequests = vector<int>(datasizes.size(), 0);

    for(int i=0; i<storage.size(); i++) {
        fill(storage[i].begin(), storage[i].end(), make_pair(0,0));
    }

    for(int k=0; k<vehicle_transfers.size(); k++) {
        for(int item=0; item<datasizes.size(); item++) {
            if(storage[k][item].first < time_units) {
                updateStorage(vehicle_transfers, storage, k, item, datasizes[item], time_units);
            }
            if(requests[k][item] < time_units) {
                avg[item] += storage[k][item].second;
                counts[item]++;
                numrequests[item]++;
                required_data[item] += datasizes[item];
                remaining_data[item] += datasizes[item] - storage[k][item].second;
                if(storage[k][item].second>=datasizes[item])
                    satrequests[item]++;
                if(datasizes[item]-storage[k][item].second < 0.1*datasizes[item]) {
                    reqnear[item]++;
                }
            }
        }
    }
    cout << "********************** " << filename << ", " << rsufilename << ", period=" << PERIOD << " **********************" << endl;
    cout << "Averages: " << endl;
    for(int i=0; i<avg.size(); i++) {
        avg[i]/=counts[i];
        cout << "\tItem " << i << ": " << avg[i] << endl;
    }
    
    cout << "Required data: " << endl;
    for(int i=0; i<required_data.size(); i++) {
        cout << "\tItem " << i << ": " << required_data[i] << endl;
    }
    cout << "Remaining data: " << endl;
    for(int i=0; i<remaining_data.size(); i++) {
        cout << "\tItem " << i << ": " << remaining_data[i] << endl;
    }
    cout << "Requests: " << endl;
    for(int i=0; i<numrequests.size(); i++) {
        cout << "\tItem " << i << ": " << "Number of requests: " << numrequests[i] << ", Satisfied requests: " << satrequests[i] << endl;
    }

    vector<float> v2vtotal = vector<float>(datasizes.size(), 0);
    vector<float> rsutotal = vector<float>(datasizes.size(), 0);
    for(int i=0; i<vehicle_transfers.size(); i++) {
        for(int t=0; t<time_units; t++) {
            if(vehicle_transfers[i][t].id!=-1 && vehicle_transfers[i][t].isrecv) {
                if(vehicle_transfers[i][t].type==VEHICLE) {
                    v2vtotal[vehicle_transfers[i][t].item] += vehicle_transfers[i][t].amount;
                } else {
                    rsutotal[vehicle_transfers[i][t].item] += vehicle_transfers[i][t].amount;
                }
            }
        }
    }

    for(int i=0; i<storage.size(); i++) {
        fill(storage[i].begin(), storage[i].end(), make_pair(0,0));
    }
    float v2v_max = 0;
    float v2v_miss = 0;
    for(int t=0; t<time_units; t++) {
        int count = 0;
        vector<bool> recvehicles = vector<bool>(vehicle_ids.size(), false);
        for(int i=0; i<vehicle_ids.size(); i++) {
            for(int j=0; j<vehicle_ids.size(); j++) {
                if(j!=i && v2v_contacts[i][j][t]) {
                    for(int d=0; d<datasizes.size(); d++) {
                        updateStorage(vehicle_transfers, storage, i, d, datasizes[d], t);
                        updateStorage(vehicle_transfers, storage, j, d, datasizes[d], t);
                        if(storage[i][d].second > storage[j][d].second) {
                            if(!recvehicles[j] && !recvehicles[i]) {
                                v2v_max += min(storage[i][d].second-storage[j][d].second, VEHICLE_RATE);
                                recvehicles[j] = true;
                                recvehicles[i] = true;
                                count++;
                                if(vehicle_transfers[i][t].id==-1 && vehicle_transfers[j][t].id==-1) {
                                    //cout << storage[i][d].second << " " << storage[j][d].second << endl;
                                    v2v_miss += min(storage[i][d].second-storage[j][d].second, VEHICLE_RATE);
                                }
                            }
                        }
                    }
                }
            }
        }
        //cout << count << endl;
    }

    cout << "Data by source: " << endl;
    for(int i=0; i<v2vtotal.size(); i++) {
        cout << "\tItem " << i << "- V2V Transfers: " << v2vtotal[i] << ", RSU Transfers: " << rsutotal[i] << endl;
    }

    cout << "Overall data: " << endl;
    float rsuTime = 0, rsuUsed = 0;
    float v2vtot=0, rsutot=0, remtot=0, reqtot=0;
    int totreqnum=0, satreq=0, reqneartot=0;
    int totscore=0;
    float selTot=0;
    for(int d = 0; d<datasizes.size(); d++) {
        v2vtot+=v2vtotal[d];
        rsutot+=rsutotal[d];
        remtot+=remaining_data[d];
        reqtot+=required_data[d];
        totreqnum+=numrequests[d];
        satreq+=satrequests[d];
        totscore+=scoreTotal[d];
        reqneartot+=reqnear[d];
    }
    for(int k=0; k<vehicle_transfers.size(); k++) {
        int item = -1, rsu = -1;
        for(int t=0; t<time_units; t++) {
            if(vehicle_transfers[k][t].type==RSU) {
                //entered rsu transfer
                if(rsu==-1) {
                    rsu = vehicle_transfers[k][t].id;
                    item = vehicle_transfers[k][t].item;
                } else if(rsu!=vehicle_transfers[k][t].id || item!=vehicle_transfers[k][t].item) {
                    //switched to another rsu/item pair directly
                    //cout << "Test" << endl;
                    selTot+=getRealScore(vehicle_transfers[k], item, t, time_units);
                    rsu = vehicle_transfers[k][t].id;
                    item = vehicle_transfers[k][t].item;
                }
            } else {
                //exited rsu transfer
                if(rsu!=-1) {
                    //cout << "Test" << endl;
                    selTot+=getRealScore(vehicle_transfers[k], item, t, time_units);
                    rsu = -1;
                    item = -1;
                }
            }
        }
    }
    cout << "Required Total: " << reqtot << endl;
    cout << "V2V Total: " << v2vtot << ", RSU Total: " << rsutot << ", Remaining Total: " << remtot << endl;
    cout << "Total requests: " << totreqnum << ", Satisfied: " << satreq << " Requests within 10 percent: " << reqneartot << endl;
    for(int i=0; i<rsu.size(); i++) {
        for(int t=0; t<time_units; t++) {
            int count=0;
            for(int v=0; v<vehicle_ids.size(); v++) {
                if(v2r_contacts[v][i][t]) {
                    count++;
                }
            }
            if(count>RSU_MAX)
                rsuTime+=RSU_MAX;
            else
                rsuTime+=count;
            rsuUsed += rsu_transfers[i][t].size();
        }
        avg_load[i]/=time_units;
    }
    cout << "RSU utilization: " << rsuUsed*100/rsuTime << "%" << endl;
    cout << "Score accuracy: " << selTot*100/totscore << "%" << endl;
    cout << "V2V Max: " << v2v_max << endl;
    cout << "V2V utilization: " << v2vtot*100/v2v_max << "%" << endl;
    cout << "V2V missed: " << v2v_miss << " percent: " << v2v_miss*100/v2v_max << endl;
    float sum = 0;
    for(int i=0; i<rsu.size(); i++) {
        //cout << avg_load[i] << endl;
        sum += avg_load[i];
    }
    cout << "Average RSU load: " << sum/rsu.size() << endl;
    cout << "**************************************************************************************************************" << endl;
    // cout << "Total number of caches: " << caches.size() << endl;
    // float remaining_data = 0;
    // for(int i=0; i<vehicle_ids.size(); i++) {
    //     remaining_data += datasize - storage[i][time_units-1];
    // }
    // cout << "Total data: " << vehicle_ids.size()*datasize << " Remaining data: " << remaining_data  << " V2V transfers: " << v2vtotal << " RSU transfers: " << rsutotal << endl;
    return 0;
}