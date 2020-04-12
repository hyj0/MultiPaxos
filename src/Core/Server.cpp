//
// Created by hyj on 2020-04-11.
//

#include <stack>
#include <pthread.h>
#include <co_routine.h>
#include <co_routine_inner.h>
#include <netinet/in.h>
#include "Server.h"
#include "Network.h"
#include "Log.h"
using namespace std;



Server::Server(int listenFd) : listenFd(listenFd) {
}

void Server::addMultiPaxos(unsigned int group_id, MultiPaxos *pPaxos) {
    groupIdMultiPaxosMap.insert(make_pair(group_id, pPaxos));
}

static void *threadFun(void *args) {
    ThreadArgs *threadArgs = static_cast<ThreadArgs *>(args);
    ((Server*)threadArgs->server)->mainThreadFun(threadArgs->threadIndex);
}

void Server::start() {
    int nCpu = tpc::Core::Utils::GetCpuCount();
    int nThreadCount = nCpu*2;
    pthread_t pthreadArr[nThreadCount];
    ThreadArgs threadArgs[nThreadCount];
    g_readwrite_task = new stack<task_t*>[nThreadCount];

    for (int i = 0; i < nThreadCount; ++i) {
        threadArgs[i].server = this;
        threadArgs[i].threadIndex = i;
        int ret = pthread_create(&pthreadArr[i], NULL, threadFun, (void *)&threadArgs[i]);
        assert(ret == 0);
    }
    for (int j = 0; j < nThreadCount; ++j) {
        pthread_join(pthreadArr[j], NULL);
    }
}

static void *__WorkerCoroutine(void *args) {
    task_t *task = static_cast<task_t *>(args);
    ((Server*)task->server)->workerCoroutine(task);
}

static void *__AcceptCoroutine(void *args) {
    task_t *task = static_cast<task_t *>(args);
    ((Server*)task->server)->acceptCoroutine(task);
}

void Server::mainThreadFun(int threadIndex) {
    for (int i = 0; i < 100; ++i) {
        task_t * task = (task_t*)calloc( 1,sizeof(task_t) );
        task->fd = -1;
        task->threadIndex = threadIndex;
        task->server = this;
        co_create(&(task->co), NULL, __WorkerCoroutine, task);
        co_resume(task->co);
    }

    task_t *task = (task_t*)calloc( 1,sizeof(task_t) );
    task->fd = -1;
    task->threadIndex = threadIndex;
    task->server = this;
    stCoRoutine_t *ctx = NULL;
    co_create(&ctx, NULL, __AcceptCoroutine, task);
    co_resume(ctx);

    tpc::Core::Utils::bindThreadCpu(threadIndex);
    co_eventloop(co_get_epoll_ct(), NULL, NULL);
}

void Server::acceptCoroutine(task_t *pTask) {
    co_enable_hook_sys();
    int threadIndex = pTask->threadIndex;
    stack<task_t*> &g_readwrite = g_readwrite_task[threadIndex];

    for(;;)
    {
        if( g_readwrite.empty() )
        {
//            LOG_COUT << "g_readwrite.empty !! " << LOG_ENDL;
            task_t * task = (task_t*)calloc( 1,sizeof(task_t) );
            task->fd = -1;
            task->threadIndex = threadIndex;
            co_create(&(task->co), NULL, __WorkerCoroutine, task);
            co_resume(task->co);
        }
        struct sockaddr_in addr; //maybe sockaddr_un;
        memset( &addr,0,sizeof(addr) );
        socklen_t len = sizeof(addr);

        struct pollfd pf = { 0 };
        pf.fd = listenFd;
        pf.events = (POLLIN | POLLERR | POLLHUP);
        int ret = co_poll(co_get_epoll_ct(), &pf, 1, 1000*50 );
        if (ret == 0) {
            continue;
        }
        int fd = accept(listenFd, (struct sockaddr *)&addr, &len);
        if( fd < 0 )
        {
            continue;
        }
        if( g_readwrite.empty() )
        {
            close( fd );
            continue;
        }
        tpc::Core::Network::SetNonBlock( fd );
        task_t *co = g_readwrite.top();
        co->fd = fd;
        co->threadIndex = threadIndex;
        g_readwrite.pop();
        co_resume(co->co);
    }
    return ;
}

void Server::workerCoroutine(task_t *pTask) {
    int threadIndex = pTask->threadIndex;
    stack<task_t*> &g_readwrite = g_readwrite_task[threadIndex];
    co_enable_hook_sys();

    stCoCond_t *cond = co_cond_alloc();
    string retStr = "HTTP/1.1 200 OK\r\n"
                    "Content-Length: 2\r\n"
                    "Connection: Keep-Alive\r\n"
                    "Content-Type: text/html\r\n\r\nab";
    string retStr1 = "HTTP/1.1 200 OK\r\n"
                     "Content-Length: 8\r\n"
                     "Connection: Keep-Alive\r\n"
                     "Content-Type: text/html\r\n\r\n%08d";
    char buf[1024];
    void *pPaxosData = NULL;
    for(;;) {
        if (-1 == pTask->fd) {
            g_readwrite.push(pTask);
            co_yield_ct();
            continue;
        }
        int fd = pTask->fd;
        pTask->fd = -1;
        while (1) {
            struct pollfd pf = {0};
            pf.fd = fd;
            pf.events = (POLLIN | POLLERR | POLLHUP);
            int ret = co_poll(co_get_epoll_ct(), &pf, 1, 55 * 1000);
            if (ret == 0) {
                continue;
            }
            ret = read(fd, buf, sizeof(buf));
            if (ret <= 0) {
//                LOG_COUT << "read err " << LVAR(ret) << LOG_ENDL_ERR;
                break;
            }
//            LOG_COUT << LVAR(buf) << LOG_ENDL;
            int groupId = 1;
            auto it = groupIdMultiPaxosMap.find(groupId);
            if (it == groupIdMultiPaxosMap.end()) {

            } else {
                it->second->syncPropose(&pPaxosData);
            }
//            LOG_COUT << LVAR(buf) << LOG_ENDL;
            ret = write( fd, retStr.c_str(), retStr.length());
            if (ret != retStr.length()) {
                break;
            }
        }
        close(fd);
    }
}
