#include "redismsg.h"
#include "gr_proxy_global.h"
#include <ctype.h>
#include <stdarg.h>

/* Return the number of digits of 'v' when converted to string in radix 10.
 * Implementation borrowed from link in redis/src/util.c:string2ll(). */
static uint32 countDigits(uint64 v) 
{
  uint32 result = 1;
  for (;;) {
    if (v < 10) return result;
    if (v < 100) return result + 1;
    if (v < 1000) return result + 2;
    if (v < 10000) return result + 3;
    v /= 10000U;
    result += 4;
  }
}

/* Helper that calculates the bulk length given a certain string length. */
#define GR_BULKLEN(len) \
    (1+countDigits(len)+2+(len)+2)

GR_RedisMsgMeta &GR_RedisMsgMeta::operator=(GR_RedisMsgMeta &other)
{
    this->iLen = other.iLen;
    this->szStart = other.szStart;
}

int GR_RedisMsgMeta::ToInt()
{
    int iRet = GR_OK;
    try
    {
        int iValue = CharToInt(this->szStart, this->iLen, iRet);
        if (iRet != GR_OK)
        {
            return iRet;
        }
        return iValue;
    }
    catch(exception &e)
    {
        iRet = GR_ERROR;
        GR_LOGE("transform redis msg to int failed, exception:%s", e.what());
        return 0;
    }
}


uint64 GR_RedisMsgMeta::ToUint64()
{
    try
    {
    }
    catch(exception &e)
    {
        GR_LOGE("transform redis msg to int failed, exception:%s", e.what());
        return 0;
    }
}

char*  GR_RedisMsgMeta::ToChar()
{
    try
    {
    }
    catch(exception &e)
    {
        GR_LOGE("transform redis msg to int failed, exception:%s", e.what());
        return 0;
    }
}

string GR_RedisMsgMeta::ToString()
{
    try
    {
        return move(string(this->szStart, this->iLen));
    }
    catch(exception &e)
    {
        GR_LOGE("transform redis msg to string failed, exception:%s", e.what());
        return 0;
    }
}


GR_RedisMsgResults::GR_RedisMsgResults(int iNum)
{
    if (iNum <= 0)
    {
        throw string("create msg result got invalid param");
    }
    this->vMsgMeta = new GR_RedisMsgMeta[iNum];
    this->iTotal = iNum;
    this->iUsed = 0;
}

GR_RedisMsgResults::~GR_RedisMsgResults()
{
    if (this->vMsgMeta == nullptr)
    {
        delete []this->vMsgMeta;
        this->vMsgMeta = nullptr;
    }
}

void GR_RedisMsgResults::Reinit()
{
    this->iUsed = 0;
    this->iCode = GR_OK;
}

GR_RedisMsgMeta* GR_RedisMsgResults::GetNext()
{
    if (this->iUsed == this->iTotal)
    {
        return nullptr;
    }
    GR_RedisMsgMeta* pRet = &this->vMsgMeta[iUsed++];
    return pRet;
}

GR_RedisMsg::GR_RedisMsg()
{
}

GR_RedisMsg::~GR_RedisMsg()
{
    try
    {
        if (this->pNextLayer != nullptr)
        {
            delete this->pNextLayer;
            this->pNextLayer = nullptr;
        }
    }
    catch(exception &e)
    {
        GR_LOGE("delete failed");
    }
}

void GR_RedisMsg::Init(char *szNewStart)
{
    memset(this, 0, sizeof(GR_RedisMsg));
    this->szEnd = szNewStart;
    this->szNowPos = szNewStart;
    this->szStart = szNewStart;
    //this->StartNext();
    memset(&this->m_Info, 0, sizeof(MsgInfo));
}

int GR_RedisMsg::ReInit(char *szNewStart)
{
    this->Init(szNewStart);
    if (this->pNextLayer != nullptr)
    {
        if (GR_OK != this->pNextLayer->ReInit(nullptr))
        {
            return GR_ERROR;
        }
    }
    return GR_OK;
}

void GR_RedisMsg::Reset(char *szNewStart)
{
    GR_RedisMsg *pTmp;
    for (pTmp=this->pNextLayer; pTmp!=nullptr; pTmp=pTmp->pNextLayer)
    {
        pTmp->szEnd = szNewStart + (pTmp->szEnd - this->szStart);
        if (pTmp->m_Info.szKeyStart != nullptr)
        {
            pTmp->m_Info.szKeyStart = szNewStart + (pTmp->m_Info.szKeyStart - this->szStart);
        }      
        pTmp->szNowPos = szNewStart + (pTmp->szNowPos - this->szStart);
        pTmp->szStart = szNewStart + (pTmp->szStart - this->szStart);
    }
    this->szEnd = szNewStart + (this->szEnd - this->szStart);
    if (this->m_Info.szKeyStart != nullptr)
    {
        this->m_Info.szKeyStart = szNewStart + (this->m_Info.szKeyStart - this->szStart);
    }
    this->szNowPos = szNewStart + (this->szNowPos - this->szStart);
    this->szStart = szNewStart;
}

void GR_RedisMsg::Expand(int iLen)
{
    this->szEnd = this->szEnd + iLen;
    GR_RedisMsg *pTmp;
    for (pTmp=this->pNextLayer; pTmp!=nullptr; pTmp=pTmp->pNextLayer)
    {
        pTmp->szEnd = pTmp->szEnd + iLen;
    }
}

int GR_RedisMsg::ParseReq()
{
    return GR_OK;
}

#define GR_MSG_KEY_START(p)\
if (this->m_Info.szKeyStart == nullptr && bKeyLine) \
{                                                   \
    this->m_Info.szKeyStart = (p);                  \
}

#define GR_MSG_CMD_INFO(p)                          \
if (this->m_Info.szCmd == nullptr)                  \
{                                                   \
    this->m_Info.szCmd = (p);                       \
    this->m_Info.iCmdLen = this->m_Info.iArgLen;    \
}

#define GR_MSG_STEP_FORWARD(num)    \
this->szNowPos += (num);            \
this->m_Info.iLen += (num);

int GR_RedisMsg::ParseRsp(GR_RedisMsgResults *pResults)
{
    char    ch;
    int     iRet;
    char    *p;
    GR_RedisMsgMeta    *pLine;
    for(p=this->szNowPos;this->szNowPos<this->szEnd;p++)
    {
        if (this->bReadingLine) // 正在读取一行
        {
            if (this->ReadLine(iRet, this->m_Info.bKeyLine))
            {
                if (pResults != nullptr)
                {
                    pLine = pResults->GetNext();
                    if (pLine == nullptr)
                    {
                        pResults->iCode = GR_ERROR;
                        GR_LOGE("result pool is poor.");
                        return GR_ERROR;
                    }
                    *pLine = this->line;
                }
                this->m_Info.iNowLine += 1;
                // 开始解析key
                if (this->m_Info.szCmd != nullptr && this->m_Info.szKeyStart == nullptr)
                {
                    if (this->m_Info.iKeyInLines == 0)
                    {
                        this->m_Info.iKeyInLines = this->KeyInLine();
                    }
                    if (this->m_Info.iKeyInLines == this->m_Info.iNowLine+1) // 下面该解析key了
                    {
                        this->m_Info.bKeyLine = true;
                    }
                }
                this->m_Info.iNeedArgNum -= 1;
                if (this->m_Info.iNeedArgNum == 0) // 解析完一条指令
                {
                    this->m_Info.nowState = GR_END;
                    return GR_OK;
                }
                this->m_Info.nowState = GR_READING_LINE; // 开始读取一行数据
                this->bReadingLine = true;
                continue; // 解析完一行了
            }
            return iRet; // 需要读完一行又没读完必定是没数据了
        }
        ch = *p;
        GR_MSG_STEP_FORWARD(1);
        switch(this->m_Info.nowState)
        {
            case GR_START:
            {
                this->m_Info.iNeedArgNum = 1;
                this->szStart = p;
                this->bReadingLine = false;
                switch (ch)
                {
                    case SYMBOL_PLUS: // +字符串\r\n
                    {
                        this->m_Info.nowState = GR_STR;
                        this->bReadingLine = true;
                        break;
                    }
                    case SYMBOL_SUB:    // -字符串\r\n
                    {
                        this->m_Info.nowState = GR_STR;
                        this->bReadingLine = true;
                        this->m_Info.iErrFlag = 1;
                        break;
                    }
                    case SYMBOL_COLON:  // :数字\r\n
                    {
                        this->m_Info.nowState = GR_STR;
                        this->bReadingLine = true;
                        break;
                    }
                    case SYMBOL_USD:    // :数字\r\n字符串\r\n      ":0\r\n\r\n"  ":-1\r\n"  
                    {
                        this->m_Info.nowState = GR_USD_NUM;
                        this->m_Info.iArgLen = 0;
                        this->bReadingLine = true;
                        break;
                    }
                    case SYMBOL_STAR: // *3\r\n
                    {
                        this->m_Info.nowState = GR_NARG; // *数字
                        this->m_Info.iNeedArgNum = 0;
                        break;
                    }
                    //case SYMBOL_PING_S: // redis-benchmark 会发送PING\r\n
                    case SYMBOL_PING_B: // redis-benchmark 会发送PING\r\n
                    {
                        this->m_Info.nowState = GR_STR;
                        this->bReadingLine = true;
                        break;
                    }
                    default:
                    {
                        GR_LOGE("invalid char start:%c", ch);
                        return GR_ERROR;
                    }
                }
                break;
            }
            case GR_NARG:
            {
                if (isdigit(ch))
                {
                    this->m_Info.iNeedArgNum = this->m_Info.iNeedArgNum*10 + int(ch-'0');
                    if (this->m_Info.iNeedArgNum == 0) // *0\r\n
                    {
                        this->m_Info.nowState = GR_NARG_ZERO;
                    }
                    break;
                }
                else if (ch == CR)
                {
                    this->m_Info.nowState = GR_NARG_CRLF;
                    break;
                }
                else if (ch == '-') // *-1\r\n
                {
                    this->m_Info.nowState = GR_NARG_NAG_ONE;
                    break;
                }
                else
                {
                    GR_LOGE("GR_NARG got invaid char:%d", ch);
                    return GR_ERROR;
                }
                break;
            }
            case GR_NARG_ZERO:
            {
                if (ch != CR)
                {
                    GR_LOGE("GR_NARG_ZERO need CR");
                    return GR_ERROR;
                }
                this->m_Info.nowState = GR_NARG_CRLF;
                break;
            }
            case GR_NARG_NAG_ONE:
            {
                if (ch != '1')
                {
                    GR_LOGE("GR_NARG_NAG_ONE need char 1");
                    return GR_ERROR;
                }
                this->m_Info.nowState = GR_NARG_ZERO; // 没有命令
                break;
            }
            case GR_NARG_CRLF:
            {
                if (ch != LF)
                {
                    GR_LOGE("GR_NARG_CRLF need LF");
                    return GR_ERROR;
                }
                if (this->m_Info.iNeedArgNum == 0) // 没有要读取的数据为 *0\r\n 或 *-1\r\n
                {
                    this->m_Info.nowState = GR_END;
                    return GR_OK;
                }
                this->m_Info.nowState = GR_READING_LINE; // 开始读取一行数据
                this->bReadingLine = true;
                break;
            }
        }
    }

    return GR_OK;
}

// 读取一行数据
bool GR_RedisMsg::ReadLine(int &iRet, bool bKeyLine)
{
    iRet = GR_OK;
    char ch;
    char *p;
    for(p=this->szNowPos;p<this->szEnd;p++)
    {
        if (this->m_Info.nowState == GR_IN_NEXT_LAYER)
        {
            iRet = this->pNextLayer->ParseRsp();
            if (iRet != GR_OK)
            {
                return false;
            }
            if (this->pNextLayer->m_Info.nowState == GR_END) // 下一层解析完成
            {
                this->bReadingLine = false;
                this->m_Info.iLen += this->pNextLayer->m_Info.iLen;
                this->line.iLen = this->pNextLayer->m_Info.iLen;
                this->szNowPos += this->pNextLayer->m_Info.iLen;
                p = this->szNowPos-1;
                return true;   
            }
            else
            {
                return false;
            }
        }
        ch = *p;
        switch(this->m_Info.nowState)
        {
           case GR_READING_LINE: // 开始读取一行数据
           {
                switch (ch)
                {
                    case SYMBOL_PLUS: // +字符串\r\n
                    {
                        this->m_Info.nowState = GR_STR;
                        GR_MSG_STEP_FORWARD(1);
                        this->line.iLen = this->m_Info.iLen;
                        GR_MSG_KEY_START(p);
                        break;
                    }
                    case SYMBOL_SUB: // -字符串
                    {
                        this->m_Info.nowState = GR_STR;
                        GR_MSG_STEP_FORWARD(1);
                        this->line.iLen = this->m_Info.iLen;
                        GR_MSG_KEY_START(p);
                        break;
                    }
                    case SYMBOL_COLON: // :字符串
                    {
                        this->m_Info.nowState = GR_STR;
                        GR_MSG_STEP_FORWARD(1);
                        this->line.iLen = this->m_Info.iLen;
                        GR_MSG_KEY_START(p);
                        break;
                    }
                    case SYMBOL_USD: // ":数字\r\n字符串\r\n"         ":0\r\n\r\n"    ":-1\r\n"  
                    {
                        this->m_Info.nowState = GR_USD_NUM;
                        this->m_Info.iArgLen = 0;
                        GR_MSG_STEP_FORWARD(1);
                        break;
                    }
                    case SYMBOL_STAR: // *开头，一行包含多个子字段
                    {
                        this->line.szStart = this->szNowPos;
                        if (this->pNextLayer == nullptr)
                        {
                            // 目前redis消息最多也就是3层，不需要调用多少次new，暂不需要内存池
                            this->pNextLayer = new GR_RedisMsg();
                            this->pNextLayer->Init(nullptr);
                        }
                        this->m_Info.nowState = GR_IN_NEXT_LAYER;
                        this->pNextLayer->StartNext();
                        this->pNextLayer->Init(this->szNowPos);         // 初始化
                        this->pNextLayer->szEnd = this->szEnd;
                        iRet = this->pNextLayer->ParseRsp(); // 第二层解析完成
                        if (iRet != GR_OK)
                        {
                            return false;
                        }
                        if (this->pNextLayer->m_Info.nowState == GR_END) // 下一层解析完成
                        {
                            this->bReadingLine = false;
                            this->m_Info.iLen += this->pNextLayer->m_Info.iLen;
                            this->line.iLen = this->pNextLayer->m_Info.iLen;
                            this->szNowPos += this->pNextLayer->m_Info.iLen;
                            p = this->szNowPos-1;
                            return true;   
                        }
                        break;
                    }
                    default:
                    {
                        iRet = GR_ERROR;
                        GR_LOGE("GR_READING_LINE got invalid char %d", ch);
                        return false;
                    }
                }
                break;
           }
           case GR_STR:
           {
                GR_MSG_STEP_FORWARD(1);
                if (ch == CR) // 回复str结束
                {
                    this->m_Info.nowState = GR_STR_CRLF;
                }
                break;
           }
           case GR_STR_CRLF: // 一行结束了
           {
               this->line.iLen = this->m_Info.iLen - this->line.iLen-1;
               this->line.szStart = this->szNowPos - this->line.iLen-1;
               GR_MSG_STEP_FORWARD(1);
               if (ch != LF)
               {
                   GR_LOGE("GR_STR_CRLF need LF");
                   iRet = GR_ERROR;
                   return false;
               }
               this->bReadingLine = false;
               if (this->m_Info.szKeyStart != nullptr && this->m_Info.iKeyLen == 0)
               {
                    this->m_Info.iKeyLen = p - 1 - this->m_Info.szKeyStart;
                    if (this->m_Info.iKeyLen > 2048)
                    {
                        GR_LOGE("key is too long:%d", this->m_Info.iKeyLen);
                        return GR_ERROR;
                    }
               }
               return true;                    
           }
           case GR_USD_NUM:
           {
                GR_MSG_STEP_FORWARD(1);
                if (isdigit(ch))
                {
                    this->m_Info.iArgLen = this->m_Info.iArgLen* 10 + int(ch-'0');
                    /*if (this->m_Info.iArgLen == 0) // +0\r\n
                    {
                        this->m_Info.nowState = GR_USD_ZERO;
                        this->line.iLen = this->m_Info.iLen;
                        break;
                    }*/
                }
                else if (ch == CR)
                {
                    /*if (this->m_Info.iArgLen == 0)
                    {
                        GR_LOGE("GR_USD_NUM got CR the len should not be 0");
                        iRet = GR_ERROR;
                        return false;
                    }*/
                    if (this->m_Info.iArgLen > this->m_Info.iMaxLen)
                    {
                        this->m_Info.iMaxLen = this->m_Info.iArgLen;
                    }
                    this->m_Info.nowState = GR_USD_NUM_CRLF;
                    break;
                }
                else if (ch == '-') // 没有内容 +-1\r\n
                {
                    this->m_Info.nowState = GR_USD_EMPTY;
                    if (this->m_Info.iArgLen != 0)
                    {
                        GR_LOGE("GR_USD_NUM got - the len should be 0");
                        iRet = GR_ERROR;
                        return false;
                    }
                }
                else
                {
                    GR_LOGE("GR_USD_NUM got invalid char:%d", ch);
                    iRet = GR_ERROR;
                    return false;
                }
                break;
           }
           case GR_USD_NUM_CRLF:
           {
                GR_MSG_STEP_FORWARD(1);
                if (ch != LF)
                {
                    GR_LOGE("GR_USD_NUM_CRLF need LF");
                    return false;
                }
                this->line.iLen = this->m_Info.iLen;
                this->m_Info.nowState = GR_USD_STR;
                int iTmpLen = this->szEnd - p;
                ASSERT(iTmpLen > 0);
                if (iTmpLen == 1 || this->m_Info.iArgLen == 0) // 没有字符串:0\r\n\r\n
                {
                    break;
                }
                ++p;
                if (iTmpLen > this->m_Info.iArgLen)
                {
                    GR_MSG_KEY_START(p);
                    GR_MSG_CMD_INFO(p);
                    GR_MSG_STEP_FORWARD(this->m_Info.iArgLen);
                    this->m_Info.iArgLen = 0;
                }
                else
                {
                    GR_MSG_KEY_START(p);
                    GR_MSG_CMD_INFO(p);
                    iTmpLen -= 1;
                    GR_MSG_STEP_FORWARD(iTmpLen);
                    this->m_Info.iArgLen -= iTmpLen;
                }
                p = this->szNowPos-1;
                break;
           }
           case GR_USD_STR:
           {
                GR_MSG_KEY_START(p);
                GR_MSG_CMD_INFO(p);
                GR_MSG_STEP_FORWARD(1);
                if (this->m_Info.iArgLen == 0)
                {
                    if (ch != CR)
                    {
                        GR_LOGE("GR_USD_STR finished need CR");
                        iRet = GR_ERROR;
                        return false;
                    }
                    this->m_Info.nowState = GR_STR_CRLF;
                    break;
                }
                this->m_Info.iArgLen -= 1;
                break;
           }
           case GR_USD_ZERO:
           {
                GR_MSG_STEP_FORWARD(1);
                if (ch != CR)
                {
                    GR_LOGE("GR_NARG_ZERO need CR");
                    iRet = GR_ERROR;
                    return false;
                }
                this->m_Info.nowState = GR_STR_CRLF;
                break;
           }
           case GR_USD_EMPTY:
           {
                GR_MSG_STEP_FORWARD(1);
                if (ch != '1')
                {
                    GR_LOGE("GR_USD_EMPTY need char 1");
                    iRet = GR_ERROR;
                    return false;
                }
                this->line.iLen = this->m_Info.iLen;
                this->m_Info.nowState = GR_USD_EMPTY_ONE;
                break;
           }
           case GR_USD_EMPTY_ONE:
           {
                GR_MSG_STEP_FORWARD(1);
                if (ch != CR)
                {
                    GR_LOGE("GR_USD_EMPTY_ONE need CR");
                    iRet = GR_ERROR;
                    return false;
                }
                this->m_Info.nowState = GR_STR_CRLF;
                break;
           }
           default:
           {
                GR_LOGE("read line invalid status:%d", this->m_Info.nowState);
                iRet = GR_ERROR;
                return false;
           }
        }
    }

    return false;
}

bool GR_RedisMsg::KeyPing()
{
    // PING
    if (this->m_Info.iCmdLen != 4)
    {
        return false;
    }
    if (!str4icmp(this->m_Info.szCmd, 'p', 'i', 'n', 'g'))
    {
        return false;
    }
    return true;
}

int GR_RedisMsg::KeyInLine()
{
    // EVAL
    if (this->m_Info.iCmdLen == 4 && str4icmp(this->m_Info.szCmd, 'e', 'v', 'a', 'l'))
    {
        return 4;
    }
    else if (this->m_Info.iCmdLen == 7 && str7icmp(this->m_Info.szCmd, 'e', 'v', 'a', 'l', 's', 'h', 'a'))// EVALSHA
    {
        return 4;
    }
    return 2; // 默认第二行
}


void GR_RedisMsg::StartNext()
{
    this->szStart += this->m_Info.iLen;
    this->szNowPos = this->szStart;
    this->bReadingLine = false;
    memset(&this->m_Info, 0, sizeof(MsgInfo));
}

GR_MsgIdenty::GR_MsgIdenty(uint8 iStaticFlag) : pAccessEvent(nullptr), pRedisEvent(nullptr), ulIndex(0),
iPreGood(0), iWaitRsp(0), iRspDone(0),iWaitDone(0),iStaticFlag(iStaticFlag)
{
}

void GR_MsgIdenty::Release(int release)
{
    ASSERT(this->pAccessEvent!=nullptr || this->pRedisEvent!=nullptr || this->iStaticFlag == 1);
    if (release == ACCESS_RELEASE)
    {
        this->pAccessEvent = nullptr;
    }
    else
    {
        this->pRedisEvent = nullptr;
    }

    if (this->pAccessEvent == nullptr && this->pRedisEvent == nullptr)
    {
        GR_MSGIDENTY_POOL()->Release(this);
    }

    return;
}

int GR_MsgIdenty::ReplyError(int iErr)
{
    if (this->pAccessEvent == nullptr)
    {
        return GR_OK;
    }
    return pAccessEvent->GetReply(iErr, this);
}

int GR_MsgIdenty::GetReply(GR_MemPoolData *pData)
{
    if (this->pAccessEvent == nullptr)
    {
        return GR_OK;
    }
    ASSERT(pData!=nullptr);
    return this->pAccessEvent->GetReply(pData, this);
}

GR_MsgIdentyPool* GR_MsgIdentyPool::m_pInstance = new GR_MsgIdentyPool();
GR_MsgIdenty *GR_MsgIdentyPool::m_pAskingIdenty = new GR_MsgIdenty(1);

GR_MsgIdenty *GR_MsgIdentyPool::GetAsking()
{
    return m_pAskingIdenty;
}

GR_MsgIdentyPool* GR_MsgIdentyPool::Instance()
{
    return m_pInstance;
}

GR_MsgIdentyPool::GR_MsgIdentyPool()
{
    this->m_pIdentyBuffer = new RingBuffer(MAX_REDIS_MSG_IDENTY_POOL);
}

GR_MsgIdentyPool::~GR_MsgIdentyPool()
{
    try
    {
        if (this->m_pIdentyBuffer == nullptr)
        {
            delete []this->m_pIdentyBuffer;
            this->m_pIdentyBuffer = nullptr;
        }
    }
    catch(exception &e)
    {
        GR_LOGE("destroy GR_MsgIdentyPool failed.");        
    }
}

GR_MsgIdenty *GR_MsgIdentyPool::Get()
{
    try
    {
        GR_MsgIdenty *pIdenty = (GR_MsgIdenty*)this->m_pIdentyBuffer->PopFront();
        if (pIdenty == nullptr)
        {
            pIdenty = new GR_MsgIdenty();
        }

        ASSERT(pIdenty->pData==nullptr && pIdenty->pReqData==nullptr);
        pIdenty->ulReqMS = CURRENT_MS();
        return pIdenty;
    }
    catch(exception &e)
    {
        GR_LOGE("create GR_MsgIdenty failed.");
        return nullptr;
    }
}

void GR_MsgIdentyPool::Release(GR_MsgIdenty * pIdenty)
{
    ASSERT(pIdenty!=nullptr);
    if (pIdenty->pData != nullptr)
    {
        GR_MEMDATA_RELEASE(pIdenty->pData);
        pIdenty->pData = nullptr;
    }
    if (pIdenty->pReqData != nullptr)
    {
        GR_MEMDATA_RELEASE(pIdenty->pReqData);
        pIdenty->pReqData = nullptr;
    }
    pIdenty->bRedirect = false;
    if (pIdenty->iStaticFlag == 1) // 不回收
    {
        return;
    }
    if (!this->m_pIdentyBuffer->AddData((char*)pIdenty))
    {
        delete pIdenty;
    }
    else 
    {
        memset(pIdenty, 0, sizeof(GR_MsgIdenty));
        ASSERT(pIdenty->pData==nullptr && pIdenty->pReqData==nullptr);
    }

    return;
}

GR_MsgProcess* GR_MsgProcess::m_pInstance = new GR_MsgProcess();

GR_MsgProcess::GR_MsgProcess()
{
}

GR_MsgProcess::~GR_MsgProcess()
{
}

GR_MsgProcess* GR_MsgProcess::Instance()
{
    return m_pInstance;
}

#define REDIS_RSP_DEFINE(ERRCODE)\
this->vErrDatas[ERRCODE] = GR_MemPool::Instance()->GetData(ERRCODE##_DESC, strlen(ERRCODE##_DESC));\
this->vErrDatas[ERRCODE]->m_cStaticFlag = 1


int GR_MsgProcess::Init()
{
    this->vErrDatas = new GR_MemPoolData*[REDIS_ERR_END];
    REDIS_RSP_DEFINE(REDIS_RSP_PING);
    REDIS_RSP_DEFINE(REDIS_RSP_PONG);
    REDIS_RSP_DEFINE(REDIS_RSP_COMM_ERR);
    REDIS_RSP_DEFINE(REDIS_RSP_DISCONNECT);
    REDIS_RSP_DEFINE(REDIS_RSP_UNSPPORT_CMD);
    REDIS_RSP_DEFINE(REDIS_RSP_SYNTAX);
    REDIS_RSP_DEFINE(REDIS_REQ_ASK);
    return GR_OK;
}

GR_MemPoolData* GR_MsgProcess::GetErrMsg(int iErr)
{
    if (iErr >= REDIS_ERR_END)
    {
        return this->vErrDatas[REDIS_RSP_COMM_ERR];
    }
    GR_MemPoolData* pData = this->vErrDatas[iErr];
    if (pData == nullptr)
    {
        return this->vErrDatas[REDIS_RSP_COMM_ERR];
    }

    return pData;
}

GR_MemPoolData* GR_MsgProcess::SentinelGetAddrByName(const char *szName, int iLen)
{
    char *subCmd = "get-master-addr-by-name";
    return this->SentinelCmd(subCmd, 23, (char*)szName, iLen);
}

GR_MemPoolData* GR_MsgProcess::SentinelGetAddrByName(const string &strName)
{
    char *subCmd = "get-master-addr-by-name";
    return this->SentinelCmd(subCmd, 23, (char*)strName.c_str(), strName.length());
}

GR_MemPoolData* GR_MsgProcess::SentinelCmd(const char *szSubCmd, const char *szArg)
{
    if (szArg != nullptr)
    {
        return this->SentinelCmd(szSubCmd, strlen(szSubCmd), szArg, strlen(szArg));
    }
    return this->SentinelCmd(szSubCmd, strlen(szSubCmd), nullptr, 0);
}

GR_MemPoolData* GR_MsgProcess::SentinelCmd(const char *szSubCmd, int iSubCmdLen, const char *szArg, int iArgLen)
{
    static char *argv[3];
    argv[0] = "sentinel";
    argv[1] = (char*)szSubCmd;
    argv[2] = (char*)szArg;
    static size_t lens[3] = { 8, 0, 0 };
    lens[1] = iSubCmdLen;
    lens[2] = iArgLen;
    if (szArg != nullptr)
    {
        return this->PackageMsg(3, (const char**)argv, lens);
    }
    return this->PackageMsg(2, (const char**)argv, lens);
}

GR_MemPoolData* GR_MsgProcess::ClusterCmd()
{
    return nullptr;
}

GR_MemPoolData* GR_MsgProcess::ClusterNodesCmd()
{
    static char *argv[2];
    argv[0] = "cluster";
    argv[1] = "nodes";
    static size_t lens[3] = { 7, 5};
    lens[1] = 5;

    return this->PackageMsg(2, (const char**)argv, lens);
}

GR_MemPoolData* GR_MsgProcess::AskingCmd()
{
    static GR_MemPoolData *g_asking_data;
    if (g_asking_data != nullptr)
    {
        return g_asking_data;
    }
    static char *argv[1];
    argv[0] = "asking";
    static size_t lens[1] = {6};

    g_asking_data = this->PackageMsg(1, (const char**)argv, lens);
    g_asking_data->m_cStaticFlag = 1;
    return g_asking_data;
}

GR_MemPoolData *ClusterSlotsOption(int iStartSlot, int iEndSlot, char *szIP, uint16 usPort)
{
    try
    {
        //:12312321\r\n
        GR_MemPoolData *pData;
        pData = GR_MEMPOOL_GETDATA(2048); // szCmd长度不会超过10位数
        if (pData == nullptr)
        {
            return nullptr;
        }
        char *szNodeId = "0e25908a9078fe6f881c8ecef1b7e3e9afacb239";
        //*3\r\n:4096\r\n:8191\r\n*3\r\n$13\r\n192.168.21.95\r\n:10002\r\n$40\r\n0e25908a9078fe6f881c8ecef1b7e3e9afacb239\r\n
        int pos = sprintf(pData->m_uszData, "*3\r\n*3\r\n:%d\r\n:%d\r\n*3\r\n$%d\r\n%s\r\n:%d\r\n%s\r\n", iStartSlot, iEndSlot, 
            strlen(szIP), szIP, usPort, szNodeId);
        int iLen = sprintf(pData->m_uszData + pos, "*3\r\n:%d\r\n:%d\r\n*3\r\n$%d\r\n%s\r\n:%d\r\n%s\r\n", iStartSlot, iEndSlot, 
            strlen(szIP), szIP, usPort, szNodeId);
        pos += iLen;
        iLen = sprintf(pData->m_uszData + pos, "*3\r\n:%d\r\n:%d\r\n*3\r\n$%d\r\n%s\r\n:%d\r\n%s\r\n", iStartSlot, iEndSlot, 
            strlen(szIP), szIP, usPort, szNodeId);
        return pData;
    }
    catch(exception &e)
    {
        GR_LOGE("package dollar got exception:%s", e.what());
        return nullptr;
    }
    
    return nullptr;
}

GR_MemPoolData* GR_MsgProcess::ReplicateCmd(int iArgNum, ...)
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
        while(1) {
            arg = va_arg(ap, char*);
            if (arg == NULL) break;

            iLen = sprintf(pArgsData->m_uszData+pos,"$%zu\r\n%s\r\n",strlen(arg),arg);
            pos += iLen;
            argslen++;
        }

        va_end(ap);

        iLen = sprintf(pData->m_uszData,"*%zu\r\n",argslen);
        memcpy(pData->m_uszData+iLen, pArgsData->m_uszData, pos);
        pos+=iLen;
        pData->Release();
        pData->m_sUsedSize = pos;

        return pArgsData;
    }
    catch(exception &e)
    {
        GR_LOGE("package replicate command got exception:%s", e.what());
        return nullptr;
    }
}

// 
GR_MemPoolData* GR_MsgProcess::CLusterSlots(char *szIp, uint16 usPort)
{
    static char *szNodeId = "0e25908a9078fe6f881c8ecef1b7e3e9afacb239";
    // 0-10383
    // 0-4095
    // 4096-8191
    // 8192-10383
    try
    {
        //:12312321\r\n
        GR_MemPoolData *pData;
        pData = GR_MEMPOOL_GETDATA(2048); // szCmd长度不会超过10位数
        if (pData == nullptr)
        {
            return nullptr;
        }
        char *szNodeId = "0e25908a9078fe6f881c8ecef1b7e3e9afacb239";
        //*3\r\n:4096\r\n:8191\r\n*3\r\n$13\r\n192.168.21.95\r\n:10002\r\n$40\r\n0e25908a9078fe6f881c8ecef1b7e3e9afacb239\r\n
        int pos = sprintf(pData->m_uszData, "*3\r\n*3\r\n:%d\r\n:%d\r\n*3\r\n$%d\r\n%s\r\n:%d\r\n$40\r\n%s\r\n", 0, 4095, 
            strlen(szIp), szIp, usPort, szNodeId);
        int iLen = sprintf(pData->m_uszData + pos, "*3\r\n:%d\r\n:%d\r\n*3\r\n$%d\r\n%s\r\n:%d\r\n$40\r\n%s\r\n", 4096, 8191, 
            strlen(szIp), szIp, usPort, szNodeId);
        pos += iLen;
        iLen = sprintf(pData->m_uszData + pos, "*3\r\n:%d\r\n:%d\r\n*3\r\n$%d\r\n%s\r\n:%d\r\n$40\r\n%s\r\n", 8192, 10383, 
            strlen(szIp), szIp, usPort, szNodeId);
        pos += iLen;
        pData->m_uszData[pos] = '\0';
        pData->m_sUsedSize = pos;
        return pData;
    }
    catch(exception &e)
    {
        GR_LOGE("package dollar got exception:%s", e.what());
        return nullptr;
    }
    
    return nullptr;
}

// 自己组装的消息不要超过1024个字符
/* Format a command according to the Redis protocol. This function takes the
 * number of arguments, an array with arguments and an array with their
 * lengths. If the latter is set to NULL, strlen will be used to compute the
 * argument lengths.
 */
GR_MemPoolData* GR_MsgProcess::PackageMsg(int argc, const char **argv, const size_t *argvlen)
{
    char *cmd = NULL; /* final command */
    int pos; /* position in final command */
    size_t len;
    int totlen, j;

    /* Calculate number of bytes needed for the command */
    totlen = 1+countDigits(argc)+2;
    for (j = 0; j < argc; j++) {
        len = argvlen ? argvlen[j] : strlen(argv[j]);
        totlen += GR_BULKLEN(len);
    }

    /* Build the command at protocol level */
    //cmd = malloc(totlen+1);
    GR_MemPoolData *pData;
    pData = GR_MEMPOOL_GETDATA(totlen+1);
    cmd = pData->m_uszData;

    pos = sprintf(cmd,"*%d\r\n",argc);
    for (j = 0; j < argc; j++) {
        len = argvlen ? argvlen[j] : strlen(argv[j]);
        pos += sprintf(cmd+pos,"$%zu\r\n",len);
        memcpy(cmd+pos,argv[j],len);
        pos += len;
        cmd[pos++] = '\r';
        cmd[pos++] = '\n';
    }
    ASSERT(pos == totlen);
    cmd[pos] = '\0';
    pData->m_sUsedSize = pos;

    return pData;
}

GR_MemPoolData* GR_MsgProcess::PackageString(char *szCmd, int iLen)
{
    GR_MemPoolData *pData;
    pData = GR_MEMPOOL_GETDATA(iLen+3+1); // szCmd长度不会超过10位数

    int pos = sprintf(pData->m_uszData,"+%s\r\n", szCmd);
    pData->m_uszData[pos] = '\0';
    pData->m_sUsedSize = pos;
    return pData;
}

GR_MemPoolData* GR_MsgProcess::PackageDollarString(char *szCmd, int iLen)
{
    try
    {
        if (iLen > 1024*1024)
        {
            return nullptr;
        }
        // :10\r\n1234567890\r\n
        GR_MemPoolData *pData;
        pData = GR_MEMPOOL_GETDATA(iLen+5+1+7); // szCmd长度不会超过10位数
        if (pData == nullptr)
        {
            return nullptr;
        }
        int pos = sprintf(pData->m_uszData, ":%d\r\n", iLen);
        memcpy(pData->m_uszData+pos, szCmd, iLen);
        pos += iLen;
        pData->m_uszData[pos++] = '\r';
        pData->m_uszData[pos++] = '\n';
        pData->m_uszData[pos] = '\0';
        return pData;
    }
    catch(exception &e)
    {
        GR_LOGE("package dollar got exception:%s", e.what());
        return nullptr;
    }
    
    return nullptr;
}

GR_MemPoolData* GR_MsgProcess::PackageInteger(uint64 ulNum)
{
    try
    {
        //:12312321\r\n
        GR_MemPoolData *pData;
        pData = GR_MEMPOOL_GETDATA(512); // szCmd长度不会超过10位数
        if (pData == nullptr)
        {
            return nullptr;
        }
        int pos = sprintf(pData->m_uszData, ":%lld\r\n", ulNum);
        pData->m_uszData[pos] = '\0';

        return pData;
    }
    catch(exception &e)
    {
        GR_LOGE("package dollar got exception:%s", e.what());
        return nullptr;
    }
    
    return nullptr;
}


