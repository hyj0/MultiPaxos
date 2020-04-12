//
// Created by hyj on 2019-12-10.
//

#ifndef PROJECT_NETWORK_H
#define PROJECT_NETWORK_H

#include <string>
#include <rpc.pb.h>

using namespace std;

namespace tpc::Core {
    struct Msghead {
        unsigned int msgLen;
    };
    class Network {
    public:
        static int SetNonBlock(int iSock);

        static void SetAddr(const char *pszIP,const unsigned short shPort,struct sockaddr_in &addr);

        static int CreateTcpSocket(const unsigned short shPort /* = 0 */,const char *pszIP /* = "*" */,bool bReuse /* = false */);

        static string ReadBuff(int fd, int len);

        static int SendBuff(int fd, char *buff, size_t len);

        static int Connect(string host, int port);

        static int SendMsg(int fd, Proto::Network::Msg &msg);

        //·µ»ØmsgType, Ê§°Ü·µ»Ø<0
        static int ReadOneMsg(int fd, Proto::Network::Msg &msg);


    };
}



#endif //PROJECT_NETWORK_H
