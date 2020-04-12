//
// Created by hyj on 2020-04-12.
//

#ifndef PROJECT_STORAGE_H
#define PROJECT_STORAGE_H

#include "Log.h"
#include <string>
#include "storage.pb.h"

using namespace std;

class Storage {
protected:
    string storage_path;
public:
    Storage(const string &storage_path) : storage_path(storage_path) {}
    virtual ~Storage() {}

    virtual int getLog(uint64_t logIndex, Proto::Storage::Log &outLog) = 0;

    virtual int setLog(uint64_t logIndex, const Proto::Storage::Log &log) = 0;

    virtual int setMaxLogId(uint64_t logId) = 0;

    virtual uint64_t getMaxLogId() = 0;
    //返回logid, 并原子加1
    virtual uint64_t atomicAddOneMaxLogId() = 0;

};


#endif //PROJECT_STORAGE_H
