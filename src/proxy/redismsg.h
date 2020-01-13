#ifndef _GR_REDIS_MSG_H__
#define _GR_REDIS_MSG_H__

#include "event.h"
#include "ringbuffer.h"
#include "mempool.h"
#include "utils.h"

#define GR_REDIS_PROTO_PRE_LEN 128

class GR_Event;

#define LF                  (uint8_t) 10
#define CR                  (uint8_t) 13
#define CRLF                "\x0d\x0a"
#define CRLF_LEN            (sizeof("\x0d\x0a") - 1)

#define SYMBOL_STAR     '*'
#define SYMBOL_USD      '$'
#define SYMBOL_PLUS     '+'
#define SYMBOL_SUB      '-'
#define SYMBOL_COLON    ':'
#define SYMBOL_PING_S     'p' //PING\r\n
#define SYMBOL_PING_B     'P' //PING\r\n


// lua脚本，EVAL格式SCRIPT
//*7\r\n$4\r\nEVAL\r\n$40\r\nreturn {KEYS[1],KEYS[2],ARGV[1],ARGV[2]}\r\n$1\r\n2\r\n
//$4\r\nkey1\r\n$4\r\nkey2\r\n$5\r\nfirst\r\n$6\r\nsecond\r\n


//*<*后面的数字> CR LF
//$<参数 1 的字节数量> CR LF
//<参数 1 的数据> CR LF
//...
//$<参数 N 的字节数量> CR LF
//<参数 N 的数据> CR LF

//多个串的格式
/*
<开始的星号><*后面的数字> <*后面的数字后面的CRLF>
<key的长度前面的$><key的长度><key长度后面的CRLF>
<key><key后面的CRLF>
<参数长度前面的$><参数参数的长度><参数长度后面的CRLF>
<参数><参数后面的CRLF>
*/

// 单个串的格式
/*
1、+字符串\r\n
2、-字符串\r\n
3、:数字\r\n
4、$数字\r\n字符串\r\n
5、*多条批量回复
*/
enum
{
    GR_START,

    GR_STAR,            // 星号
    GR_NARG,            // *后面的数字
    GR_NARG_NAG_ONE,        // *-1\r\n 等后面的1
    GR_NARG_NAG_ONE_CRLF,   // 等*-1\r\n 等后面的\r
    GR_NARG_ZERO,       // *0\r\n  等\r
    GR_NARG_CRLF,       // *后面的数字后面的CRLF
    GR_READING_LINE,       // 开始读取一行数据
    GR_IN_NEXT_LAYER,    // 在下一层解析

    GR_PLUS,            // 加号
    GR_PLUS_STR,        // 加号后面的字符串
    GR_PLUS_CRLF,       // 加号后面的字符串后面的CRLF
    GR_SUB,             // 减号
    GR_SUB_STR,         // 减号后面的字符串
    GR_SUB_CRLF,        // 减号后面的字符串后面的CRLF
    GR_COLON,           // 冒号
    GR_COLON_NUM,       // 冒号后面的数字
    GR_COLEN_CRLF,      // 冒号后面的数字后面的CRLF
    GR_USD,             // 美元符号
    GR_USD_NUM,         // $后面的数字
    GR_USD_ZERO,        // $0\r\n 等后面的\r
    GR_USD_NUM_CRLF,    // $后面的数字后面的CRLF
    GR_USD_STR,         // $后面的字符串
    //GR_USD_STR_CRLF,    // $字符串结尾的CRLF
    GR_USD_EMPTY,       // 返回的是空字符串$-1\r\n
    GR_USD_EMPTY_ONE,   // $-1/r/n需要读取1
    GR_STR,
    GR_STR_CRLF,
    
    GR_END
}REDIS_MSG_TYPE;

enum
{
    GR_CMD_EVAL,
    GR_CMD_EVALSHA,
    GR_CMD_SENTINEL_GET_MASTER, // sentinel get-master-addr-by-name
    GR_CMD_CLUSTER_NODES,
}GR_REDIS_CMD;

enum
{
}GR_REDIS_ERR;

struct GR_RedisMsgMeta
{
public:
    GR_RedisMsgMeta& operator =(GR_RedisMsgMeta &other);
    uint64 ToUint64();
    int    ToInt();
    char*  ToChar();
    string ToString();
public:
    char    *szStart;   // 一行的起点
    int     iLen;       // 一行的长度
};

struct GR_RedisMsgResults
{
public:
    GR_RedisMsgResults(int iNum = 128);
    ~GR_RedisMsgResults();

    void Reinit();
    GR_RedisMsgMeta* GetNext(); // 获取下一个未使用的元数据,并将iUsed加1
public:
    GR_RedisMsgMeta *vMsgMeta;
    int iTotal;
    int iUsed;  // 消息行数
    int iCode;  // 错误码
};

#define REDISMSG_EXPAND(redis_msg, expand_len)                          \
redis_msg.szEnd = redis_msg.szEnd + expand_len;                         \
GR_RedisMsg *pTmp;                                                      \
for (pTmp=redis_msg.pNextLayer; pTmp!=nullptr; pTmp=pTmp->pNextLayer)   \
{                                                                       \
    pTmp->szEnd = pTmp->szEnd + expand_len;                             \
}

class GR_RedisMsg
{
public:
    GR_RedisMsg();
    ~GR_RedisMsg();

    void Init(char *szNewStart);
    int ParseReq();
    int ParseRsp(GR_RedisMsgResults *pResults = nullptr);

    // 缓存长度不够的时候移动数据
    void Reset(char *szNewStart);
    // 消息长度增加iLen个字节数
    void Expand(int iLen);
    // 解析完一组命令重新开始新的命令解析
    void StartNext();

    int ReInit(char *szNewStart);
private:
    // 解析一行记录，下面为一行
    // $5\r\n12345\r\n
    // +2\r\n12\r\n
    // -1\r\n
    // ....
    inline bool ReadLine(int &iRet, bool bKeyLine);
    // 计算key在第几行
    int KeyInLine();
    bool KeyPing();

public:
    struct MsgInfo
    {
        int         iLen;                   // 消息总长度
        char        *szKeyStart;            // key开始地址
        int         iKeyLen;                // key总长度
        bool        bKeyLine;               // key所在的行
        char        *szCmd;                 // 请求的命令
        int         iCmdLen;                // 命令的长度
        int         iArgLen;                // arg的长度
        int         iNowLine;               // 目前在解析第几行
        int         iKeyInLines;            // key在请求的第几行
        int         iNeedArgNum;            // 当前层级需要的行数
        int         nowState;               // 当前正在解析的类型
        int         iErrFlag;               // 错误类型
        int         iMaxLen;                // 消息中最长的行数
    };
    MsgInfo     m_Info;
    char        *szStart;               // 消息开始地址
    char        *szEnd;                 // 消息的结尾(可供分析的消息的结尾)
    
    char        *szNowPos;              // 当前处理到的位置
    int         iLineLen;
    GR_RedisMsgMeta line;
    uint64      ulIdenty;               // 消息标志 自增id

    GR_Event    *pClient;               // 请求的来处
    GR_RedisMsg *pNextLayer;            // 最多支持3层解析
    
private:
    bool    bReadingLine;   // 正在读取一行数据
};


#define ACCESS_RELEASE 0
#define REDIS_RELEASE 1
// 一条消息的标识
struct GR_MsgIdenty
{
public:
    GR_MsgIdenty(uint8 iStaticFlag = 0);
    void Release(int release);
    int ReplyError(int iErr);
    int GetReply(GR_MemPoolData *pData);
public:
    uint8           iPreGood:1;         // 前面一个消息已经有响应了 0：没有 1：收到响应
    uint8           iWaitRsp:1;         // 自己是否收到响应 0：没收到，1：收到响应
    uint8           iWaitDone:1;        // 已经得到响应了
    uint8           iRspDone:1;         // 响应是否发送完成
    uint8           iStaticFlag:1;      // 不回收标记
    uint16          uiCmdType;
    GR_Event        *pAccessEvent = nullptr;    // 消息的来源
    GR_Event        *pRedisEvent = nullptr;     // 处理消息的redis,如果此值为null则表示没有发送给对应的redis
    GR_MemPoolData  *pData = nullptr;           // 响应的消息的内容
    uint64          ulIndex = 0;                // 消息id
    char*           szErr = nullptr;            // 直接将错误消息发送给客户端run
    void            *pCB = nullptr;             // 回调
    int             iSlot = 0;                  // 消息的分片
    GR_MemPoolData  *pReqData = nullptr;        // 请求消息内容,如果slot正处于扩容期间，则缓存请求的消息
    bool            bRedirect = false;          // 路由信息需要被重定向
    int             iMovedTimes = 0;
    int             iAskTimes = 0;
    uint64          ulReqMS = 0;                 // 请求时间，毫秒
};

#define GR_ASKING_IDENTY()\
GR_MsgIdentyPool::m_pAskingIdenty

#define GR_MSGIDENTY_POOL()\
GR_MsgIdentyPool::m_pInstance


class GR_MsgIdentyPool
{
public:
    ~GR_MsgIdentyPool();
    static GR_MsgIdentyPool* Instance();

    GR_MsgIdenty *Get();
    void Release(GR_MsgIdenty *pIdenty);
    GR_MsgIdenty *GetAsking();

    static GR_MsgIdentyPool* m_pInstance;
    static GR_MsgIdenty *m_pAskingIdenty;   // asking转向标记请求
private:
    GR_MsgIdentyPool();
    
    RingBuffer *m_pIdentyBuffer;
};


#define GR_REDIS_GET_ERR(iErr)\
GR_MsgProcess::m_pInstance->GetErrMsg(iErr)

class GR_MsgProcess
{
public:
    ~GR_MsgProcess();
    static GR_MsgProcess* Instance();

    int Init();
    GR_MemPoolData* GetErrMsg(int iErr);
    GR_MemPoolData* SentinelGetAddrByName(const char *szName, int iLen);
    GR_MemPoolData* SentinelGetAddrByName(const string &strName);
    GR_MemPoolData* SentinelCmd(const char *szSubCmd, const char *szArg = nullptr);
    GR_MemPoolData* SentinelCmd(const char *szSubCmd, int iSubCmdLen, const char *szArg = nullptr, int iArgLen = 0);
    GR_MemPoolData* ClusterCmd();
    GR_MemPoolData* ClusterNodesCmd();
    GR_MemPoolData* AskingCmd();
    GR_MemPoolData* CLusterSlots(char *szIp, uint16 usPort);
    // 发送的消息的长度不要超过2048个字节
    GR_MemPoolData* ReplicateCmd(int iArgNum, ...);

    static GR_MsgProcess* m_pInstance;
private:
    GR_MemPoolData* PackageMsg(int argc, const char **argv, const size_t *argvlen);
    GR_MemPoolData* PackageString(char *szCmd, int iLen);
    GR_MemPoolData* PackageDollarString(char *szCmd, int iLen);
    GR_MemPoolData* PackageInteger(uint64 ulNum);
    GR_MsgProcess();

    GR_MemPoolData **vErrDatas;
};

#define IS_EXPANDS_CMD(m)\
str7icmp(m, 'e', 'x', 'p', 'a', 'n', 'd', 's')
    
#define IS_EXPANDE_CMD(m)\
str7icmp(m, 'e', 'x', 'p', 'a', 'n', 'd', 'e')

#define IS_CLUSTER_CMD(m)\
str7icmp(m, 'c', 'l', 'u', 's', 't', 'e', 'r')

#define IS_MOVED_ERR(m)\
str6icmp(m, '-', 'm', 'o', 'v', 'e', 'd')

#define IS_ASK_ERR(m)\
str4icmp(m, '-', 'a', 's', 'k')

#define IS_SELECT_CMD(m)\
str6icmp(m, 's', 'e', 'l', 'e', 'c', 't')

//INFO
#define IS_CLUSTER_INFO(info)\
(info.iKeyLen == 4 && str4icmp(info.szKeyStart, 'i', 'n', 'f', 'o'))
//KEYSLOT
#define IS_CLUSTER_KEYSLOT(info)\
(info.iKeyLen == 7 && str7icmp(info.szKeyStart, 'k', 'e', 'y', 's', 'l', 'o', 't'))
//NODES
#define IS_CLUSTER_NODES(info)\
(info.iKeyLen == 5 && str5icmp(info.szKeyStart, 'n', 'o', 'd', 'e', 's'))
//SLOTS
#define IS_CLUSTER_SLOTS(info)\
(info.iKeyLen == 5 && str5icmp(info.szKeyStart, 's', 'l', 'o', 't', 's'))

#define IF_CLUSTER_SUPPORT_CMD(info)\
(IS_CLUSTER_SLOTS(info) || IS_CLUSTER_INFO(info) || IS_CLUSTER_KEYSLOT(info) || IS_CLUSTER_NODES(info) )

#endif
