//
// Created by hyj on 2020-04-11.
//

#ifndef PROJECT_MULTIPAXOS_H
#define PROJECT_MULTIPAXOS_H

#include <string>
#include <config.pb.h>

using namespace std;

class MultiPaxos {
private:
    int groupid;
    int state;//0--leader, 1--follower, 2---ca

    string storage_path;
    void *storage;

public:
    MultiPaxos() {}
    virtual ~MultiPaxos() {}
    int syncPropose();
    int asyncPropose();
    int getValue();

    MultiPaxos(const Proto::Config::PaxosGroup &group);
};


#endif //PROJECT_MULTIPAXOS_H
