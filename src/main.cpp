//
// Created by hyj on 2020-04-11.
//

#include <unistd.h>
#include <signal.h>
#include <config.pb.h>
#include <pb2json.h>
#include <sys/socket.h>
#include "Core/Log.h"
#include "Core/Utils.h"
#include "Core/MultiPaxos.h"
#include "Core/Network.h"
#include "Core/Server.h"

static int g_run = 1;
void sigHander(int) {
    g_run = 0;
}
void testConfig() {
    Proto::Config::ServerConfig serverConfig;
    Proto::Config::PaxosGroup *paxosGroup = serverConfig.add_groups();
    paxosGroup->set_group_id(1);
    Proto::Config::HostId *hostId = paxosGroup->add_hostids();
    hostId->set_host_id(1);
    hostId->set_host_ip("127.0.0.1");
    hostId->set_host_port(8901);

    hostId = paxosGroup->add_hostids();
    hostId->set_host_id(2);
    hostId->set_host_ip("127.0.0.1");
    hostId->set_host_port(8902);

    paxosGroup->set_storage_dir("data1");

    serverConfig.set_server_port(9901);

    string jsonStr  = tpc::Core::Utils::Msg2JsonStr(serverConfig);
    LOG_COUT << LVAR(jsonStr) << LOG_ENDL;
}

int main(int argc, char **argv) {
    int ret;
    if (argc != 2) {
        LOG_COUT << "usage:main host_id" << LOG_ENDL;
        return -1;
    }
    int host_id = atoi(argv[1]);
    LOG_COUT << LVAR(host_id) << LOG_ENDL;
//    testConfig();
    string configStr = tpc::Core::Utils::ReadWholeFile("config.json");
    if (configStr.empty()) {
        LOG_COUT << "read config err! " << LOG_ENDL_ERR;
        return -2;
    }
    Proto::Config::ServerConfig serverConfig;
    ret = tpc::Core::Utils::JsonStr2Msg(configStr, serverConfig);
    if (ret != 0) {
        LOG_COUT << LVAR(ret) << LOG_ENDL_ERR;
        return -3;
    }

    int serverSock = tpc::Core::Network::CreateTcpSocket(serverConfig.server_port(), "*", true);
    if (serverSock < 0) {
        LOG_COUT << LVAR(serverSock) << LOG_ENDL_ERR;
        return serverSock;
    }
    ret = listen(serverSock, 10240);
    if (ret != 0) {
        LOG_COUT << "listen " << LVAR(ret) << LOG_ENDL_ERR;
        return ret;
    }
    tpc::Core::Network::SetNonBlock(serverSock);
    Server server(serverSock);

    MultiPaxos *pMultiPaxos[serverConfig.groups_size()];
    for (int i = 0; i < serverConfig.groups_size(); ++i) {
        LOG_COUT << LVAR(serverConfig.groups(i).group_id()) << LOG_ENDL;
        pMultiPaxos[i] = new MultiPaxos(serverConfig.groups(i), host_id);
        pMultiPaxos[i]->start();
        server.addMultiPaxos(serverConfig.groups(i).group_id(), pMultiPaxos[i]);
    }
    signal(SIGTERM, sigHander);
    //loop
    server.start();

    while (g_run) {
        sleep(1000*60);
    }
    LOG_COUT << LVAR(g_run) << LOG_ENDL;
}