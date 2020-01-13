#ifndef _GR_SOCKET_H__
#define _GR_SOCKET_H__

#include "include.h"

class GR_Socket
{
public:
    GR_Socket(int iFD);
    GR_Socket();
    ~GR_Socket();

    void SetFD(int iFD);

    static int CreateAndListen(const char *szIp, uint16 usPort, int iBackLog, bool bSysBA);

    static int SetBlocking(int iFD);
    static int SetNonBlocking(int iFD);
    static int SetReuseAddr(int iFD, bool bSysBA=false);
    static int SetReuserPort(int iFD);
    static int SetTcpNoDelay(int iFD);
    static int SetLinger(int iFD, int iTimeOut);
    static int SetTcpKeepAlive(int iFD);
    static int SetSndBuff(int iFD, int iSize);
    static int SetRevBuf(int iFD, int iSize);
    static int SetSndTimeO(int iFD, int iMS);
    static int SetRcvTimeO(int iFD, int iMS);
    //SYN重传次数影响connect超时时间，当重传次数为6时，超时时间为1+2+4+8+16+32+64=127秒。
    static int SetSynCnt(int iFD, int iCnt);
    // 判断连接是否处于连接状态
    static bool ConnectedCheck(int iFD);
    static int GetSoError(int iFD);
    static int GetSndBuff(int iFD);
    static int GetRcvBuff(int iFD);
    static int GetError(int iFD);

    int SetBlocking();
    int SetNonBlocking();
    int SetReuseAddr();
    int SetTcpNoDelay();
    int SetLinger(int iTimeOut);
    int SetTcpKeepAlive();
    int SetSndBuff(int iSize);
    int SetRevBuf(int iSize);
    int GetSoError();
    int GetSndBuff();
    int GetRcvBuff();
public:
    int     m_iFD = -1;

private:
    sockaddr            *addr;
    socklen_t           addrlen;
};

#endif

