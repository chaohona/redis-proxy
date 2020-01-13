#include "replicaevent.h"
#include "log.h"
#include "redismgr.h"
#include "redismsg.h"
#include "gr_keyfilter.h"
#include "gr_masterinfo_cfg.h"
#include "gr_keyfilter.h"
#include "proxy.h"
#include "config.h"

GR_ReplicaEvent::GR_ReplicaEvent(int iFD, sockaddr &sa, socklen_t &salen)
{
    this->m_iRedisType = REDIS_TYPE_REPLICA;
}

GR_ReplicaEvent::GR_ReplicaEvent(GR_RedisServer *pServer)
{
    this->m_iRedisType = REDIS_TYPE_REPLICA;
    this->m_pServer = pServer;
}

GR_ReplicaEvent::GR_ReplicaEvent()
{
    this->m_iRedisType = REDIS_TYPE_REPLICA;
}

GR_ReplicaEvent::~GR_ReplicaEvent()
{
}

int GR_ReplicaEvent::ConnectSuccess()
{
    // 发送开始同步命令
    // psync
    GR_LOGI("connect with master redis success, %s:%d", this->m_szAddr, this->m_uiPort);
    GR_ReplicaServer *pServer = (GR_ReplicaServer*)this->m_pServer;
    pServer->m_iReplState = REPL_STATE_RECEIVE_PONG;
    if (GR_OK != this->SendCommand(1, "PING"))
    {
        GR_LOGE("send ping command to master redis failed.");
        this->Close();
    }
    this->m_ReadMsg.StartNext();
    this->m_ReadMsg.Reset(this->m_ReadCache.m_pData->m_uszData); // 重置msg
    return GR_OK;
}

int GR_ReplicaEvent::SendCommand(int iArgNum, ...)
{
    try
    {
        char *arg;
        va_list ap;
        GR_MemPoolData *pData;
        pData = GR_MEMPOOL_GETDATA(2048); // szCmd长度不会超过10位数
        GR_MemPoolData *pArgsData;
        pArgsData = GR_MEMPOOL_GETDATA(2048); // szCmd长度不会超过10位数
        size_t argslen = 0;
        va_start(ap,iArgNum);

        int pos = 0;
        int iLen = 0;
        for(int i=0; i<iArgNum; i++){
            arg = va_arg(ap, char*);

            iLen = snprintf(pArgsData->m_uszData+pos, 2048-pos, "$%zu\r\n%s\r\n",strlen(arg),arg);
            pos += iLen;
            argslen++;
        }

        va_end(ap);

        iLen = sprintf(pData->m_uszData,"*%zu\r\n",argslen);
        memcpy(pData->m_uszData+iLen, pArgsData->m_uszData, pos);
        pos+=iLen;
        pArgsData->Release();
        pData->m_sUsedSize = pos;

        GR_MsgIdenty *pIdenty = GR_MsgIdentyPool::Instance()->Get();
        pIdenty->pAccessEvent = nullptr;
        if (GR_OK != this->SendMsg(pData, pIdenty))
        {
            GR_LOGE("send psync msg to master redis failed.");
            return GR_ERROR;
        }
        
        GR_Epoll::Instance()->AddEventWrite(this);
        return GR_OK;
    }
    catch(exception &e)
    {
        GR_LOGE("send command to master redis got exception:%s", e.what());
        return GR_ERROR;
    }
}

int GR_ReplicaEvent::ConnectFailed()
{
    GR_LOGI("connect with master redis failed, %s:%d", this->m_szAddr, this->m_uiPort);
    this->Close();
    return GR_OK;
}


#define REPLICA_READ_MSG()                                                                  \
this->m_ReadMsg.Expand(iRead);                                                              \
iParseRet = this->m_ReadMsg.ParseRsp();                                                     \
if (iParseRet != GR_OK)                                                                     \
{                                                                                           \
    GR_LOGE("parse redis msg failed %s:%d %d", this->m_szAddr, this->m_uiPort, iParseRet);  \
    this->Close();                                                                          \
    ASSERT(false);                                                                          \
    return GR_ERROR;                                                                        \
}                                                                                           \
if (this->m_ReadMsg.m_Info.nowState != GR_END)                                              \
{                                                                                           \
    return GR_EAGAIN;                                                                       \
}                                                                                           \
szMsgStart = this->m_ReadMsg.szStart;                                                       
// 1、如果是REPL_STATE_RECEIVE_PONG状态，则检测返回结果，状态改为REPL_STATE_SEND_AUTH
// 2、如果需要auth则走auth流程,状态改为REPL_STATE_RECEIVE_AUTH
// 3、如果健全通过，则状态改为REPL_STATE_SEND_PORT
// 4、发送本机ip:port，状态改为REPL_STATE_RECEIVE_PORT
// 5、REPL_STATE_SEND_CAPA
// 6、发送psync同步命令REPL_STATE_SEND_CAPA
// 7、根据同步命令的结果判断是否支持psync
//    +FULLRESYNC
//    +CONTINUE
//    -NOMASTERLINK
//    -LOADING
//    -ERR
// 8、状态改为REPL_STATE_CONNECTED开始接收同步文件.参考redis的readSyncBulkPayload函数
int GR_ReplicaEvent::PrepareSync(int iRead)
{
    GR_ReplicaServer *pServer = (GR_ReplicaServer*)this->m_pServer;
    int iParseRet = GR_OK;
    char *szMsgStart = nullptr;
    switch(pServer->m_iReplState)
    {
        case REPL_STATE_RECEIVE_PONG: // 发送了ping,希望得到pong
        {
            REPLICA_READ_MSG();
            if (szMsgStart[0] != '+' &&
                strncmp(szMsgStart,"-NOAUTH",7) != 0 &&
                strncmp(szMsgStart,"-ERR operation not permitted",28) != 0)
            {
                return GR_ERROR;
            }
            else
            {
                GR_LOGD("Master replied to PING, replication can continue, %s:%d", this->m_szAddr, this->m_uiPort);
            }
            pServer->m_iReplState = REPL_STATE_SEND_AUTH;
            // 发送Auth消息
            if (GR_OK != this->SendAuth())
            {
                return GR_ERROR;
            }
            this->StartNext();
            break;
        }
        case REPL_STATE_RECEIVE_AUTH:
        {
            REPLICA_READ_MSG();
            if (szMsgStart[0] == '-')
            {
                GR_LOGE("Unable to AUTH to MASTER, address %s:%d, errmsg:%d", this->m_szAddr, this->m_uiPort, szMsgStart);
                return GR_ERROR;
            }
                // 发送listening-port
            string strPort = to_string(pServer->iPort);
            if (GR_OK != this->SendCommand(3, "REPLCONF", "listening-port", strPort.c_str()))
            {
                GR_LOGE("send replconf listening-port to master redis failed %s:%d", this->m_szAddr, this->m_uiPort);
                this->Close();
                return GR_ERROR;
            }
            pServer->m_iReplState = REPL_STATE_RECEIVE_PORT;
            this->StartNext();
            break;
        }
        case REPL_STATE_RECEIVE_PORT:
        {
            REPLICA_READ_MSG();
            if (szMsgStart[0] == '-' )
            {
                GR_LOGE("(Non critical) Master does not understand REPLCONF listening-port %s:%d", this->m_szAddr, this->m_uiPort);
            }
            if (GR_OK != this->SendIP())
            {
                return GR_ERROR;
            }
            pServer->m_iReplState = REPL_STATE_RECEIVE_IP;
            this->StartNext();
            break;
        }
        case REPL_STATE_RECEIVE_IP:
        {
            REPLICA_READ_MSG();
            if (szMsgStart[0] == '-' )
            {
                GR_LOGE("(Non critical) Master does not understand REPLCONF ip-address %s:%d", this->m_szAddr, this->m_uiPort);
            }
            if (GR_OK != this->SendCommand(5, "REPLCONF", "capa","eof","capa","psync2"))
            {
                this->Close();
                GR_LOGE("send replconf to master redis failed %s:%d", this->m_szAddr, this->m_uiPort);
            }
            pServer->m_iReplState = REPL_STATE_RECEIVE_CAPA;
            this->StartNext();
            break;
        }
        case REPL_STATE_RECEIVE_CAPA:
        {
            REPLICA_READ_MSG();
            if (szMsgStart[0] == '-' )
            {
                GR_LOGE("(Non critical) Master does not understand REPLCONF capa %s:%d", this->m_szAddr, this->m_uiPort);
            }
            char psync_offset[32];
            int pos = 0;
            if (pServer->m_lReplOffSet < 0)
            {
                //pos = sprintf(psync_offset, "%lld", -1);
                pos = 2;
                psync_offset[0] = '-';
                psync_offset[1] = '1';
            }
            else
            {
                pos = sprintf(psync_offset, "%lld", pServer->m_lReplOffSet+1);
            }
            psync_offset[pos] = '\0';
            if (GR_OK != this->SendCommand(3, "PSYNC", pServer->m_szMasterId, psync_offset))
            {
                this->Close();
                GR_LOGE("send psync command to master redis failed %s:%d", this->m_szAddr, this->m_uiPort);
            }
            pServer->m_iReplState = REPL_STATE_RECEIVE_PSYNC;
            this->StartNext();
            break;
        }
        case REPL_STATE_RECEIVE_PSYNC:
        {
            int iResult;
            int iPassLen = 0;
            if (GR_OK != this->ProcPsyncResult(iResult, iPassLen))
            {
                this->Close();
                GR_LOGE("read psync result failed master redis address %s:%d", this->m_szAddr, this->m_uiPort);
            }
            switch (iResult)
            {
                case PSYNC_WAIT_REPLY:
                    return GR_OK;
                case PSYNC_TRY_LATER:
                {
                    this->Close();
                    return GR_OK;
                }
                case PSYNC_CONTINUE:
                {
                    GR_LOGI("MASTER <-> REPLICA sync: Master accepted a Partial Resynchronization %s:%d", 
                        this->m_szAddr, this->m_uiPort);
                    // 有可能连全量数据一起发送过来了，所以这个地方要接着处理全量数据
                    if (iRead - iPassLen > 0)
                    {
                        return SyncBulkPayload(iRead-iPassLen);
                    }
                    break;
                }
                case PSYNC_NOT_SUPPORTED:
                {
                    GR_LOGI("master redis not support PSYNC, retrying with SYNC, %s:%d", this->m_szAddr, this->m_uiPort);
                    this->Close();
                    return GR_OK;
                }
                case PSYNC_FULLRESYNC:
                {
                    pServer->m_iReplState = REPL_STATE_TRANSFER; // 开始接收数据
                    // 有可能连全量数据一起发送过来了，所以这个地方要接着处理全量数据
                    if (iRead - iPassLen > 0)
                    {
                        return SyncBulkPayload(iRead-iPassLen);
                    }
                    break;
                }
                default:
                {
                    GR_LOGE("replication got invalid psync result:%d", iResult);
                    return GR_ERROR;
                }
            }
            this->StartNext();
            break;
        }
        case REPL_STATE_TRANSFER: // 接收rdb文件
        {
            
        }
    }

    GR_LOGD("Replicate from master redis, replica state change to %d", pServer->m_iReplState);
    return GR_OK;
}

int GR_ReplicaEvent::SyncBulkInit(int &iRead)
{
    GR_ReplicaServer *pServer = (GR_ReplicaServer*)this->m_pServer;
    char *szReadStart = nullptr;

    int iLine = 0;
    do
    {
        iLine = 0;
        szReadStart = this->m_ReadCache.m_szMsgStart;
        // 读取一行数据
        int iLen = this->m_ReadCache.m_szMsgEnd - this->m_ReadCache.m_szMsgStart;
        if (iLen == 0)
        {
            return GR_OK;
        }
        bool bLine = false;
        char *szLine = strchr(szReadStart, '\n');
        iLine = szLine - szReadStart + 1;
        if (szLine == nullptr)
        {
            return GR_OK;
        }
        if (szReadStart[0] == '-')
        {
            GR_LOGE("MASTER aborted replication with an error: %s", szReadStart+1);
            this->Close();
            return GR_OK;
        }
        else if (szReadStart[0] == '\0' || szReadStart[0] == '\n')
        {
            /* At this stage just a newline works as a PING in order to take
             * the connection live. So we refresh our last interaction
             * timestamp. */
            pServer->m_lHeartBeatMS = GR_GetNowMS();
            iRead -= 1;
            this->m_ReadCache.Read(1);
            continue;
        }
        else if (szReadStart[0] != '$')
        {
            GR_LOGE("Bad protocol from MASTER, the first byte is not '$' (we received '%s'), are you sure the host and port are right?", szReadStart);
            this->Close();
            return GR_ERROR;
        }
        break;
    }while(1);
    if (iRead == 0)
    {
        return GR_OK;
    }

    /* There are two possible forms for the bulk payload. One is the
     * usual $<count> bulk format. The other is used for diskless transfers
     * when the master does not know beforehand the size of the file to
     * transfer. In the latter case, the following format is used:
     *
     * $EOF:<40 bytes delimiter>
     *
     * At the end of the file the announced delimiter is transmitted. The
     * delimiter is long and random enough that the probability of a
     * collision with the actual file content can be ignored. */
    if (strncmp(szReadStart+1,"EOF:",4) == 0 && strlen(szReadStart+5) >= CONFIG_RUN_ID_SIZE) {
        pServer->usemark = 1;
        memcpy(pServer->eofmark,szReadStart+5,CONFIG_RUN_ID_SIZE);
        memset(pServer->lastbytes,0,CONFIG_RUN_ID_SIZE);
        /* Set any repl_transfer_size to avoid entering this code path
         * at the next call. */
        pServer->repl_transfer_size = 0;
        GR_LOGI("MASTER <-> REPLICA sync: receiving streamed RDB from master with EOF to parser");
    } else {
        pServer->usemark = 0;
        pServer->repl_transfer_size = strtol(szReadStart+1,NULL,10);
        GR_LOGI("MASTER <-> REPLICA sync: receiving %lld bytes from master to parser",(long long)pServer->repl_transfer_size);
    }
    this->m_ReadCache.Read(iLine);
    iRead = this->m_ReadCache.m_szMsgEnd - this->m_ReadCache.m_szMsgStart;
    this->m_rdbParse.Init(&this->m_ReadCache);
    return GR_OK;
}

// 同步全量数据中
int GR_ReplicaEvent::SyncBulkPayload(int iRead)
{
    GR_ReplicaServer *pServer = (GR_ReplicaServer*)this->m_pServer;
    char *szReadStart = nullptr;
    /* If repl_transfer_size == -1 we still have to read the bulk length
     * from the master reply. */
    if (pServer->repl_transfer_size == -1)
    {
        if (GR_OK != this->SyncBulkInit(iRead))
        {
            return GR_ERROR;
        }
    }
    if (iRead == 0)
    {
        return GR_OK;
    }

    szReadStart = this->m_ReadCache.m_szMsgStart;
    pServer->stat_net_input_bytes += iRead;
    /* When a mark is used, we want to detect EOF asap in order to avoid
     * writing the EOF mark into the file... */
    int eof_reached = 0;
    if (pServer->usemark) {
        /* Update the last bytes array, and check if it matches our
         * delimiter. */
        if (iRead >= CONFIG_RUN_ID_SIZE) {
            memcpy(pServer->lastbytes,szReadStart+iRead-CONFIG_RUN_ID_SIZE,
                   CONFIG_RUN_ID_SIZE);
        } else {
            int rem = CONFIG_RUN_ID_SIZE-iRead;
            memmove(pServer->lastbytes,pServer->lastbytes+iRead,rem);
            memcpy(pServer->lastbytes+rem,szReadStart,iRead);
        }
        if (memcmp(pServer->lastbytes,pServer->eofmark,CONFIG_RUN_ID_SIZE) == 0)
            eof_reached = 1;
    }
    /* Update the last I/O time for the replication transfer (used in
     * order to detect timeouts during replication), and write what we
     * got from the socket to the dump file on disk. */
    pServer->m_lHeartBeatMS = GR_GetNowMS();
    pServer->repl_transfer_read += iRead;
    /* Delete the last 40 bytes from the file if we reached EOF. */
    if (pServer->usemark && eof_reached) { // 去掉尾部的runid，因为ReadCache不是回环的cache所以直接减就可以了
        this->m_ReadCache.m_szMsgEnd -= CONFIG_RUN_ID_SIZE;
    }
    /* Check if the transfer is now complete */
    if (!pServer->usemark) {
        if (pServer->repl_transfer_read == pServer->repl_transfer_size)
            eof_reached = 1;
    }
    // 解析全量消息
    int iRet = GR_OK;
    iRet = this->m_rdbParse.Parse();
    if (GR_OK != iRet && GR_EAGAIN != iRet)
    {
        GR_LOGE("parse rdb file failed %s:%d", this->m_szAddr, this->m_uiPort);
        return GR_ERROR;
    }
    
    if(eof_reached == 0)
    {
        return iRet;
    }
    GR_LOGI("MASTER <-> REPLICA sync: Flushing old data");
    pServer->m_iReplState = REPL_STATE_CONNECTED;
    pServer->master_repl_offset = pServer->m_lInitReplOffSet;
    // 初始化消息解析器
    this->m_ReadMsg.Init(this->m_ReadCache.m_szMsgStart);
    /* Avoid the master to detect the slave is timing out while loading the
     * RDB file in initial synchronization. We send a single newline character
     * that is valid protocol but is guaranteed to either be sent entirely or
     * not, since the byte is indivisible.
     *
     * The function is called in two contexts: while we flush the current
     * data with emptyDb(), and while we load the new data received as an
     * RDB file from the master. */
    return GR_OK;
}

int GR_ReplicaEvent::ProcPsyncResult(int &iResult, int &iPassLen)
{
    // 查找一行，如果没有读取到一行数据则接着读取
    char *szMsgStart = nullptr;
    szMsgStart = this->m_ReadCache.m_szMsgStart;
    char *szPos = strchr(szMsgStart, '\n');
    if (szPos == nullptr)
    {
        iResult = PSYNC_WAIT_REPLY;
        return GR_OK;
    }
    iPassLen = szPos-szMsgStart+1;
    /* The master may send empty newlines after it receives PSYNC
     * and before to reply, just to keep the connection alive. */
    if (iPassLen < 3)
    {
        iResult = PSYNC_WAIT_REPLY;
        this->m_ReadCache.Read(iPassLen);
        return GR_OK;
    }
    this->m_ReadCache.Read(iPassLen);
    GR_ReplicaServer *pServer = (GR_ReplicaServer*)this->m_pServer;
    if (!strncmp(szMsgStart, "+FULLRESYNC", 11))
    {
        char *replid = nullptr, *offset = nullptr;
        /* FULL RESYNC, parse the reply in order to extract the run id
         * and the replication offset. */
        replid = strchr(szMsgStart,' ');
        if (replid) {
            replid++;
            offset = strchr(replid,' ');
            if (offset) offset++;
        }
        if (!replid || !offset || (offset-replid-1) != CONFIG_RUN_ID_SIZE) {
            GR_LOGW("Master replied with wrong +FULLRESYNC syntax.");
            /* This is an unexpected condition, actually the +FULLRESYNC
             * reply means that the master supports PSYNC, but the reply
             * format seems wrong. To stay safe we blank the master
             * replid to make sure next PSYNCs will fail. */
            memset(pServer->m_szMasterId,0,CONFIG_RUN_ID_SIZE+1);
        }
        else
        {
            memcpy(pServer->m_szMasterId, replid, offset-replid-1);
            pServer->m_szMasterId[CONFIG_RUN_ID_SIZE] = '\0';
            pServer->m_lInitReplOffSet = strtoll(offset,NULL,10);
            pServer->m_lReplOffSet = pServer->m_lInitReplOffSet;
            GR_LOGI("Full resync from master: %s:%lld", pServer->m_szMasterId, pServer->m_lInitReplOffSet);
        }
        iResult = PSYNC_FULLRESYNC;
        return GR_OK;
    }
    else if (!strncmp(szMsgStart, "+CONTINUE", 9))
    {
        /* Check the new replication ID advertised by the master. If it
         * changed, we need to set the new ID as primary ID, and set or
         * secondary ID as the old master ID up to the current offset, so
         * that our sub-slaves will be able to PSYNC with us after a
         * disconnection. */
        char *start = szMsgStart+10;
        char *end = szMsgStart+9;
        while(end[0] != '\r' && end[0] != '\n' && end[0] != '\0') end++;
         if (end-start == CONFIG_RUN_ID_SIZE) {
            char newRunId[CONFIG_RUN_ID_SIZE+1];
            memcpy(newRunId,start,CONFIG_RUN_ID_SIZE);
            newRunId[CONFIG_RUN_ID_SIZE] = '\0';

            /* Update the cached master ID and our own primary ID to the
             * new one. */
            memcpy(pServer->m_szMasterId,newRunId,sizeof(pServer->m_szMasterId));
         }
        GR_LOGI("Successful partial resynchronization with master, address %s:%d", this->m_szAddr, this->m_uiPort);
        iResult = PSYNC_CONTINUE;
        return GR_OK;
    }
    else if (!strncmp(szMsgStart, "-NOMASTERLINK", 13) ||
        !strncmp(szMsgStart,"-LOADING",8))
    {
        GR_LOGE("Master is currently unable to PSYNC "
            "but should be in the future, address %s:%d, err: %s", this->m_szAddr, this->m_uiPort, szMsgStart);
        iResult = PSYNC_TRY_LATER;
        return GR_OK;
    }
    else if (strncmp(szMsgStart,"-ERR",4)) 
    {
        /* If it's not an error, log the unexpected event. */
        GR_LOGE("Unexpected reply to PSYNC from master, address %s:%d ,err: %s", this->m_szAddr, this->m_uiPort, szMsgStart);
    } 
    else 
    {
        GR_LOGI("Master does not support PSYNC or is in error state, address %s:%d ,err: %s", this->m_szAddr, this->m_uiPort, szMsgStart);
    }
    iResult = PSYNC_NOT_SUPPORTED;
    return GR_OK;
}

int GR_ReplicaEvent::Replicating(int iRead)
{
    GR_MsgIdenty *pIdenty = nullptr;
    int iParseRet = 0;
    int iRet = 0;
    bool bPass = false;
    bool bNotSupport = false;
    // 更新复制偏移量
    GR_ReplicaServer *pServer = (GR_ReplicaServer*)this->m_pServer;
    for (;;)
    {
        // 解析消息
        iParseRet = this->m_ReadMsg.ParseRsp();
        if (iParseRet != GR_OK)
        {
            GR_LOGE("parse client msg failed, fd %d, addr %s:%d", this->m_iFD, this->m_szAddr, this->m_uiPort);
            return GR_ERROR;
        }
        // 没有收到完整的消息
        if (this->m_ReadMsg.m_Info.nowState != GR_END)
        {
            break;
        }
        pServer->m_lReplOffSet += this->m_ReadMsg.m_Info.iLen;
        // 如果是ping，则回复pong
        if (m_ReadMsg.m_Info.iCmdLen == 4 && str4icmp(m_ReadMsg.m_Info.szCmd, 'p', 'i', 'n', 'g'))
        {
            this->SendCommand(1, "PING");
            char szReplOffSet[32];
            int iTmpLen = sprintf(szReplOffSet, "%llu", pServer->m_lReplOffSet);
            szReplOffSet[iTmpLen] = '\0';
            this->SendCommand(3, "REPLCONF", "ACK", szReplOffSet);
            GR_Epoll::Instance()->AddEventWrite(this);
            this->StartNext(); // 没有缓存请求，直接释放请求的内存
            continue;
        }
        // 过滤数据
        bPass = false;
        if (GR_OK != this->FilterMsg(bPass))
        {
            GR_LOGE("replicate, parse msg filter failed ", this->m_szAddr, this->m_uiPort);
            this->StartNext();
            return GR_ERROR;
        }
        if (!bPass)
        {
            this->StartNext();
            continue;
        }
        bNotSupport = false;
        if (GR_OK != this->NotSupport(bNotSupport))
        {
            GR_LOGE("replicate, parse msg not support failed ", this->m_szAddr, this->m_uiPort);
            this->StartNext();
            continue;
        }
        if (bNotSupport)
        {
            this->StartNext();
            continue;
        }
        
        // 将消息放入等待列表
        pIdenty = GR_MsgIdentyPool::Instance()->Get();
        if (pIdenty == nullptr)
        {
            GR_LOGE("get msg identy failed, fd %d, addr %s:%d", this->m_iFD, this->m_szAddr, this->m_uiPort);
            return GR_ERROR;
        }
        ASSERT(pIdenty->pData==nullptr && pIdenty->pReqData==nullptr && pIdenty->pAccessEvent==nullptr && pIdenty->pRedisEvent==nullptr);
        pIdenty->ulIndex = this->m_ulMsgIdenty++;
        pIdenty->iWaitRsp = 1;

        // 将消息发送给后端redis
        iRet = GR_RedisMgr::Instance()->ReplicateMsgToRedis(this, pIdenty);
        if (GR_OK != iRet)
        {
            // TODO 和后端redis连接有问题的处理
            // 1、如果是池子满了则返回繁忙的标记,并触发告警
            if (iRet == GR_FULL)
            {
                GR_LOGE("redis is busy %s:%d", this->m_szAddr, this->m_uiPort);
                return iRet;
            }
            // 2、如果是出错了，则返回错误
            this->StartNext();// 没有缓存请求，直接释放请求的内存
            continue;
        }
        // 更新缓存
        // 开始解析下一条消息
        // 重新申请一块内存
        this->StartNext(false); // 根据pIdenty->pReqData判断是否缓存了请求，决定是否释放请求的内存
        continue;
    }
    
    return GR_OK;
}

int GR_ReplicaEvent::FilterMsg(bool &bPass)
{
    bPass = GR_Filter::Instance()->Match(this->m_ReadMsg.m_Info.szKeyStart, this->m_ReadMsg.m_Info.iKeyLen);
    return GR_OK;
}

int GR_ReplicaEvent::NotSupport(bool &bNotSupport)
{
    bNotSupport = false;
    switch (m_ReadMsg.m_Info.iLen)
    {
        case 7:
        {
            if (str5icmp(m_ReadMsg.szStart, '+', 'P', 'O', 'N', 'G'))
            {
                bNotSupport = true;
                return GR_OK;
            }
        }
    }
    switch (m_ReadMsg.m_Info.iCmdLen)
    {
        case 5:
        {
            if (str5icmp(m_ReadMsg.szStart, '+', 'P', 'O', 'N', 'G'))
            {
                bNotSupport = true;
                return GR_OK;
            }
        }
        case 7:
        {
            if (str7icmp(m_ReadMsg.m_Info.szCmd, 'P', 'U', 'B', 'L', 'I', 'S', 'H'))
            {
                bNotSupport = true;
                return GR_OK;
            }
        }
    }

    return GR_OK;
}

int GR_ReplicaEvent::StartNext(bool bRealseReq)
{
    this->m_ReadCache.Read(this->m_ReadMsg.m_Info.iLen);
    // pIdenty->pReqData==nullptr说明请求不需要缓存，此处回收请求所占用内存
    this->m_ReadCache.ResetMemPool(this->m_ReadCache.m_pData->m_sCapacity, bRealseReq);
    this->m_ReadMsg.StartNext();
    this->m_ReadMsg.Reset(this->m_ReadCache.m_pData->m_uszData);
    return GR_OK;
}


int GR_ReplicaEvent::SendAuth()
{
    GR_ReplicaServer *pServer = (GR_ReplicaServer*)this->m_pServer;
    if (pServer->m_strMasterUser != "" && pServer->m_strMasterAuth != "")
    {
        if (GR_OK != this->SendCommand(3, "AUTH", pServer->m_strMasterUser.c_str(), pServer->m_strMasterAuth.c_str()))
        {
            this->Close();
            GR_LOGE("send auth to master redis failed %s:%d", this->m_szAddr, this->m_uiPort);
            return GR_ERROR;
        }
    }
    else if (pServer->m_strMasterAuth != "")
    {
        if (GR_OK != this->SendCommand(2, "AUTH", pServer->m_strMasterAuth.c_str()))
        {
            this->Close();
            GR_LOGE("send auth to master redis failed %s:%d", this->m_szAddr, this->m_uiPort);
            return GR_ERROR;
        }
    }
    else
    {
        GR_Config *pConfig = &GR_Proxy::Instance()->m_Config;
        // 发送listening-port
        string strPort = to_string(pConfig->m_usPort);
        if (GR_OK != this->SendCommand(3, "REPLCONF", "listening-port", strPort.c_str()))
        {
            GR_LOGE("send replconf listening-port to master redis failed %s:%d", this->m_szAddr, this->m_uiPort);
            this->Close();
            return GR_ERROR;
        }
        pServer->m_iReplState = REPL_STATE_RECEIVE_PORT;
    }
    return GR_OK;
}

int GR_ReplicaEvent::SendIP()
{
    // 发送listening-port
    GR_ReplicaServer *pServer = (GR_ReplicaServer*)this->m_pServer;
    GR_Config *pConfig = &GR_Proxy::Instance()->m_Config;
    if (GR_OK != this->SendCommand(3, "REPLCONF", "ip-address", pConfig->m_strIP.c_str()))
    {
        GR_LOGE("send replconf listening-port to master redis failed %s:%d", this->m_szAddr, this->m_uiPort);
        this->Close();
        return GR_ERROR;
    }
    pServer->m_iReplState = REPL_STATE_RECEIVE_IP;
    return GR_OK;
}

int GR_ReplicaEvent::Read()
{
    if (this->m_iFD <= 0)
    {
        GR_LOGD("try to read from nagtive fd %s:%d", this->m_szAddr, this->m_uiPort);
        return GR_ERROR;
    }
    // 将消息读到缓存中，并解析消息，如果解析完一条则开始转发这条消息
    // 如果内存不够解析一条消息的则移动数据
    // 如果消息已经占内存的一半则申请一个更大的缓存，否则在此缓存中直接移动
    int iMaxMsgRead = 1000; // 每次最多接收1000条消息则让出cpu
    int iRead;              // 本次读取的字节数
    int iLeft = 0;
    int iParseRet = 0;
    bool bReleaseData = true;
    GR_MsgIdenty *pIdenty;
    int iProcNum;
    int iRet;
    GR_ReplicaServer *pServer = (GR_ReplicaServer*)this->m_pServer;

    do{
        iLeft = m_ReadCache.LeftCapcityToEnd();
        iRead = read(this->m_iFD, m_ReadCache.m_szMsgEnd, iLeft);
        if (iRead < 0)
        {
            if (errno == EINTR)
            {
                GR_LOGD("recv from redis not ready - eintr, %s:%d", this->m_szAddr, this->m_uiPort);
                continue;
            }
            else if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                GR_LOGD("recv from redis not ready - eagain, %s:%d", this->m_szAddr, this->m_uiPort);
                return GR_EAGAIN;
            }
            GR_LOGE("redis read failed:%d, errmsg:%s", errno, strerror(errno));
            this->Close(false);
            return GR_ERROR;
        }
        else if (iRead == 0)
        {
            GR_LOGE("redis close the connection, %d %s:%d", this->m_iFD, this->m_szAddr, this->m_uiPort);
            this->Close();
            return GR_ERROR;
        }
        this->m_ReadCache.Write(iRead);
        this->m_ReadMsg.Expand(iRead);
        
        if (pServer->m_iReplState == REPL_STATE_CONNECTED)      // 全量数据同步完成，开始增量同步数据
        {
            GR_LOGD("replicate from master %s", this->m_ReadCache.m_szMsgStart);
            iRet = Replicating(iRead);
        }
        else if (pServer->m_iReplState == REPL_STATE_TRANSFER)  // 同步全量数据
        {
            iRet = SyncBulkPayload(iRead);
        }
        else    // 主从同步准备阶段
        {
            iRet = PrepareSync(iRead);
        }
        if (GR_OK != iRet && GR_EAGAIN != iRet)
        {
            GR_LOGE("process master message failed, %s:%d", this->m_szAddr, this->m_uiPort);
            this->Close();
            return GR_ERROR;
        }
        if (iRead < iLeft) // 数据已经读完了
        {
            break;
        }
        // 剩余空间小于一半则扩容
        if (this->m_ReadCache.LeftCapcity() < this->m_ReadCache.m_pData->m_sCapacity>>1)
        {
            // 空间不够存储当前消息，扩展消息的缓存
            this->m_ReadCache.Expand();
            this->m_ReadMsg.Reset(this->m_ReadCache.m_pData->m_uszData);
        }
        // 否则将剩余数据拷贝到内存池开头
        else
        {
            if (iRet != GR_EAGAIN)
            {
                this->m_ReadCache.ResetBuffer();
                this->m_ReadMsg.Reset(this->m_ReadCache.m_pData->m_uszData);
            }
        }
    }while(1);
    
    return 0;
}

int GR_ReplicaEvent::Error()
{
    return GR_OK;
}

int GR_ReplicaEvent::Close(bool bForceClose)
{
    GR_Event::Close();
    // TODO 断线重连
    return GR_OK;
}

GR_ReplicaServer::GR_ReplicaServer()
{
    this->m_szMasterId[0] = '?';
    this->m_szMasterId[1] = '\0';
}

GR_ReplicaServer::~GR_ReplicaServer()
{
}

int GR_ReplicaServer::Connect()
{
    int iRet = GR_OK;
    try
    {
        if (this->pEvent != nullptr)
        {
            this->pEvent->Close(true);
            delete this->pEvent; // TODO 连接池子
        }
        this->pEvent = new GR_ReplicaEvent(this);
        iRet = this->pEvent->ConnectToRedis(this->iPort, this->strHostname.c_str());
        if (iRet != GR_OK)
        {
            GR_LOGE("connect to redis failed, %s:%d", this->strHostname.c_str(), this->iPort);
            return GR_ERROR;
        }
        GR_LOGI("connecting to master redis %s:%d", this->strHostname.c_str(), this->iPort);
    }
    catch(exception &e)
    {
        GR_LOGE("connect to redis got exception:%s", e.what());
        return GR_ERROR;
    }
    
    return GR_OK;
}

GR_ReplicaMgr* GR_ReplicaMgr::m_pInstance = new GR_ReplicaMgr();

GR_ReplicaMgr::GR_ReplicaMgr()
{
}

GR_ReplicaMgr::~GR_ReplicaMgr()
{
    try
    {
        if (this->m_vMasterRedis != nullptr)
        {
            delete []this->m_vMasterRedis;
            this->m_vMasterRedis = nullptr;
        }
    }
    catch(exception &e)
    {
    }
}

GR_ReplicaMgr *GR_ReplicaMgr::Instance()
{
    return m_pInstance;
}

int GR_ReplicaMgr::AddRedis(GR_MasterAuth redisInfo, string &strReplInfo)
{
    string  &strIp = redisInfo.strIP;
    uint16  usPort = redisInfo.usPort;
    if (m_iRedisNum >= GR_MAX_REPLICA_REDIS)
    {
        GR_LOGE("there was too much redis to replica, should less than:%d", GR_MAX_REPLICA_REDIS);
        return GR_ERROR;
    }
    ASSERT(m_vMasterRedis!=nullptr);
    GR_ReplicaServer *pServer = m_vMasterRedis+m_iRedisNum;
    pServer->strHostname = strIp;
    pServer->iPort = usPort;
    pServer->m_strMasterUser = redisInfo.strUser;
    pServer->m_strMasterAuth = redisInfo.strAuth;
    this->m_iRedisNum += 1;
    // 解析master redis信息
    if (strReplInfo != "")
    {
        auto results = split(strReplInfo, " ");
        if (results.size() != 3)
        {
            GR_LOGE("master info invalid:%s", strReplInfo.c_str());
            return GR_ERROR;
        }
        if (results[1].length() != 40)
        {
            GR_LOGE("master info(master id) invalid:%s", strReplInfo.c_str());
            return GR_ERROR;
        }
        pServer->m_lReplOffSet = atoll(results[2].c_str());
        memcpy(pServer->m_szMasterId, results[1].c_str(), CONFIG_RUN_ID_SIZE);
    }
    
    return GR_OK;
}

int GR_ReplicaMgr::Init(GR_Config *pConfig)
{
    try
    {
        // 1.初始化过滤器
        if(GR_OK != GR_Filter::Instance()->Init(pConfig) )
        {
            GR_LOGE("init key filter failed.");
            return GR_OK;
        }
        // 2.解析主redis信息
        // 2.1解析主从复制持久化信息
        GR_MasterInfoCfg masterCfg;
        if (GR_OK != masterCfg.Init(pConfig->m_ReplicateInfo.m_strCfgFile.c_str()))
        {
            return GR_ERROR;
        }
        // 2.2组织主连接信息
        this->m_iRedisNum = pConfig->m_ReplicateInfo.m_vMasters.size();
        if (this->m_iRedisNum == 0)
        {
            GR_LOGE("there are not master redis to replicate.");
            return GR_ERROR;
        }
        this->m_vMasterRedis = new GR_ReplicaServer[GR_MAX_REPLICA_REDIS];
        auto itr = pConfig->m_ReplicateInfo.m_vMasters.begin();
        for(; itr!=pConfig->m_ReplicateInfo.m_vMasters.end(); ++itr)
        {
            string strReplInfo = masterCfg.m_mapMasters[(*itr).strAddr];
            if (GR_OK != this->AddRedis(*itr, strReplInfo))
            {
                return GR_ERROR;
            }
        }
        // 3.和所有主建立连接
        if (GR_OK != this->ConnectToRedis())
        {
            return GR_ERROR;
        }
        // 4.在Event中
    }
    catch(exception &e)
    {
        GR_LOGE("init replica got exception:%s", e.what());
        return GR_ERROR;
    }
    return GR_OK;
}

int GR_ReplicaMgr::ConnectToRedis()
{
    ASSERT(this->m_iRedisNum>0 && this->m_vMasterRedis!=nullptr);
    GR_ReplicaServer *pServer = nullptr;
    for(int i=0; i<this->m_iRedisNum; i++)
    {
        pServer = this->m_vMasterRedis+i;
        if (GR_OK != pServer->Connect())
        {
            return GR_ERROR;
        }
    }
    return GR_OK;
}

