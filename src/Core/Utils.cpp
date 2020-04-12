//
// Created by hyj on 2019-12-10.
//

#include "Utils.h"
#include "Log.h"
#include <fstream>
#include <cstring>
#include <sys/time.h>
#include <sys/sysinfo.h>
#include <unistd.h>

string tpc::Core::Utils::ReadWholeFile(string filePath) {
    ifstream infile(filePath);
    if (!infile) {
        LOG_COUT << "open config file" << LOG_ENDL_ERR;
        return std::__cxx11::string();
    }
    string retStr;
    while (!infile.eof()) {
        char buff[1000];
        memset(buff, 0, sizeof(buff));
        infile.read(buff, 1000);
        retStr += buff;
    }
    return retStr;
}

string tpc::Core::Utils::Msg2JsonStr(::google::protobuf::Message &message) {
    Pb2Json::Json  json;
    Pb2Json::Message2Json(message, json, true);
    string retStr = json.dump();
    return retStr;
}

string tpc::Core::Utils::GetTS() {
    //todo:��Ҫ�Ż�...
    struct timeval tv;
    gettimeofday(&tv, NULL);
    stringstream ss;
    ss << "ts" << (tv.tv_sec*1000000*10 + tv.tv_usec);
    return ss.str();
}

int tpc::Core::Utils::GetHash(string key, int size) {
    unsigned int n = 0;
    for (int i = 0; i < key.length(); ++i) {
        n += key[i];
    }
    return n%size;
}

int tpc::Core::Utils::JsonStr2Msg(string jsonStr, ::google::protobuf::Message &message) {
    Pb2Json::Json  json;
    json = Pb2Json::Json::parse(jsonStr);
    bool ret = Pb2Json::Json2Message(json, message, true);
    if (ret != true) {
        LOG_COUT << "Json2Message err jsonStr=" << jsonStr << " json=" << json << LOG_ENDL_ERR;
        return ret;
    }
    return 0;
}

int tpc::Core::Utils::GetCpuCount() {
    int nCpuOn = sysconf(_SC_NPROCESSORS_ONLN);
    return nCpuOn;
}

int tpc::Core::Utils::bindThreadCpu(int nCpuIndex) {
    int cpu_nums = sysconf(_SC_NPROCESSORS_CONF);
    int index = nCpuIndex % cpu_nums;
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(index,&mask);
    if(-1 == pthread_setaffinity_np(pthread_self() ,sizeof(mask),&mask))
    {
        return -1;
    }
    return 0;
}
