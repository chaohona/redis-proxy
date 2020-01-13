#include "clientevent.h"

GR_ClientEvent::GR_ClientEvent(int iFD, sockaddr & sa, socklen_t &salen)
{
    this->m_iFD = iFD;
    sockaddr_in *s = (sockaddr_in *)&sa;
    inet_ntop(AF_INET,(void*)&(s->sin_addr),this->m_szIp,NET_IP_STR_LEN);
    this->m_iPort = ntohs(s->sin_port);
    this->m_iValidClient = 1;

    this->m_pReadMemData = GR_MEMPOOL_GETDATA(0x200<<8); 
    this->m_pWriteMemData = GR_MEMPOOL_GETDATA(0x200<<8);
}

GR_ClientEvent::GR_ClientEvent()
{
    this->m_iValidClient = 1;

    this->m_pReadMemData = GR_MEMPOOL_GETDATA(0x200<<8); 
    this->m_pWriteMemData = GR_MEMPOOL_GETDATA(0x200<<8);
}

GR_ClientEvent::~GR_ClientEvent()
{
    this->m_pReadMemData->Release();
    this->m_pWriteMemData->Release();
    this->m_pReadMemData = nullptr;
    this->m_pWriteMemData = nullptr;
}

int GR_ClientEvent::Write()
{
    return 0;
}

// 读取网络消息，解析
int GR_ClientEvent::Read()
{
    char szBuff[1024];
    int iRead = read(this->m_iFD, szBuff, 1024);
    return 0;
}

int GR_ClientEvent::Error()
{
    return 0;
}

int GR_ClientEvent::Close()
{
    return 0;
}

