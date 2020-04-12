//
// Created by hyj on 2020-04-12.
//

#ifndef PROJECT_STORAGE_MEM_H
#define PROJECT_STORAGE_MEM_H

#include <map>
#include "Storage.h"
#include "Log.h"
#include "Utils.h"

class Storage_mem: public Storage {
private:
    map<uint64_t , Proto::Storage::Log> mapLog;
    pthread_rwlock_t mapLogLock = PTHREAD_RWLOCK_INITIALIZER;
    uint64_t max_log_id = 1;
    pthread_mutex_t max_log_id_lock = PTHREAD_MUTEX_INITIALIZER;
public:
    Storage_mem(const string &storage_path) : Storage(storage_path) {}

protected:
    int getLog(uint64_t logIndex, Proto::Storage::Log &outLog) override {
        pthread_rwlock_rdlock(&mapLogLock);
        auto it = mapLog.find(logIndex);
        if (it == mapLog.end()) {
            pthread_rwlock_unlock(&mapLogLock);
            return -1;
        } else {
            outLog = it->second;
        }
        pthread_rwlock_unlock(&mapLogLock);
        return 0;
    }

    int setLog(uint64_t logIndex, const Proto::Storage::Log &log) override {
        pthread_rwlock_wrlock(&mapLogLock);
        auto it = mapLog.find(logIndex);
        if (it == mapLog.end()) {
            mapLog.insert(make_pair(logIndex, log));
        } else {
            mapLog[logIndex] = log;
        }
        string jsonStr = tpc::Core::Utils::Msg2JsonStr((google::protobuf::Message &) log);
        LOG_COUT << "setlog " << LVAR(logIndex) << LVAR(jsonStr) << LOG_ENDL;
        pthread_rwlock_unlock(&mapLogLock);
        return 0;
    }

    int setMaxLogId(uint64_t logId) override {
        assert(0);
        return 0;
    }

    uint64_t getMaxLogId() override {
        return max_log_id;
    }

    uint64_t atomicAddOneMaxLogId() override {
        pthread_mutex_lock(&max_log_id_lock);
        uint64_t ret = max_log_id;
        max_log_id += 1;
        pthread_mutex_unlock(&max_log_id_lock);
        return ret;
    }
};


#endif //PROJECT_STORAGE_MEM_H
