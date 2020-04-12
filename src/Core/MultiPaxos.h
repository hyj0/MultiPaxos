//
// Created by hyj on 2020-04-11.
//

#ifndef PROJECT_MULTIPAXOS_H
#define PROJECT_MULTIPAXOS_H

#include <string>
#include <stack>
#include <config.pb.h>
#include "Utils.h"
using namespace std;

class MultiPaxos {
private:
    int listenFd;
    int host_id = -1;
    Proto::Config::PaxosGroup groupConfig;
    int groupVersion;//�汾��
    int groupid;
    int state = 1;//0--leader, 1--follower, 2---ca
    int max_log_id;
    string storage_path;
    void *storage;
    stack<task_t *> *g_readwrite_task;

public:
    MultiPaxos() {}
    virtual ~MultiPaxos() {}
    int syncPropose(void **pPaxosData);
    int asyncPropose();
    int getValue();

    MultiPaxos(const Proto::Config::PaxosGroup &group, int host_id);

    int start();

    void mainThreadFun(int threadIndex);

    void workerCoroutine(task_t *pTask);

    void acceptCoroutine(task_t *pTask);
};

class HostInfo {
public:
    uint32_t id;
    string ip;
    uint32_t port;
    int fd;
};

class PaxosProposeHandler {
public:
    Proto::Config::PaxosGroup group;
    int groupVersion;//�汾��
    map<uint32_t , HostInfo> mapHostInfo; //hostid-->HostInfo
    int updateHostInfo();
};


#endif //PROJECT_MULTIPAXOS_H
