#include "ringbuffer.h"

RingBuffer::RingBuffer(int iPoolLen)
{
    this->Init(iPoolLen);
}

RingBuffer::RingBuffer()
{
    this->Init(1024);
}

RingBuffer::~RingBuffer()
{
    if(this->m_pWaitMsgPool != nullptr)
    {
        delete []this->m_pWaitMsgPool;
        this->m_pWaitMsgPool = nullptr;
    }
}

bool RingBuffer::PoolFull()
{
    return this->m_iDataNum == this->m_iPoolLen;
}

bool RingBuffer::AddData(char *pData)
{
    if (RINGBUFFER_POOLFULL(this))
    {
        return false;
    }

    this->m_iDataNum += 1;
    *this->m_pEnd = pData;
    this->m_pEnd += 1;
    if (this->m_pEnd == this->m_pWaitMsgEnd)
    {
        this->m_pEnd = this->m_pWaitMsgPool;
    }
    return true;
}

bool RingBuffer::AddData(char *pData, uint64 ulIndex)
{
    if (RINGBUFFER_POOLFULL(this))
    {
        return false;
    }

    int index = ulIndex%(m_iPoolLen);
    if (this->m_pEnd != this->m_pWaitMsgPool + index)
    {
        GR_LOGE("ring buffer add failed");
        return false;
    }
    *this->m_pEnd = pData;
    this->m_pEnd += 1;
    if (this->m_pEnd == this->m_pWaitMsgEnd)
    {
        this->m_pEnd = this->m_pWaitMsgPool;
    }
    this->m_iDataNum += 1;
    return true;
}

int RingBuffer::GetNum()
{
    return this->m_iDataNum;
}

char* RingBuffer::PopFront()
{
    if (this->m_iDataNum == 0)
    {
        return nullptr;
    }
    this->m_iDataNum -= 1;
    char *pRet = *m_pStart;
    char **tmp = m_pStart;
    m_pStart += 1;
    *tmp = nullptr;

    if (m_pStart >= this->m_pWaitMsgEnd)
    {
        m_pStart = this->m_pWaitMsgPool;
    }
    return pRet;
}

char *RingBuffer::PopFront(int iNum)
{
    char *pRet;
    for(int i=0; i<iNum; i++)
    {
        pRet = this->PopFront();
        if (pRet == nullptr)
        {
            return nullptr;
        }
    }

    return nullptr;
}

char *RingBuffer::GetFront()
{
    if (this->m_iDataNum == 0)
    {
        return nullptr;
    }
    return *m_pStart;
}

char *RingBuffer::GetFront(char **pNow)
{
    if (this->m_iDataNum == 0)
    {
        return nullptr;
    }
    pNow = m_pStart;
    return *m_pStart;
}

char *RingBuffer::GetBack()
{
    if (this->m_iDataNum == 0)
    {
        return nullptr;
    }
    char **pBack;
    if (m_pEnd == m_pWaitMsgPool || m_pEnd == m_pWaitMsgEnd)
    {
        pBack = m_pWaitMsgEnd-1;
        return *pBack;
    }
    
    pBack = m_pEnd-1;
    return *(pBack);
}

char *RingBuffer::GetData(uint64 ulIndex, bool &bValid)
{
    return this->m_pWaitMsgPool[ulIndex%(this->m_iPoolLen)];
}

char *RingBuffer::GetData(uint64 ulIndex)
{
    return this->m_pWaitMsgPool[ulIndex%(this->m_iPoolLen)];
}

char *RingBuffer::GetPre(char **pData, bool &bValid)
{
    if(pData-1 >= this->m_pWaitMsgPool)
    {
        return *(pData-1);
    }
    return *(this->m_pWaitMsgEnd-1);
}

char *RingBuffer::GetPre(char **pData)
{
    if(pData-1 >= this->m_pWaitMsgPool)
    {
        return *(pData-1);
    }
    return *(this->m_pWaitMsgEnd-1);
}

char *RingBuffer::GetPre(uint64 ulIndex, bool &bValid)
{
    return this->m_pWaitMsgPool[(ulIndex-1)%(this->m_iPoolLen)];
}

char *RingBuffer::GetPre(uint64 ulIndex)
{
    return this->m_pWaitMsgPool[(ulIndex-1)%(this->m_iPoolLen)];
}

char *RingBuffer::GetNext(char **pData, bool &bValid)
{
    if (pData+1 < this->m_pWaitMsgEnd)
    {
        return *(pData+1);
    }
    return *(this->m_pWaitMsgPool);
}

char *RingBuffer::GetNext(char **pData)
{
    if (pData+1 < this->m_pWaitMsgEnd)
    {
        return *(pData+1);
    }
    return *(this->m_pWaitMsgPool);
}

char *RingBuffer::GetNext(uint64 ulIndex, bool &bValid)
{
    return this->m_pWaitMsgPool[(ulIndex+1)%(this->m_iPoolLen)];
}

char *RingBuffer::GetNext(uint64 ulIndex)
{
    return this->m_pWaitMsgPool[(ulIndex+1)%(this->m_iPoolLen)];
}


bool RingBuffer::Init(int iPoolLen)
{
    this->m_pWaitMsgPool = new char*[iPoolLen];
    this->m_iPoolLen = iPoolLen;
    this->ReInit();
    return true;
}

int RingBuffer::ReInit()
{
    memset(this->m_pWaitMsgPool, 0, this->m_iPoolLen*sizeof(char*));
    this->m_pStart = this->m_pWaitMsgPool;
    this->m_pEnd = this->m_pWaitMsgPool;
    this->m_pWaitMsgEnd = this->m_pWaitMsgPool + this->m_iPoolLen;
    this->m_iDataNum = 0;
    return GR_OK;
}


