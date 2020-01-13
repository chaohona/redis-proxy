#include "gr_loadevent.h"
#include "redismgr.h"

GR_LoadRdbEvent::GR_LoadRdbEvent(int iFD)
{
    this->m_iFD = iFD;
}

GR_LoadRdbEvent::~GR_LoadRdbEvent()
{
}

int GR_LoadRdbEvent::GetReply(int iErr, GR_MsgIdenty *pIdenty)
{
    return GR_ERROR;
}

int GR_LoadRdbEvent::GetReply(GR_MemPoolData *pData, GR_MsgIdenty *pIdenty)
{
    
    return GR_ERROR;
}


/////////////////////////////AOF////////////////////////////////////////
GR_LoadAofEvent::GR_LoadAofEvent()
{
}

GR_LoadAofEvent::~GR_LoadAofEvent()
{
}

int GR_LoadAofEvent::Init()
{
    try
    {
        this->m_ulMsgIdenty = 0;

        this->m_ReadCache.Init(1024);

        this->m_pWaitRing   = new RingBuffer(MAX_WAIT_RING_LEN);

        // 将缓存的起始地址赋值给msg
        this->m_ReadMsg.Init((char*)this->m_ReadCache.m_pData->m_uszData);
    }
    catch(exception &e)
    {
        GR_LOGE("init accesss event got exception:%s", e.what());
        return GR_ERROR;
    }

    return GR_OK;
}

int GR_LoadAofEvent::GetReply(int iErr, GR_MsgIdenty *pIdenty)
{
    GR_MemPoolData *pData = GR_MsgProcess::Instance()->GetErrMsg(iErr);
    pIdenty->pData = nullptr;
    return this->GetReply(pData, pIdenty);
}

int GR_LoadAofEvent::GetReply(GR_MemPoolData *pData, GR_MsgIdenty *pIdentyReply)
{
    pIdentyReply->iWaitDone = 1;
    GR_MsgIdenty *pIdenty = (GR_MsgIdenty *)this->m_pWaitRing->GetFront();
    if (pIdenty == nullptr)
    {
        this->m_iProcFlag = GR_LOAD_ERROR;
        GR_LOGE("can not get identy, from wait ring, should not happened.");
        return GR_ERROR;
    }
    int iWaitNum = this->m_pWaitRing->GetNum();
    int iStart = pIdenty->ulIndex%MAX_WAIT_RING_LEN;
    uint64 ulNextIndex = 0;
    int iSendNum = 0;
    for (int i=0; i<iWaitNum; i++)
    {
        ulNextIndex = pIdenty->ulIndex+1;
        if (pIdenty->iWaitDone != 1) // 获取到响应消息了
        {
            break;
        }
        // 讲标记清除去缓冲队列
        pIdenty = (GR_MsgIdenty*)this->m_pWaitRing->PopFront();
        ASSERT(pIdenty!=nullptr);
        pIdenty->Release(ACCESS_RELEASE);
        pIdenty = (GR_MsgIdenty *)this->m_pWaitRing->GetData(ulNextIndex);
        if (pIdenty == nullptr)
        {
            if(this->m_iProcFlag == GR_LOAD_FINISH_READ)
            {
                this->m_iProcFlag = GR_LOAD_FINISH;
            }
            break;
        }
    }
    return GR_ERROR;
}

int GR_LoadAofEvent::Write()
{
    ASSERT(false);
    this->m_iProcFlag = GR_LOAD_ERROR;
    GR_LOGE("load aof file got err, errmsg:%s", strerror(errno));
    return GR_ERROR;
}

int GR_LoadAofEvent::Error()
{
    ASSERT(false);
    this->m_iProcFlag = GR_LOAD_ERROR;
    GR_LOGE("load aof file got err, errmsg:%s", strerror(errno));
    return GR_ERROR;
}

int GR_LoadAofEvent::Close()
{
    this->m_iProcFlag = GR_LOAD_ERROR;
    return GR_ERROR;
}

int GR_LoadAofEvent::Read()
{
    ASSERT(this->m_iProcFlag != GR_LOAD_FINISH_READ);
    int iRead;              // 本次读取的字节数
    int iLeft = 0;
    int iRet;
    while(1)
    {
        if (this->m_pWaitRing->PoolFull())
        {
            GR_LOGW("loading data from aof file blocking, %s", this->m_strAof.c_str());
            this->m_iPending = 1;
            break;
        }
        iLeft = m_ReadCache.LeftCapcityToEnd();
        iRead = fread(m_ReadCache.m_szMsgEnd, 1, iLeft, fp);
        if (iRead < 0)
        {
            if (errno == EINTR)
            {
                GR_LOGD("recv from client not ready - eintr, %s:%d", this->m_szAddr, this->m_uiPort);
                continue;
            }
            else if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                GR_LOGD("recv from client not ready - eagain, %s:%d", this->m_szAddr, this->m_uiPort);
                return GR_OK;
            }
            this->Close();
            return GR_ERROR;
        }
        else if (iRead == 0)
        {
            goto finish_check;
        }
        if (iRead > 0)
        {
            this->m_ReadMsg.Expand(iRead);
            this->m_ReadCache.Write(iRead);
        }
        iRet = this->ProcessMsg();
        if (iRet != GR_OK)
        {
            if (iRet == GR_FULL)
            {
                this->m_iPending = 1;
                return GR_OK;
            }
            this->Close();
            return iRet;
        }
        if (iRead < iLeft) // 数据已经读完了
        {
            goto finish_check;
        }
        // 空间不够存储当前消息，但是剩余空间大于实际空间的一半，将消息拷贝到开始
        if (this->m_ReadCache.LeftCapcity() > this->m_ReadCache.m_pData->m_sCapacity>>1)
        {
            this->m_ReadCache.ResetBuffer();
            this->m_ReadMsg.Reset(this->m_ReadCache.m_pData->m_uszData);
        }
        else
        {
            this->m_ReadCache.Expand();
            this->m_ReadMsg.Reset(this->m_ReadCache.m_pData->m_uszData);
        }
    }

    return GR_OK;

finish_check:
    if (feof(fp))
    {
        this->m_iProcFlag = GR_LOAD_FINISH_READ;
        GR_LOGI("finish read aof file:%s", this->m_strAof.c_str());
        return GR_OK;
    }
    GR_LOGE("client close the connection %s:%d", this->m_szAddr, this->m_uiPort);
    this->Close();
    return GR_ERROR;
}

int GR_LoadAofEvent::ClearPending()
{
    GR_MsgIdenty *pIdenty = (GR_MsgIdenty*)this->m_pWaitRing->GetBack();
    if (pIdenty == nullptr || pIdenty->pRedisEvent != nullptr)
    {
        GR_LOGE("ClearPending, should not happend");
        return GR_ERROR;
    }
    int iRet = GR_RedisMgr::Instance()->TransferMsgToRedis(this, pIdenty);
    if (GR_OK != iRet)
    {
        return iRet;
    }
    // 将剩余消息处理完
    iRet = this->ProcessMsg();
    if (iRet != GR_OK)
    {
        return iRet;
    }
    return GR_OK;
}

int GR_LoadAofEvent::ProcessMsg()
{
    GR_MsgIdenty *pIdenty;
    int iParseRet = 0;
    int iRet = 0;
    for (;;)
    {
        if (this->m_pWaitRing->PoolFull()) // 不能接收更多消息了
        {
            GR_LOGW("loading data from aof file blocking, %s", this->m_strAof.c_str());
            return GR_FULL;
        }
        // 解析消息
        iParseRet = this->m_ReadMsg.ParseRsp();
        if (iParseRet != GR_OK)
        {
            GR_LOGE("parse msg failed");
            return GR_ERROR;
        }
        if (this->m_ReadMsg.m_Info.nowState != GR_END)
        {
            break;
        }
        
        // 将消息放入等待列表
        pIdenty = GR_MsgIdentyPool::Instance()->Get();
        if (pIdenty == nullptr)
        {
            GR_LOGE("get msg identy failed");
            return GR_ERROR;
        }
        ASSERT(pIdenty->pData==nullptr && pIdenty->pReqData==nullptr && pIdenty->pAccessEvent==nullptr && pIdenty->pRedisEvent==nullptr);
        pIdenty->pAccessEvent = this;
        pIdenty->ulIndex = this->m_ulMsgIdenty++;
        if (!this->m_pWaitRing->AddData((char*)pIdenty, pIdenty->ulIndex))
        {
            GR_LOGE("add identy failed");
            return GR_ERROR;
        }
        pIdenty->iWaitRsp = 1;
        // TODO 各种校验
        if (this->m_ReadMsg.m_Info.iKeyLen == 0 || this->m_ReadMsg.m_Info.iLen > 1.5*1024*1024)
        {
            GR_LOGE("got a message too long");
            this->GetReply(REDIS_RSP_UNSPPORT_CMD, pIdenty);
            this->StartNext();// 没有缓存请求，直接释放请求的内存
            return GR_ERROR;
        }
        if (this->m_ReadMsg.m_Info.iCmdLen == 6 && IS_SELECT_CMD(this->m_ReadMsg.m_Info.szCmd))
        {
            if (++this->m_iSelectNum > 1)
            {
                GR_LOGE("more than one select cmd, please check...");
                return GR_ERROR;
            }
            this->GetReply(REDIS_RSP_OK, pIdenty);
            this->StartNext();// 没有缓存请求，直接释放请求的内存
            continue;
        }

        // 将消息发送给后端redis
        iRet = GR_RedisMgr::Instance()->TransferMsgToRedis(this, pIdenty);
        if (GR_OK != iRet)
        {
            // 1、如果是池子满了则返回繁忙的标记,并触发告警
            if (iRet == GR_FULL)
            {
                GR_LOGW("loading data from aof file blocking, %s", this->m_strAof.c_str());
                return iRet;
            }
            return iRet;
        }
        // 更新缓存
        // 开始解析下一条消息
        // 重新申请一块内存
        this->StartNext(); // 根据pIdenty->pReqData判断是否缓存了请求，决定是否释放请求的内存
        continue;
    }
    return GR_OK;
}

int GR_LoadAofEvent::StartNext()
{
    this->m_ReadCache.Read(this->m_ReadMsg.m_Info.iLen);
    // pIdenty->pReqData==nullptr说明请求不需要缓存，此处回收请求所占用内存
    this->m_ReadCache.ResetMemPool(true);
    this->m_ReadMsg.StartNext();
    this->m_ReadMsg.Reset(this->m_ReadCache.m_pData->m_uszData);
    return GR_OK;
}

int GR_LoadAofEvent::Loading(FILE *fp, string &strAof)
{
    this->fp = fp;
    this->m_strAof = strAof;

    int iFD = fileno(fp);
    if (iFD < 1)
    {
        GR_LOGE("fileno got error, aof file %s, errmsg %s", strAof.c_str(), strerror(errno));
        return GR_ERROR;
    }
    this->m_iFD = iFD;

    /* Check if this AOF file has an RDB preamble. In that case we need to
     * load the RDB file and later continue loading the AOF tail. */
    char sig[5]; /* "REDIS" */
    if (fread(sig,1,5,fp) != 5 || memcmp(sig,"REDIS",5) != 0) {
        /* No RDB preamble, seek back at 0 offset. */
        if (fseek(fp,0,SEEK_SET) == -1) 
        {
            GR_LOGE("fseek failed, errmsg:%s", strerror(errno));
            return GR_ERROR;
        }
    }
    else
    {
        GR_LOGE("reading rdb preamble from aof file...");
        return GR_ERROR;
    }

    this->m_iProcFlag = GR_LOAD_START;
    this->m_iSelectNum = 0;
    return GR_OK;
}

