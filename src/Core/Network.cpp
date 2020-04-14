//
// Created by hyj on 2019-12-10.
//

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include <fcntl.h>
#include "Network.h"
#include "Log.h"
#include "Utils.h"
#include <memory>

int tpc::Core::Network::SetNonBlock(int iSock) {
    int iFlags;

    iFlags = fcntl(iSock, F_GETFL, 0);
    iFlags |= O_NONBLOCK;
    iFlags |= O_NDELAY;
    int ret = fcntl(iSock, F_SETFL, iFlags);
    return ret;

}

int tpc::Core::Network::CreateTcpSocket(const unsigned short shPort, const char *pszIP, bool bReuse) {
    int fd = socket(AF_INET,SOCK_STREAM, IPPROTO_TCP);
    if( fd >= 0 )
    {
        if(shPort != 0)
        {
            if(bReuse)
            {
                int nReuseAddr = 1;
                setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&nReuseAddr,sizeof(nReuseAddr));
            }
            struct sockaddr_in addr ;
            SetAddr(pszIP,shPort,addr);
            int ret = bind(fd,(struct sockaddr*)&addr,sizeof(addr));
            if( ret != 0)
            {
                close(fd);
                return -1;
            }
        }
    }
    return fd;
}

void tpc::Core::Network::SetAddr(const char *pszIP, const unsigned short shPort, struct sockaddr_in &addr) {
    bzero(&addr,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(shPort);
    int nIP = 0;
    if( !pszIP || '\0' == *pszIP
        || 0 == strcmp(pszIP,"0") || 0 == strcmp(pszIP,"0.0.0.0")
        || 0 == strcmp(pszIP,"*")
            )
    {
        nIP = htonl(INADDR_ANY);
    }
    else
    {
        nIP = inet_addr(pszIP);
    }
    addr.sin_addr.s_addr = nIP;

}

string tpc::Core::Network::ReadBuff(int fd, int len) {
    char *buff = new char[len+1];
    string retStr;
    while (len > 0) {
        long n = read(fd, buff, len);
        if (n <= 0) {
//            if (errno == EAGAIN)
//                continue;
            delete[](buff);
            return retStr;
        }
        retStr += string(buff, n);
        len -= n;
    }
    delete[](buff);
    return retStr;
}

int tpc::Core::Network::SendBuff(int fd, char *buff, size_t len) {
    int pos = 0;
    while (len > 0) {
        long n = write(fd, buff+pos, len);
        if (n < 0) {
//            if (errno == EAGAIN)
//                continue;
            return n;
        }
        len -= n;
        pos += n;
    }
    return 0;
}

int tpc::Core::Network::Connect(string host, int port) {
    int fd = socket(PF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    SetAddr(host.c_str(), port, addr);
    int ret = connect(fd,(struct sockaddr*)&addr,sizeof(addr));
    if (ret != 0) {
        close(fd);
        return ret;
    }
    SetNonBlock(fd);
    return fd;
}

int tpc::Core::Network::SendMsg(int fd, Proto::Network::Msg &msg) {
    int ret;
    string outStr;
#if __MSG_JSON
    outStr = tpc::Core::Utils::Msg2JsonStr(msg);
//    LOG_COUT << "resMsg:" << outStr << LOG_ENDL;
#else
    bool result = msg.SerializeToString(&outStr);
    if (!result) {
        LOG_COUT << "SerializeToString err ret=" << ret << LOG_ENDL_ERR;
        LOG_COUT << "json=" << tpc::Core::Utils::Msg2JsonStr(msg) << LOG_ENDL;
        return ret;
    }
#endif

    tpc::Core::Msghead msghead;
    msghead.msgLen = outStr.size();
    char *buff = new char[sizeof(Msghead) + outStr.size()];
    memcpy(buff, &msghead, sizeof(Msghead));
    memcpy(buff+ sizeof(Msghead), outStr.c_str(), outStr.size());
    ret = tpc::Core::Network::SendBuff(fd, buff, sizeof(Msghead) + outStr.size());
    delete[](buff);
    if (ret != 0) {
        LOG_COUT << "SendBuff err ret=" << ret << LOG_ENDL_ERR;
        return ret;
    }
    LOG_COUT << LVAR(tpc::Core::Utils::Msg2JsonStr(msg)) << LOG_ENDL;
    return 0;

}

int tpc::Core::Network::ReadOneMsg(int fd, Proto::Network::Msg &msg) {
//    //todo:优化两次co_poll 超时时间设置
//    struct pollfd pf = { 0 };
//    pf.fd = fd;
//    pf.events = (POLLIN|POLLERR|POLLHUP);
//    co_poll( co_get_epoll_ct(),&pf, 1, 3*60*1000);

    string str = tpc::Core::Network::ReadBuff(fd, sizeof(Msghead));
    if (str.length() <= 0) {
        return -1;
    }
    Msghead *msghead = (Msghead *)str.c_str();
    int msgLen = msghead->msgLen;
    string strMsg = tpc::Core::Network::ReadBuff(fd, msgLen);
    msg.Clear();
#if __MSG_JSON
    Pb2Json::Json  json;
    json = Pb2Json::Json::parse(strMsg);
    Pb2Json::Json2Message(json, msg, true);
#else
    int ret = msg.ParseFromString(strMsg);
    if (ret < 0) {
        return ret;
    }
#endif
    LOG_COUT << LVAR(tpc::Core::Utils::Msg2JsonStr(msg)) << LOG_ENDL;
    return msg.msg_type();
}
