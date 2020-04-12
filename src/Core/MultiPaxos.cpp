//
// Created by hyj on 2020-04-11.
//
#include <stack>
#include <netinet/in.h>
#include <sys/time.h>
#include "MultiPaxos.h"
#include "Utils.h"
#include "Network.h"
#include "Log.h"
#include "Storage_mem.h"

using namespace std;

int MultiPaxos::syncPropose(void **pPaxosData, Proto::Network::CliReq *cliReq, uint64_t &outLogId) {
    PaxosProposeHandler *paxosProposeHandler;
    if (*pPaxosData == NULL) {
        *pPaxosData = new PaxosProposeHandler;
        paxosProposeHandler = reinterpret_cast<PaxosProposeHandler *>(*pPaxosData);
        paxosProposeHandler->group = groupConfig;
        paxosProposeHandler->groupVersion = groupVersion;
        paxosProposeHandler->updateHostInfo();
    }
    paxosProposeHandler = reinterpret_cast<PaxosProposeHandler *>(*pPaxosData);
    if (paxosProposeHandler->groupVersion != groupVersion) {
        paxosProposeHandler->group = groupConfig;
        paxosProposeHandler->groupVersion = groupVersion;
        paxosProposeHandler->updateHostInfo();
    }

    //get new log
    outLogId = storage->atomicAddOneMaxLogId();
    Proto::Storage::Log logData;
    int ret = storage->getLog(outLogId, logData);
    if (ret == -1) {
        logData.set_type(cliReq->request_type());
        logData.set_log_index(outLogId);
        uint64_t proposeId = newProposeId();
        logData.set_proposal_id(proposeId);
        logData.clear_log_entry();
        Proto::Storage::LogEntry *logEntry = logData.add_log_entry();
        logEntry->set_action(1);
        logEntry->set_key(cliReq->key());
        logEntry->set_value(cliReq->value());
    } else {
        LOG_COUT << "logData has write " << LVAR(outLogId) <<  LVAR(logData.proposal_id()) << LOG_ENDL;
        return -2;
    }

    // todo:Prepare msg
    //

    // AcceptReq msg
    Proto::Network::Msg reqMsg;
    reqMsg.set_msg_type(Proto::Network::MsgType::MSG_Type_Prepare_Request);
    Proto::Network::AcceptReq *acceptRequest = reqMsg.mutable_accept_request();
    acceptRequest->set_group_id(groupid);
    acceptRequest->set_log_id(outLogId);
    acceptRequest->set_proposal_id(logData.proposal_id());
//        acceptRequest->set_from_node()
    acceptRequest->set_accept_value(cliReq->key() + ":" + cliReq->value());
    
    struct pollfd fds[paxosProposeHandler->group.hostids_size()];
    for (int j = 0; j < paxosProposeHandler->group.hostids_size(); ++j) {
        fds[j].fd = -1;
    }
    //send msg
    int nSendCount = 0;
    for (int i = 0; i < paxosProposeHandler->group.hostids_size(); ++i) {
        if (paxosProposeHandler->group.hostids(i).host_id() == host_id) {
            //skip self
            continue;
        }
        auto it = paxosProposeHandler->mapHostInfo.find(paxosProposeHandler->group.hostids(i).host_id());
        if (it->second.fd <= 0) {
            it->second.fd = tpc::Core::Network::Connect(groupConfig.hostids(i).host_ip(), groupConfig.hostids(i).host_port());
            if (it->second.fd < 0) {
                LOG_COUT << "Connect to " << LVAR(groupConfig.hostids(i).host_ip()) << LVAR(groupConfig.hostids(i).host_port()) \
                    << LVAR(it->second.fd) << LOG_ENDL_ERR;
                continue;
            }
        }

        ret = tpc::Core::Network::SendMsg(it->second.fd, reqMsg);
        if (ret != 0) {
            LOG_COUT << "SendBuff" << LVAR(ret) << LOG_ENDL_ERR;
            close(it->second.fd);
            it->second.fd = -1;
            continue;
        }
        fds[i].fd = it->second.fd;
        fds[i].events = POLLIN|POLLERR|POLLHUP;
        fds[i].revents = 0;
        nSendCount += 1;
    }
    LOG_COUT << LVAR(nSendCount) << LOG_ENDL;
    int nSuccCount = 0;
    //写本地日志
    ret = storage->setLog(outLogId, logData);
    assert(ret == 0);
    nSuccCount += 1;
    // read msg
    while (1) {
        ret = poll(fds, paxosProposeHandler->group.hostids_size(), 1000);
        if (ret == 0) {
            continue;
        }
        for (int i = 0; i < paxosProposeHandler->group.hostids_size(); ++i) {
            if (fds[i].fd > 0 && fds[i].revents) {
                Proto::Network::Msg resMsg;
                ret = tpc::Core::Network::ReadOneMsg(fds[i].fd, resMsg);
                if (ret < 0) {
                    auto it = paxosProposeHandler->mapHostInfo.find(paxosProposeHandler->group.hostids(i).host_id());
                    close(it->second.fd);
                    it->second.fd = -1;
                    LOG_COUT << "read msg err " << LVAR(ret) << LVAR(it->second.ip) << LVAR(it->second.port) << LOG_ENDL_ERR;
                    continue;
                }
                Proto::Network::AcceptRes *acceptResponse = resMsg.mutable_accept_response();
                if (acceptResponse->result() != 0) {
                    LOG_COUT << "acceptResponse " << LVAR(acceptResponse->result()) << LVAR(acceptResponse->ret_proposal_id())
                        << LVAR(acceptResponse->accept_value()) << LOG_ENDL;
                } else {
                    nSuccCount += 1;
                }
            }
        }
        if (nSuccCount > (paxosProposeHandler->group.hostids_size())/2) {
            LOG_COUT << LVAR(nSuccCount) << LOG_ENDL;
            //todo:需要处理其他的返回
            break;
        }
    }

    return 0;
}

int MultiPaxos::asyncPropose() {
    return 0;
}

int MultiPaxos::getValue() {
    return 0;
}

MultiPaxos::MultiPaxos(const Proto::Config::PaxosGroup &group, int host_id) {
    groupConfig = group;
    groupid = group.group_id();
    this->host_id = host_id;
    state = 1;
}

static void *threadFun(void *args) {
    ThreadArgs *threadArgs = static_cast<ThreadArgs *>(args);
    ((MultiPaxos*)threadArgs->server)->mainThreadFun(threadArgs->threadIndex);
}

static void *threadFun1(void *args) {
    ThreadArgs *threadArgs = static_cast<ThreadArgs *>(args);
    ((MultiPaxos*)threadArgs->server)->adminWorkerThread();
}

int MultiPaxos::start() {
    //init storage
    storage = new Storage_mem(groupConfig.storage_dir());

    //test log
    Proto::Storage::Log logData;
    storage->setLog(0, logData);

    int nCpu = tpc::Core::Utils::GetCpuCount();
    int nThreadCount = nCpu*2;
    pthread_t pthreadArr[nThreadCount+1];
//    ThreadArgs threadArgs[nThreadCount];
    g_readwrite_task = new stack<task_t*>[nThreadCount];

    //server sock
    const Proto::Config::HostId *selfHostId = NULL;
    for (int j = 0; j < groupConfig.hostids_size(); ++j) {
        if (groupConfig.hostids(j).host_id() == host_id) {
            selfHostId = &groupConfig.hostids(j);
            break;
        }
    }
    if (selfHostId == NULL) {
        LOG_COUT << "not found " << LVAR(host_id) << LOG_ENDL;
        return -1;
    }

    listenFd = tpc::Core::Network::CreateTcpSocket(selfHostId->host_port(), selfHostId->host_ip().c_str(), true);
    if (listenFd < 0) {
        LOG_COUT << LVAR(listenFd) << LOG_ENDL_ERR;
        return listenFd;
    }

    int ret = listen(listenFd, 10240);
    if (ret != 0) {
        LOG_COUT << LVAR(ret) << LOG_ENDL_ERR;
        return ret;
    }
    tpc::Core::Network::SetNonBlock(listenFd);

    for (int i = 0; i < nThreadCount; ++i) {
        ThreadArgs *threadArgs = new ThreadArgs;
        threadArgs->server = this;
        threadArgs->threadIndex = i;
        int ret = pthread_create(&pthreadArr[i], NULL, threadFun, (void *)threadArgs);
        assert(ret == 0);
    }

    ThreadArgs *threadArgs = new ThreadArgs;
    threadArgs->server = this;
    ret = pthread_create(&pthreadArr[nThreadCount], NULL, threadFun1, threadArgs);
    assert(ret == 0);
}

static void *__WorkerCoroutine(void *args) {
    task_t *task = static_cast<task_t *>(args);
    ((MultiPaxos*)task->server)->workerCoroutine(task);
}

static void *__AcceptCoroutine(void *args) {
    task_t *task = static_cast<task_t *>(args);
    ((MultiPaxos*)task->server)->acceptCoroutine(task);
}

void MultiPaxos::mainThreadFun(int threadIndex) {
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

void MultiPaxos::workerCoroutine(task_t *pTask) {
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
            LOG_COUT << "start read msg" << LOG_ENDL;
            Proto::Network::Msg reqMsg;
            ret = tpc::Core::Network::ReadOneMsg(fd, reqMsg);
            if (ret <= 0) {
                LOG_COUT << "read err " << LVAR(ret) << LOG_ENDL_ERR;
                break;
            }
            LOG_COUT << "start read msg ok" << LVAR(ret) << LOG_ENDL;
            Proto::Network::Msg resMsg;
            resMsg.set_msg_type(Proto::Network::MsgType::MSG_Type_Accept_Response);
            Proto::Network::AcceptRes *acceptRes = resMsg.mutable_accept_response();

            //处理逻辑
            Proto::Network::AcceptReq *acceptReq = reqMsg.mutable_accept_request();
            if (acceptReq->group_id() != groupid) {
                acceptRes->set_result(3);
                acceptRes->set_err_msg("groupid err");
            } else {
                Proto::Storage::Log logData;
                ret = storage->getLog(acceptReq->log_id(), logData);
                if (ret == 0) {
                    if (logData.proposal_id() > acceptReq->proposal_id()) {
                        acceptRes->set_result(1);
                        acceptRes->set_err_msg("proposal_id");
                        acceptRes->set_ret_proposal_id(logData.proposal_id());
                    } else {
                        //写磁盘
                        logData.set_log_index(acceptReq->log_id());
                        logData.set_proposal_id(acceptReq->proposal_id());
                        logData.set_type(0);
                        logData.clear_log_entry();
                        Proto::Storage::LogEntry *logEntry = logData.add_log_entry();
                        logEntry->set_action(1);
                        logEntry->set_key("log");
                        logEntry->set_value(acceptReq->accept_value());
                        storage->setLog(acceptReq->log_id(), logData);
                    }
                } else {
                    //写磁盘
                    logData.set_log_index(acceptReq->log_id());
                    logData.set_proposal_id(acceptReq->proposal_id());
                    logData.set_type(0);
                    logData.clear_log_entry();
                    Proto::Storage::LogEntry *logEntry = logData.add_log_entry();
                    logEntry->set_action(1);
                    logEntry->set_key("log");
                    logEntry->set_value(acceptReq->accept_value());
                    storage->setLog(acceptReq->log_id(), logData);
                }
            }

            ret = tpc::Core::Network::SendMsg(fd, resMsg);
            LOG_COUT << "SendMsg ok" << LVAR(ret) << LOG_ENDL;
            if (ret != 0) {
                LOG_COUT << "SendMsg" << LVAR(ret) << LOG_ENDL_ERR;
                break;
            }
        }
        close(fd);
    }
}

void MultiPaxos::acceptCoroutine(task_t *pTask) {
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

void MultiPaxos::adminWorkerThread() {

}

uint64_t MultiPaxos::newProposeId() {
    //todo:需要优化...
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t ret = (tv.tv_sec*1000000 + tv.tv_usec)*100 + host_id;
    return ret;
}

int PaxosProposeHandler::updateHostInfo() {
    for (int i = 0; i < group.hostids_size(); ++i) {
        if (mapHostInfo.find(group.hostids(i).host_id()) == mapHostInfo.end()) {
            HostInfo hostInfo;
            hostInfo.fd = -1;
            hostInfo.id = group.hostids(i).host_id();
            hostInfo.ip = group.hostids(i).host_ip();
            hostInfo.port = group.hostids(i).host_port();
            mapHostInfo.insert(make_pair(group.hostids(i).host_id(), hostInfo));
        }
    }
    return 0;
}
