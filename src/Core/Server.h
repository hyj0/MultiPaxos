//
// Created by hyj on 2020-04-11.
//

#ifndef PROJECT_SERVER_H
#define PROJECT_SERVER_H

#include <stack>
#include <map>
#include <cstdint>
#include "MultiPaxos.h"
#include "Utils.h"

using namespace std;

class Server;



class Server {
private:
    int listenFd;
    map<int, MultiPaxos*> groupIdMultiPaxosMap;
    stack<task_t *> *g_readwrite_task;

public:
    Server(int listenFd);

    void addMultiPaxos(unsigned int group_id, MultiPaxos *pPaxos);

    void start();

    void mainThreadFun(int threadIndex);

    void acceptCoroutine(task_t *pTask);

    void workerCoroutine(task_t *pTask);
};


#endif //PROJECT_SERVER_H
