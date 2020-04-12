//
// Created by hyj on 2019-12-10.
//

#ifndef PROJECT_UTILS_H
#define PROJECT_UTILS_H
#include <pb2json.h>
#include <string>
#include <unistd.h>
#include "co_routine.h"

using namespace std;

namespace tpc::Core {
    class Utils {
    public:
        static string ReadWholeFile(string filePath);
        static string Msg2JsonStr(::google::protobuf::Message &message);
        static int JsonStr2Msg(string jsonStr, ::google::protobuf::Message &message);
        static string GetTS();
        static int GetHash(string key, int size);
        static int GetCpuCount();
        static int bindThreadCpu(int nCpuIndex);

    };
}

struct task_t
{
    stCoRoutine_t *co;
    int fd;
    int threadIndex;
    void *server;
};

struct ThreadArgs {
    void *server;
    int threadIndex;
};


#endif //PROJECT_UTILS_H
