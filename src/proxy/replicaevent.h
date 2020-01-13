#ifndef _GR_REPLICA_EVENT_H__
#define _GR_REPLICA_EVENT_H__
#include "redisevent.h"
#include "config.h"
#include "gr_rdb.h"

/* Slave replication state. Used in server.repl_state for slaves to remember
 * what to do next. */
#define REPL_STATE_NONE 0 /* No active replication */
#define REPL_STATE_CONNECT 1 /* Must connect to master */
#define REPL_STATE_CONNECTING 2 /* Connecting to master */
/* --- Handshake states, must be ordered --- */
#define REPL_STATE_RECEIVE_PONG 3 /* Wait for PING reply */
#define REPL_STATE_SEND_AUTH 4 /* Send AUTH to master */
#define REPL_STATE_RECEIVE_AUTH 5 /* Wait for AUTH reply */
#define REPL_STATE_SEND_PORT 6 /* Send REPLCONF listening-port */
#define REPL_STATE_RECEIVE_PORT 7 /* Wait for REPLCONF reply */
#define REPL_STATE_SEND_IP 8 /* Send REPLCONF ip-address */
#define REPL_STATE_RECEIVE_IP 9 /* Wait for REPLCONF reply */
#define REPL_STATE_SEND_CAPA 10 /* Send REPLCONF capa */
#define REPL_STATE_RECEIVE_CAPA 11 /* Wait for REPLCONF reply */
#define REPL_STATE_SEND_PSYNC 12 /* Send PSYNC */
#define REPL_STATE_RECEIVE_PSYNC 13 /* Wait for PSYNC reply */
/* --- End of handshake states --- */
#define REPL_STATE_TRANSFER 14 /* Receiving .rdb from master */
#define REPL_STATE_CONNECTED 15 /* Connected to master */

#define CONFIG_RUN_ID_SIZE 40

#define PSYNC_WRITE_ERROR 0
#define PSYNC_WAIT_REPLY 1
#define PSYNC_CONTINUE 2
#define PSYNC_FULLRESYNC 3
#define PSYNC_NOT_SUPPORTED 4
#define PSYNC_TRY_LATER 5


// 和被同步的redis不能断开，断开则需要将后端的数据全部清除之后再重新同步数据
class GR_ReplicaEvent: public GR_RedisEvent
{
public:
    GR_ReplicaEvent(int iFD, sockaddr &sa, socklen_t &salen);
    GR_ReplicaEvent(GR_RedisServer *pServer);
    GR_ReplicaEvent();
    virtual ~GR_ReplicaEvent();

    virtual int ConnectSuccess();
    virtual int ConnectFailed();

    virtual int Read();
    virtual int Error();
    virtual int Close(bool bForceClose = false);

private:
    // 发送的消息的长度不要超过2048个字节
    // 传入的不定参数只能为字符串
    int SendCommand(int iArgNum, ...);
    int PrepareSync(int iRead);
    int SyncBulkPayload(int iRead);
    int SyncBulkInit(int &iRead);
    int SendAuth();
    int SendIP();
    int ProcPsyncResult(int &iResult, int &iPassLen);
    int Replicating(int iRead);
    int FilterMsg(bool &bPass);
    int NotSupport(bool &bNotSupport);
    int StartNext(bool bRealseReq = true);

private:
    uint64              m_ulMsgIdenty = 0;
    GR_Rdb              m_rdbParse;
};

class GR_ReplicaServer: public GR_RedisServer
{
public:
    GR_ReplicaServer();
    virtual ~GR_ReplicaServer();
    
    virtual int Connect();

public:
    int     m_iReplState = REPL_STATE_NONE; // 主从同步状态,通过这个状态决定收到master过来的消息之后怎么处理
    char    m_szMasterId[CONFIG_RUN_ID_SIZE + 1];   // 主id
    int64   m_lReplOffSet = -1;                          // 同步偏移量
    int64   m_lInitReplOffSet = -1;
    int64   m_lHeartBeatMS = 0;                         // 心跳发送时间
    int     m_iIdx = -1;
    string  m_strMasterUser = string("");
    string  m_strMasterAuth = string("");

    /* Static vars used to hold the EOF mark, and the last bytes received
     * form the server: when they match, we reached the end of the transfer. */
    char eofmark[CONFIG_RUN_ID_SIZE];
    char lastbytes[CONFIG_RUN_ID_SIZE];
    int usemark = 0;
    size_t repl_transfer_size = -1; /* Size of RDB to read from master during sync. */
    size_t repl_transfer_read = 0; /* Amount of RDB read from master during sync. */
    size_t repl_transfer_last_fsync_off = 0; /* Offset when we fsync-ed last time. */
    long long stat_net_input_bytes = 0; /* Bytes read from network. */
    time_t repl_down_since;
    long long master_repl_offset = 0;   /* My current replication offset */
    long long read_reploff = 0; /* Read replication offset if this is a master. */
};

#define GR_MAX_REPLICA_REDIS 256 // 最多可以和256个redis同步消息
class GR_ReplicaMgr
{
public:
    ~GR_ReplicaMgr();
    static GR_ReplicaMgr* Instance();

    int Init(GR_Config *pConfig);
private:
    int AddRedis(GR_MasterAuth redisInfo, string &strReplInfo);
    int ConnectToRedis();
    
    GR_ReplicaMgr();
    static GR_ReplicaMgr* m_pInstance;

    int                 m_iRedisNum = 0;
    GR_ReplicaServer    *m_vMasterRedis = nullptr;
    
};

#endif
