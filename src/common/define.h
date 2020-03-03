#ifndef _GR_DEFINE_H__
#define _GR_DEFINE_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <exception>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/sysinfo.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <arpa/inet.h>

#include <map>
#include <list>
#include <string>
#include <hash_map>
#include <unordered_map>

using namespace std;

typedef unsigned char   uchar;
typedef unsigned char   uint8;
typedef unsigned short  uint16;
typedef uint32_t        uint32;
typedef int32_t         int32;
typedef uint64_t        uint64;
typedef int64_t         int64; 

enum PROXY_SVR_TYPE
{
    PROXY_SVR_TYPE_MASTER,  //
    PROXY_SVR_TYPE_WORK,    // 
    PROXY_SVR_TYPE_CONFIG,  //
    PROXY_SVR_TYPE_REPLICA, // 同步数据的进程
    PROXY_SVR_TYPE_AOF,
    PROXY_SVR_TYPE_RDB,
};

enum GR_PROXY_STATUS{
    PROXY_STATUS_INIT,
    PROXY_STATUS_RUN,
    PROXY_STATUS_EXPAND,
    PROXY_STATUS_EXIT,
    PROXY_STATUS_LOAD_SUCCESS,  // 加载成功
    PROXY_STATUS_LOAD_ERR,      // 加载失败
};

#define MAX_WORK_PROCESS_NUM    256  // 处理进程个数上线

// 系统中通用错误都是负号
#define GR_OK           0
#define GR_ERROR        -1
#define GR_EAGAIN       -2
#define GR_ENOMEM       -3
#define GR_NOREDIS      -4
#define GR_EXCEPTION    -5
#define GR_FULL         -10
#define GR_RDB_END          -1024

enum{
    REDIS_RSP_OK,
    REDIS_RSP_PING,
    REDIS_RSP_PONG,
    REDIS_RSP_COMM_ERR,
    REDIS_RSP_DISCONNECT,           // 和redis连接断开了
    REDIS_RSP_UNSPPORT_CMD,         // 不支持的命令
    REDIS_RSP_SYNTAX,               // 语法错误
    REDIS_RSP_MOVED,                // 重定向错误
    REDIS_REQ_ASK,
    REDIS_REQ_TO_LARGE,
    REDIS_ERR_END
}GR_STR_RSP;

#define REDIS_RSP_OK_DESC               "+OK\r\n"
#define REDIS_RSP_PING_DESC             "+PING\r\n"
#define REDIS_RSP_PONG_DESC             "+PONG\r\n"
#define REDIS_RSP_COMM_ERR_DESC         "-inner error happend\r\n"
#define REDIS_RSP_DISCONNECT_DESC       "-redis disconnect\r\n"
#define REDIS_RSP_UNSPPORT_CMD_DESC     "-ERR unspport command\r\n"
#define REDIS_RSP_SYNTAX_DESC           "-ERR syntax error\r\n"
#define REDIS_REQ_TO_LARGE_DESC         "-ERR request is too large\r\n"
#define REDIS_REQ_ASK_DESC              "*1\r\n$6\r\nASKING\r\n"



typedef int RSTATUS; /* return type */
typedef int ERR;     /* error type */

#define UNUSED(V) ((void) V)

#define MAX_REDIS_MSG_IDENTY_POOL     65535   // 消息标记缓存池子

#define ACCESS_NET_BUFFER       1024*256     // 256K 客户端网络层缓存
#define REDIS_MSG_INIT_BUFFER   1024    

#define MAX_WAIT_RING_LEN   0xFFF           // 客户端缓存为得到响应的请求的个数，超过此个数则和客户端断开连接

#define REDIS_RSP_STATUS_WAIT   0   // 等待响应
#define REDIS_RSP_STATUS_GOOD   1   // 
#define REDIS_RSP_STATUS_DONE   2

#define MAX_REDIS_GROUP     1024    // 最多1024组redis
#define MAX_REDIS_SLAVES    8       // 每组最多有8个从redis

#define NET_IP_STR_LEN 46 /* INET6_ADDRSTRLEN is 46, but we need to be sure */

// TODO 命名改下，改为REDIS_ROUTE_TYPE
enum GR_REDIS_ROUTE_TYPE{
    PROXY_ROUTE_INVALID,
    PROXY_ROUTE_TWEM,
    PROXY_ROUTE_CODIS,
    PROXY_ROUTE_CLUSTER,
    PROXY_ROUTE_TINY,
};

enum GR_REDIS_TYPE{
    REDIS_TYPE_NONE,
    REDIS_TYPE_MASTER,
    REDIS_TYPE_SLAVE,
    REDIS_TYPE_CLUSTER_SEED,
    REDIS_TYPE_SENTINEL,
    REDIS_TYPE_REPLICA,
};


enum GR_LOG_LEVEL{
    GR_LOG_LEVEL_NONE,
    GR_LOG_LEVEL_ERROR,
    GR_LOG_LEVEL_WARNING,
    GR_LOG_LEVEL_INFO,
    GR_LOG_LEVEL_DEBUG,
};

enum GR_SLOT_FLAG{
    GR_SLOT_SURE,
    GR_SLOT_NOT_SURE,
};

#define GR_REDIS_DEF_RETRY_MS   100      // 短线重连间隔

#define GR_EVENT_TT 1000

typedef pid_t       GR_Pid;
#define GR_INVALID_PID  -1

#define GR_CLUSTER_SLOTS_MIX_CASE 3
#define GR_MAX_GROUPS  256    // 最多可以同时管理256个集群

struct continuum {
    uint32 index;  /* server index */
    uint32 value;  /* hash value */
};

#endif
