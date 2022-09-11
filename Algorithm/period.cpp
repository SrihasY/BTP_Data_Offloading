#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <set>
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
#define VEHICLE_RATE 2
#define RSU_MAX 2
#define CELL_RATE 10
#define W_SAT 1.5
#define W_NSAT 1
#define RECURSION_CUTOFF 0.4

#define RSU -2
#define CELLULAR -1
#define VEHICLE 0

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

typedef struct sel_vehicle {
    int id;
    int item;
    int amount;
    sel_vehicle() : id(-1), item(-1), amount(0) {
    }
    sel_vehicle(int id, int item, int amount) : id(id), item(item), amount(amount) {
    }
} sel_vehicle;

sel_vehicle select_vehicle(set<pair<int,int>> &candidates, vector<vector<pair<int,int>>> &storage, int srcvehicle, vector<int> &datasizes) {
    int mingap = INT_MAX;
    sel_vehicle selv = sel_vehicle();
    for(set<pair<int,int>>::iterator itr=candidates.begin(); itr!=candidates.end(); itr++) {
        int recv = itr->first;
        int item = itr->second;
        if(datasizes[item]-storage[recv][item].second < mingap) {
            mingap = datasizes[item]-storage[recv][item].second;
            selv.id = recv;
            selv.item = item;
            selv.amount = min(storage[srcvehicle][item].second-storage[recv][item].second, VEHICLE_RATE);
        }
    }
    return selv;
}

vehicle_score rsu_select_vehicle(vector<vehicle_score> &scores) {
    vehicle_score vbest = vehicle_score();
    for(int i=0; i<scores.size(); i++) {
        if(scores[i].score >= vbest.score) {
            vbest = scores[i];
        }
    }
    //if(!scores.empty() && vbest.id==-1)
        //cout << "All negative scores" << endl;
    return vbest;
}

// int lossAmount(vector<vector<vtransfer>> &vehicle_transfers, int vehicle, int start, int t_0) {
//     int loss = 0;
//     for(int t=start; t<t_0; t++) {
//         if(vehicle_transfers[vehicle][t].id!=-1 && !vehicle_transfers[vehicle][t].isrecv) {
//             loss += vehicle_transfers[vehicle][t].amount;
//         }
//     }
//     return loss;
// }

int getStorage(vector<vtransfer> &transfers, int start, int end, int item, int datasize) {
    int storage=0;
    for(int t=start; t<end; t++) {
        if(transfers[t].item==item && transfers[t].isrecv) {
            // if(transfers[t].type==RSU) {
            //     storage += RSU_RATE;
            // } else if(transfers[t].type==CELLULAR) {
            //     storage += CELL_RATE;
            // } else {
            //     storage += VEHICLE_RATE;
            // }
            storage+=transfers[t].amount;
        }
    }
    if(storage>datasize) {
        // for(int t=0; t<end; t++) {
        //     if(transfers[t].item==item && transfers[t].isrecv) {
        //         cout << "t=" << t << ", amount = " << transfers[t].amount << ", type = " << transfers[t].type << endl;
        //     }
        // }
        // exit(-1);
        return datasize;
    }
    return storage;
}

float get_score(vector<vector<vtransfer>> temp_transfers, vector<vector<pair<int,int>>> storage, vector<vector<int>> &requests, vector<vector<vector<bool>>> &v2v_contacts, int vehicle, int rsu, int start, int t_0, int time_units, int item, vector<int> &datasizes)
{
    float score = 0;
    //vehicle receives data from the rsu in temporary transfers
    for(int t=start; t<t_0; t++) {
        if(temp_transfers[vehicle][t].id!=-1 && !temp_transfers[vehicle][t].isrecv) {
            //score -= temp_transfers[vehicle][t].amount;
        }
        temp_transfers[vehicle][t].update(rsu, true, RSU, item, RSU_RATE);
    }
    vector<int> data_transfer = vector<int>(temp_transfers.size(), 0);
    set<pair<int,int>> candidates;
    for(int t=t_0; t<time_units; t++) {
        if(temp_transfers[vehicle][t].id!=-1) {
            //if there is already a transfer, check if more can be transferred than before
            int transfer_item = temp_transfers[vehicle][t].item;
            int k=temp_transfers[vehicle][t].id;
            if(temp_transfers[vehicle][t].type==VEHICLE && temp_transfers[vehicle][t].isrecv==false && temp_transfers[vehicle][t].amount < VEHICLE_RATE) {
                //update temporary storage
                storage[vehicle][transfer_item].second = storage[vehicle][transfer_item].second+getStorage(temp_transfers[vehicle], storage[vehicle][transfer_item].first, t, transfer_item, datasizes[transfer_item]);
                storage[vehicle][transfer_item].first = t;
                storage[k][transfer_item].second = storage[k][vehicle].second+getStorage(temp_transfers[k], storage[k][transfer_item].first, t, transfer_item, datasizes[transfer_item]);
                storage[k][transfer_item].first = t;
                int amount = storage[vehicle][transfer_item].second-storage[k][transfer_item].second;
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
                    storage[vehicle][item].second = storage[vehicle][item].second+getStorage(temp_transfers[vehicle], storage[vehicle][item].first, t, item, datasizes[item]);
                    storage[vehicle][item].first = t;
                    storage[k][item].second = storage[k][item].second+getStorage(temp_transfers[k], storage[k][item].first, t, item, datasizes[item]);
                    storage[k][item].first = t;
                    //update candidates
                    if(storage[vehicle][item].second-storage[k][item].second > 0)
                        candidates.insert(make_pair(k,item));
                }
            }
        }
        sel_vehicle selv = select_vehicle(candidates, storage, vehicle, datasizes);        
        if(selv.id!=-1) {
            temp_transfers[vehicle][t].update(selv.id, false, VEHICLE, selv.item, selv.amount);
            temp_transfers[selv.id][t].update(vehicle, true, VEHICLE, selv.item, selv.amount);
            data_transfer[selv.id] += selv.amount;
        }
        candidates.clear();
    }
    for(int k=0; k<temp_transfers.size(); k++) {
        if(storage[k][item].first < time_units) {
            storage[k][item].second = storage[k][item].second+getStorage(temp_transfers[k], storage[k][item].first, time_units, item, datasizes[item]);
            storage[k][item].first = time_units;
        }
        if(data_transfer[k] >= datasizes[item]-storage[k][item].second) {
            score += W_SAT*(datasizes[item]-storage[k][item].second);
        } else {
            score += W_NSAT*data_transfer[k];
        }
    }
    return score;
}

void updateTransfers(vector<vector<vtransfer>> &vehicle_transfers, vector<vector<pair<int,int>>> &start_storage, vector<vector<int>> &requests, map<int,unordered_set<int>> &vnew, vector<vector<vector<bool>>> &v2v_contacts, vector<int> &datasizes, int start, int time_units)
{
    while(!(vnew.empty())) {
        int current = vnew.begin()->first;
        unordered_set<int> items = vnew.begin()->second;
        set<pair<int,int>> candidates;
        //cout << current << endl;
        vector<vector<pair<int,int>>> storage = start_storage;
        for(int t=start; t<time_units; t++) {
            candidates.clear();
            //if a transfer is already scheduled, check for partial utilization/transfers to be cancelled
            if(vehicle_transfers[current][t].id!=-1) {
                int transfer_item = vehicle_transfers[current][t].item;
                int k=vehicle_transfers[current][t].id;
                if(vehicle_transfers[current][t].type==VEHICLE) {
                    //update temporary storage
                    storage[current][transfer_item].second = storage[current][transfer_item].second+getStorage(vehicle_transfers[current], storage[current][transfer_item].first, t, transfer_item, datasizes[transfer_item]);
                    storage[current][transfer_item].first = t;
                    storage[k][transfer_item].second = storage[k][transfer_item].second+getStorage(vehicle_transfers[k], storage[k][transfer_item].first, t, transfer_item, datasizes[transfer_item]);
                    storage[k][transfer_item].first = t;
                    int amount = storage[current][transfer_item].second-storage[k][transfer_item].second;
                    //cout << amount << endl;
                    if(vehicle_transfers[current][t].isrecv) {
                        amount *= -1;
                    }
                    if(amount<=0) {
                        //source has less data than receiver now, remove this transfer
                        if(!vehicle_transfers[current][t].isrecv){
                            //if receiving vehicle loses data, add to vnew
                            map<int,unordered_set<int>>::iterator pos = vnew.find(k);
                            if(pos==vnew.end()) {
                                vnew.insert(make_pair(k, unordered_set<int>({transfer_item})));
                            } else {
                                pos->second.insert(transfer_item);
                            }
                        }
                        vehicle_transfers[current][t].clear();
                        vehicle_transfers[k][t].clear();
                    } else if(amount!=vehicle_transfers[current][t].amount) {
                        //update amount
                        vehicle_transfers[current][t].amount = min(VEHICLE_RATE,amount);
                        vehicle_transfers[k][t].amount = min(VEHICLE_RATE,amount);
                    }
                }
                continue;
            }
            for(unordered_set<int>::iterator item=items.begin(); item!=items.end(); item++) {
                for(int k=0; k<vehicle_transfers.size(); k++) {
                    if(k!=current) {
                        //candidate vehicle requested the item, in contact and free
                        if((requests[k][*item]<=t) && v2v_contacts[k][current][t] && vehicle_transfers[k][t].id==-1) {
                            //update temporary storage
                            storage[current][*item].second = storage[current][*item].second+getStorage(vehicle_transfers[current], storage[current][*item].first, t, *item, datasizes[*item]);
                            storage[current][*item].first = t;
                            storage[k][*item].second = storage[k][*item].second+getStorage(vehicle_transfers[k], storage[k][*item].first, t, *item, datasizes[*item]);
                            storage[k][*item].first = t;
                            //update candidates
                            int amount = storage[current][*item].second-storage[k][*item].second;
                            if(amount>0 && storage[current][*item].second < datasizes[*item])
                                candidates.insert(make_pair(k,*item));
                        }
                    }
                }
            }
            sel_vehicle selv = select_vehicle(candidates,storage,current,datasizes);
            if(selv.id!=-1) {
                vehicle_transfers[current][t].update(selv.id, false, VEHICLE, selv.item, selv.amount);
                vehicle_transfers[selv.id][t].update(current, true, VEHICLE, selv.item, selv.amount);
                //recursion with bound
                if(getStorage(vehicle_transfers[selv.id], storage[selv.id][selv.item].first, time_units, selv.item, datasizes[selv.item]) > RECURSION_CUTOFF*datasizes[selv.item]) {
                    map<int,unordered_set<int>>::iterator pos = vnew.find(selv.id);
                    if(pos==vnew.end()) {
                        vnew.insert(make_pair(selv.id, unordered_set<int>({selv.item})));
                    } else {
                        pos->second.insert(selv.item);
                    }
                }
            }
        }
        vnew.erase(current);
        //cout << current << " ";
    }
    //cout << endl;
    return;
}

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

void updateContact(string id, string conid, float x, float y, float conx, float cony, const set<string> &vehicle_ids, int time, vector<vector<vector<bool>>> &v2v_contacts)
{
    int index = distance(vehicle_ids.begin(), vehicle_ids.find(id));
    int conindex = distance(vehicle_ids.begin(), vehicle_ids.find(conid));
    //cout << index << ", " << conindex;
    if(abs(getDistance(x, y)-getDistance(conx, cony)) <= VEHICLE_RANGE) {
        //cout << "yes" << endl;
        v2v_contacts[index][conindex][time] = true;
    } //else cout << endl;
}

void updateRsuContact(string id, float x, float y, const vector<pair<float, float>> &rsu, const set<string> &vehicle_ids, int time, vector<vector<vector<bool>>> &v2r_contacts) {
    int index = distance(vehicle_ids.begin(), vehicle_ids.find(id));
    for(int j=0; j<rsu.size(); j++) {
        if(abs(getDistance(x, y)-getDistance(rsu[j].first, rsu[j].second)) <= RSU_RANGE) {
            v2r_contacts[index][j][time] = true;
        }
    }
}

int main(void) {
    vector<vector<vector<bool>>> v2v_contacts;
    vector<vector<vector<bool>>> v2r_contacts;
    vector<vector<int>> requests;
    vector<vector<map<int, vehicle_score>>> rsu_transfers;
    map<int, unordered_set<int>> vnew;
    vector<set<pair<int,int>>> rsu_inrange;
    vector<vector<vtransfer>> vehicle_transfers;
    vector<vector<int>> init_storage;
    vector<int> datasizes = {300, 500};

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
                }
                updateRsuContact(string(id->value()), stof(string(x->value())), stof(string(y->value())), rsu, vehicle_ids, timec, v2r_contacts);
            }
        }
    }

    vector<char>().swap(buffer);

    rsu_transfers.resize(rsu.size(), vector<map<int, vehicle_score>>(time_units, map<int,vehicle_score>()));
    rsu_inrange.resize(rsu.size(), set<pair<int,int>>());
    vehicle_transfers.resize(vehicle_ids.size(), vector<vtransfer>(time_units, vtransfer()));
    init_storage.resize(vehicle_ids.size(), vector<int>(datasizes.size(), 0));
    requests.resize(vehicle_ids.size(), vector<int>(datasizes.size(), INT_MAX));

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

    map<int,int> rsuCongestion;
    for(int i=0; i<rsu.size(); i++) {
        rsuCongestion.insert(make_pair(0,i));
    }
    //cout << "Initial caches (vehicle ids): ";
    vector<vector<pair<int,int>>> storage = vector<vector<pair<int,int>>>(vehicle_transfers.size(), vector<pair<int,int>>(datasizes.size(), make_pair(0,0)));
    for(int i=0; i<vehicle_transfers.size(); i++) {
        for(int k=0; k<datasizes.size(); k++) {
            if(init_storage[i][k])
                storage[i][k].second = init_storage[i][k];
        }
    }
    updateTransfers(vehicle_transfers, storage, requests, vnew, v2v_contacts, datasizes, 0, time_units);
    vector<vector<int>> nextPeriod = vector<vector<int>>(rsu.size(), vector<int>(RSU_MAX, 0));
    vector<int> periodLength = vector<int>(rsu.size(), PERIOD);
    for(int t=0; t<time_units; t++) {
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
                            storage[z][item].second = storage[z][item].second+getStorage(vehicle_transfers[z], storage[z][item].first, t, item, datasizes[item]);
                            storage[z][item].first = t;
                            if(storage[z][item].second < datasizes[item] && (storage[z][item].second+getStorage(vehicle_transfers[z], storage[z][item].first, time_units, item, datasizes[item]) < datasizes[item])) {
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
                            storage[z][item].second = storage[z][item].second+getStorage(vehicle_transfers[z], storage[z][item].first, t, item, datasizes[item]);
                            storage[z][item].first = t;
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
                        rsu_transfers[j][t].insert(make_pair(v_best.id, v_best));
                        for(set<pair<int,int>>::iterator itr=rsu_inrange[j].upper_bound(make_pair(v_best.id, -1)); itr!=rsu_inrange[j].end() && itr->first==v_best.id;) {
                            itr = rsu_inrange[j].erase(itr);
                        }
                        vnew.insert(make_pair(v_best.id, unordered_set<int>({v_best.item})));
                        for(int rsut=t; rsut<v_best.t_0; rsut++) {
                            //if a vehicular transfer exists, clear it
                            int other;
                            if((other=vehicle_transfers[v_best.id][rsut].id)!=-1) {
                                if(vehicle_transfers[v_best.id][rsut].type==VEHICLE) {
                                    if(!vehicle_transfers[v_best.id][rsut].isrecv) {
                                        //if a receiving vehicle is impacted due to this rsu transfer, storage changes occur
                                        map<int,unordered_set<int>>::iterator pos = vnew.find(other);
                                        if(pos==vnew.end()) {
                                            vnew.insert(make_pair(other, unordered_set<int>({v_best.item})));
                                        } else {
                                            pos->second.insert(v_best.item);
                                        }
                                    }
                                    vehicle_transfers[other][rsut].clear();
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
                        updateTransfers(vehicle_transfers, storage, requests, vnew, v2v_contacts, datasizes, t, time_units);
                    }
                    nextPeriod[j][slot]=v_best.t_0;
                }
            }
        }
    }

    // for(int i=0; i<rsu.size(); i++) {
    //     cout << "RSU " << i << endl;
    //     for(int t=0; t<time_units; t++) {
    //         if(!rsu_transfers[i][t].empty()) {
    //             cout << "Time " << t << ": ";
    //             for(map<int, vehicle_score>::iterator itr = rsu_transfers[i][t].begin(); itr !=rsu_transfers[i][t].end();itr++) {
    //                 cout << itr->first << ", ";
    //             }
    //             cout << endl;
    //         }
    //     }
    //     cout << endl;
    // }

    vector<float> avg = vector<float>(datasizes.size(), 0);
    vector<int> counts = vector<int>(datasizes.size(), 0);
    vector<float> remaining_data = vector<float>(datasizes.size(), 0);
    vector<float> required_data = vector<float>(datasizes.size(), 0);
    for(int k=0; k<vehicle_transfers.size(); k++) {
        for(int item=0; item<datasizes.size(); item++) {
            if(storage[k][item].first < time_units) {
                storage[k][item].second = storage[k][item].second+getStorage(vehicle_transfers[k], storage[k][item].first, time_units, item, datasizes[item]);
                storage[k][item].first = time_units;
            }
            if(requests[k][item] < time_units) {
                avg[item] += storage[k][item].second;
                counts[item]++;
                required_data[item] += datasizes[item];
                remaining_data[item] += datasizes[item] - storage[k][item].second;
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

    vector<float> v2vtotal = vector<float>(datasizes.size(), 0);
    vector<float> rsutotal = vector<float>(datasizes.size(), 0);
    for(int i=0; i<vehicle_transfers.size(); i++) {
        for(int t=0; t<time_units; t++) {
            if(vehicle_transfers[i][t].id!=-1 && vehicle_transfers[i][t].isrecv==true) {
                if(vehicle_transfers[i][t].type==VEHICLE) {
                    v2vtotal[vehicle_transfers[i][t].item] += vehicle_transfers[i][t].amount;
                } else {
                    rsutotal[vehicle_transfers[i][t].item] += vehicle_transfers[i][t].amount;
                }
            }
        }
    }

    cout << "Data by source: " << endl;
    for(int i=0; i<v2vtotal.size(); i++) {
        cout << "\tItem " << i << "- V2V Transfers: " << v2vtotal[i] << ", RSU Transfers: " << rsutotal[i] << endl;
    }

    cout << "Overall data: " << endl;
    float rsuTime = 0, rsuUsed = 0;
    float v2vtot=0, rsutot=0, remtot=0, reqtot=0;
    for(int d = 0; d<datasizes.size(); d++) {
        v2vtot+=v2vtotal[d];
        rsutot+=rsutotal[d];
        remtot+=remaining_data[d];
        reqtot+=required_data[d];
    }
    cout << "Required Total: " << reqtot << endl;
    cout << "V2V Total: " << v2vtot << ", RSU Total: " << rsutot << ", Remaining Total: " << remtot << endl;
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
    }
    cout << "RSU utilization: " << rsuUsed*100/rsuTime << "%" << endl;
    cout << "**************************************************************************************************************" << endl;
    // cout << "Total number of caches: " << caches.size() << endl;
    // float remaining_data = 0;
    // for(int i=0; i<vehicle_ids.size(); i++) {
    //     remaining_data += datasize - storage[i][time_units-1];
    // }
    // cout << "Total data: " << vehicle_ids.size()*datasize << " Remaining data: " << remaining_data  << " V2V transfers: " << v2vtotal << " RSU transfers: " << rsutotal << endl;
    return 0;
}