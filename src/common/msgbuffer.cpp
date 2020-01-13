#include "msgbuffer.h"
#include "include.h"

GR_MsgBufferMgr::GR_MsgBufferMgr()
{
}

GR_MsgBufferMgr::~GR_MsgBufferMgr()
{
    if (this->m_pData != nullptr)
    {
        this->m_pData->Release();
        this->m_pData = nullptr;
    }
}

void GR_MsgBufferMgr::Init(GR_MemPoolData *pData)
{
    this->m_pData = pData;
    this->m_szMsgStart = pData->m_uszData;
    this->m_szMsgEnd = this->m_szMsgStart;
}

int GR_MsgBufferMgr::Init(int iSize)
{
    GR_MemPoolData *pData = GR_MEMPOOL_GETDATA(iSize);
    if (pData == nullptr)
    {
        GR_LOGE("get mempool failed:%d", iSize);
        return GR_ERROR;
    }

    this->Init(pData);
    return GR_OK;
}

int GR_MsgBufferMgr::ReInit()
{
    ASSERT(this->m_pData!=nullptr);
    if (GR_OK != this->m_pData->ReInit())
    {
        return GR_ERROR;
    }
    this->Init(this->m_pData);
    return GR_OK;
}

int GR_MsgBufferMgr::Expand(int iAtLeast)
{
    iAtLeast = iAtLeast>this->m_pData->m_sCapacity<<2?iAtLeast:this->m_pData->m_sCapacity<<2;
    GR_MemPoolData *pData = GR_MEMPOOL_GETDATA(iAtLeast); // 不够则申请4倍，控制内存拷贝次数
    if (pData == nullptr)
    {
        GR_LOGE("get mempool failed:%d", this->m_pData->m_sCapacity);
        return GR_ERROR;
    }
    //this->ResetMemPool(pData);
    MSGBUFFER_RESET(this , pData, true);
    return GR_OK;
}

int GR_MsgBufferMgr::ResetMemPool(int iSize, bool bRelaseData)
{
    GR_MemPoolData *pData = GR_MEMPOOL_GETDATA(iSize);
    if (pData == nullptr)
    {
        GR_LOGE("get mempool failed:%d", iSize);
        return GR_ERROR;
    }
    MSGBUFFER_RESET(this, pData, bRelaseData);
    //this->ResetMemPool(pData, bRelaseData);
    return GR_OK;
}

void GR_MsgBufferMgr::ResetMemPool(GR_MemPoolData *pData, bool bRelaseData)
{
    // buffer中有数据
    if (this->m_szMsgEnd > this->m_szMsgStart)
    {
        // 将老的数据拷入新内存中
        memcpy(pData->m_uszData, this->m_szMsgStart, this->m_szMsgEnd-this->m_szMsgStart);
    }
    this->m_szMsgEnd = pData->m_uszData + (this->m_szMsgEnd-this->m_szMsgStart);
    this->m_szMsgStart = pData->m_uszData;

    if (bRelaseData)
    {
        this->m_pData->Release();
    }

    this->m_pData = pData;
}

int GR_MsgBufferMgr::Write(int iLen)
{
    this->m_szMsgEnd += iLen;
    return GR_OK;
}

int GR_MsgBufferMgr::Read(int iLen)
{
    this->m_szMsgStart += iLen;
    return GR_OK;
}

void GR_MsgBufferMgr::ResetBuffer()
{
    memmove(this->m_pData->m_uszData, this->m_szMsgStart, this->m_szMsgEnd-this->m_szMsgStart);
    this->m_szMsgEnd = this->m_pData->m_uszData + (this->m_szMsgEnd-this->m_szMsgStart);
    this->m_szMsgStart = this->m_pData->m_uszData;
}

int GR_MsgBufferMgr::LeftCapcity()
{
    return this->m_pData->m_sCapacity - (this->m_szMsgEnd-this->m_szMsgStart);
}

int GR_MsgBufferMgr::LeftCapcityToEnd()
{
    return this->m_pData->m_uszEnd - this->m_szMsgEnd;
}

GR_RingMsgBufferMgr::GR_RingMsgBufferMgr()
{
}

GR_RingMsgBufferMgr::~GR_RingMsgBufferMgr()
{
    if (this->m_pData != nullptr)
    {
        this->m_pData->Release();
        this->m_pData = nullptr;
    }
}

void GR_RingMsgBufferMgr::Init(GR_MemPoolData *pData)
{
    ASSERT(this->m_pData == nullptr);
    this->m_pData = pData;
    this->Reuse();
}

int GR_RingMsgBufferMgr::Reuse()
{
    this->m_szMsgStart = this->m_pData->m_uszData;
    this->m_szMsgEnd = this->m_szMsgStart;
    this->m_iLeft = this->m_pData->m_sCapacity;
    this->m_ulEndIdx = 0;
    this->m_ulStartIdx = 0;
    return GR_OK;
}

int GR_RingMsgBufferMgr::Init(int iSize)
{
    GR_MemPoolData *pData = GR_MEMPOOL_GETDATA(iSize);
    if (pData == nullptr)
    {
        GR_LOGE("get mempool failed:%d", iSize);
        return GR_ERROR;
    }

    this->Init(pData);
    return GR_OK;
}

int GR_RingMsgBufferMgr::Expand(int iLen)
{
    int tmpLen = iLen + this->m_pData->m_sCapacity - this->LeftCapcity();
    if (tmpLen < this->m_pData->m_sCapacity<<1)
    {
        tmpLen = this->m_pData->m_sCapacity<<1;
    }
    GR_MemPoolData *pData = GR_MEMPOOL_GETDATA(tmpLen);
    if (pData == nullptr)
    {
        GR_LOGE("get mempool failed:%d", this->m_pData->m_sCapacity);
        return GR_ERROR;
    }
    this->ResetMemPool(pData);

    return GR_OK;
}

void GR_RingMsgBufferMgr::ResetMemPool(GR_MemPoolData *pData)
{
    ASSERT(pData->m_sCapacity >= this->m_pData->m_sCapacity);
    // buffer中有数据
    if (this->m_szMsgEnd > this->m_szMsgStart)
    {
        // 将老的数据拷入新内存中
        memcpy(pData->m_uszData, this->m_szMsgStart, this->m_szMsgEnd-this->m_szMsgStart);
        this->m_szMsgEnd = pData->m_uszData + (this->m_szMsgEnd-this->m_szMsgStart);
    } 
    else if (this->m_szMsgEnd < this->m_szMsgStart) // 出现环
    {
        memcpy(pData->m_uszData, this->m_szMsgStart, this->m_pData->m_uszEnd - this->m_szMsgStart);
        memcpy(pData->m_uszData+(this->m_pData->m_uszEnd-this->m_szMsgStart), this->m_pData->m_uszData, this->m_szMsgEnd-this->m_pData->m_uszData);
        this->m_szMsgEnd = pData->m_uszData + (this->m_pData->m_uszEnd - this->m_szMsgStart) + (this->m_szMsgEnd-this->m_pData->m_uszData);
    }
    
    ASSERT(this->m_szMsgEnd > pData->m_uszData);
    this->m_szMsgStart = pData->m_uszData;
    this->m_iLeft = pData->m_sCapacity - (this->m_szMsgEnd - this->m_szMsgStart);

    this->m_pData->Release();
    this->m_pData = pData;
}

int GR_RingMsgBufferMgr::Write(GR_MemPoolData *pData)
{
    return this->Write(pData->m_uszData, pData->m_sUsedSize);
}

int GR_RingMsgBufferMgr::Write(const char * szSrc, int iLen)
{
    int iEndLen = this->LeftCapcityToEnd();
    ASSERT(iEndLen>=0);
    ASSERT(this->LeftCapcity()>=iLen);
    if (iEndLen >= iLen)
    {
        memcpy(this->m_szMsgEnd, szSrc, iLen);
        this->m_szMsgEnd += iLen;
    }
    else if(iEndLen > 0)
    {
        memcpy(this->m_szMsgEnd, szSrc, iEndLen);
        memcpy(this->m_pData->m_uszData, szSrc+iEndLen, iLen-iEndLen);
        this->m_szMsgEnd = this->m_pData->m_uszData + (iLen-iEndLen);
        ASSERT(this->m_szMsgEnd>this->m_pData->m_uszData);
    }
    else
    {
        memcpy(this->m_pData->m_uszData, szSrc, iLen);
        this->m_szMsgEnd = this->m_pData->m_uszData + iLen;
    }
    this->m_iLeft -= iLen;
    this->m_ulEndIdx += iLen;
    this->ResetIdx();
    return GR_OK;
}

int GR_RingMsgBufferMgr::Read(int iLen)
{
    ASSERT(iLen>0);
    this->m_szMsgStart += iLen;
    this->m_iLeft += iLen;
    this->m_ulStartIdx += iLen;
    if (this->m_szMsgStart == this->m_pData->m_uszEnd)
    {
        this->m_szMsgStart = this->m_pData->m_uszData;
    }
    this->ResetIdx();
    return GR_OK;
}

int GR_RingMsgBufferMgr::LeftCapcity()
{
    return this->m_pData->m_sCapacity- m_ulEndIdx + m_ulStartIdx;
}

// 剩余空间
int GR_RingMsgBufferMgr::LeftCapcityToEnd()
{
    if (m_iLeft == 0)
    {
        return 0;
    }
    if (this->m_szMsgEnd >= this->m_szMsgStart)
    {
        return this->m_pData->m_uszEnd - this->m_szMsgEnd;
    }
    else if (this->m_szMsgEnd < this->m_szMsgStart)
    {
        return this->m_szMsgStart - this->m_szMsgEnd;
    }
    NOT_REACHED();
    return 0;
}

// 里面保存的消息
int GR_RingMsgBufferMgr::MsgLenToEnd(bool &bHasRing)
{
    if (this->m_iLeft == this->m_pData->m_sCapacity)
    {
        return 0;
    }
    // start跑到消息环的尾去了
    if (this->m_szMsgStart == this->m_szMsgEnd)
    {
        ASSERT(this->m_iLeft == 0);
        bHasRing = false;
        return this->m_pData->m_uszEnd - this->m_szMsgStart;
    }
    else if (this->m_szMsgEnd > this->m_szMsgStart)
    {
        return this->m_szMsgEnd - this->m_szMsgStart;
    }
    else
    {
        return this->m_pData->m_uszEnd - this->m_szMsgStart;
    }
    NOT_REACHED();
    return 0;
}

bool GR_RingMsgBufferMgr::Empty()
{
    return this->m_ulEndIdx == this->m_ulStartIdx;
}

void GR_RingMsgBufferMgr::ResetIdx()
{
    ASSERT(this->m_ulStartIdx <= this->m_ulEndIdx);
    if (this->m_ulEndIdx > 0xFFFFFFFF)
    {
        int iTmp = this->m_ulEndIdx - this->m_ulStartIdx;
        this->m_ulStartIdx = this->m_ulStartIdx % this->m_pData->m_sCapacity;
        this->m_ulEndIdx = this->m_ulStartIdx + iTmp;
    }
}