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
    int groupVersion;//�汾��
    uint32_t groupid;
    int state = 1;//0--leader, 1--follower, 2---ca
    int max_log_id;
    string storage_path;
    Storage *storage;
    stack<task_t *> *g_readwrite_task;

public:
    MultiPaxos() {}
    virtual ~MultiPaxos() {}
    //ͬ��Propose
    //���� 0--�ɹ�,
    //д������־  -11--δ֪, -12--��ȷ��ʧ��
    //�ѱ�������־ռ��  -20--ͬ��������־�ɹ�, -21--ͬ���Ѵ��ڵ���־���δ֪ -22---��ȷ��ʧ��
    int syncPropose(void **pPaxosData, Proto::Network::CliReq *cliReq, uint64_t &outLogId);
    int asyncPropose();
    int getValue();

    MultiPaxos(const Proto::Config::PaxosGroup &group, int host_id);

    int start();

    void mainThreadFun(int threadIndex);

    void workerCoroutine(task_t *pTask);

    void acceptCoroutine(task_t *pTask);

    //��������, prepare,ѡ�ٵ�
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
    int groupVersion;//�汾��
    map<uint32_t , HostInfo> mapHostInfo; //hostid-->HostInfo
    int updateHostInfo();
};

class HostLogInfo {
public:
    Proto::Storage::ValueData *valueDataArr;
    int *dealFlagArr; // -1---ʧ�� 0--δ����, 1--�Ѵ���
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
