//
// Created by hyj on 2020-04-11.
//

#ifndef PROJECT_MULTIPAXOS_H
#define PROJECT_MULTIPAXOS_H

#include <string>
#include <stack>
#include <config.pb.h>
#include <rpc.pb.h>
#include "Utils.h"
#include "Storage.h"

using namespace std;

class MultiPaxos {
private:
    int listenFd;
    int host_id = -1;
    Proto::Config::PaxosGroup groupConfig;
    int groupVersion;//版本号
    uint32_t groupid;
    int state = 1;//0--leader, 1--follower, 2---ca
    int max_log_id;
    string storage_path;
    Storage *storage;
    stack<task_t *> *g_readwrite_task;

public:
    MultiPaxos() {}
    virtual ~MultiPaxos() {}
    //同步Propose
    //返回 0--成功,
    //写本条日志  -11--未知, -12--明确的失败
    //已被其他日志占用  -20--同步其他日志成功, -21--同步已存在的日志结果未知 -22---明确的失败
    int syncPropose(void **pPaxosData, Proto::Network::CliReq *cliReq, uint64_t &outLogId);
    int asyncPropose();
    int getValue();

    MultiPaxos(const Proto::Config::PaxosGroup &group, int host_id);

    int start();

    void mainThreadFun(int threadIndex);

    void workerCoroutine(task_t *pTask);

    void acceptCoroutine(task_t *pTask);

    //处理心跳, prepare,选举等
    void adminWorkerThread();

    uint64_t newProposeId();
};

class HostInfo {
public:
    uint32_t id;
    string ip;
    uint32_t port;
    int fd;
    int groupIndex;//
};

class PaxosProposeHandler {
public:
    Proto::Config::PaxosGroup group;
    int groupVersion;//版本号
    map<uint32_t , HostInfo> mapHostInfo; //hostid-->HostInfo
    int updateHostInfo();
};

class HostLogInfo {
public:
    Proto::Storage::ValueData *valueDataArr;
    int *dealFlagArr; // -1---失败 0--未处理, 1--已处理
    int size = 0;
    uint64_t maxMajorityValueId = 0;
    int maxMajorityCount = 0;

    HostLogInfo(int size) {
        this->size = size;
        valueDataArr = new Proto::Storage::ValueData[size];
        dealFlagArr = new int[size];
        setDealFlagAll(0);
    }
    void setDealFlagAll(int n) {
        for (int i = 0; i < size; ++i) {
            dealFlagArr[i] = n;
        }
    }
    ~HostLogInfo() {
        delete[](valueDataArr);
        delete[](dealFlagArr);
    }
    int calcMajority() {
        map<uint64_t , int> mapValueCount;
        for (int k = 0; k < size; ++k) {
            if (valueDataArr[k].value_id() == 0) {
                continue;
            }
            auto it = mapValueCount.find(valueDataArr[k].value_id());
            if (it == mapValueCount.end()) {
                mapValueCount.insert(make_pair(valueDataArr[k].value_id(), 1));
            } else {
                it->second += 1;
            }
        }
        auto it = mapValueCount.begin();
        for (; it != mapValueCount.end(); ++it) {
            if (maxMajorityCount < it->second) {
                maxMajorityCount = it->second;
                maxMajorityValueId = it->first;
            }
        }
        return maxMajorityCount;
    }
};


#endif //PROJECT_MULTIPAXOS_H
