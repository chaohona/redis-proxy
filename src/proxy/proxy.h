#ifndef _GR_PROXY_H__
#define _GR_PROXY_H__

#include "define.h"
#include "gr_channel.h"
#include "redismgr.h"
#include "accesslayer.h"
#include "config.h"
#include "gr_shm.h"
#include "gr_adminevent.h"

#define GR_PROXY_INSTANCE()\
GR_Proxy::m_pInstance


#define GR_IS_CLUSTER_MODE()\
(GR_Proxy::m_pInstance->m_iRouteMode == PROXY_ROUTE_CLUSTER)

#define GR_IS_TINY_MODE()\
(GR_Proxy::m_pInstance->m_iRouteMode == PROXY_ROUTE_TINY)


// 代理进程信息管理
class GR_Proxy
{
public:
    static GR_Proxy *Instance();
    ~GR_Proxy();

    int Init();
    // 工作进程启动前的准备工作
    int WorkPrepare(int iIdx);
    // 工作进程主循环
    int WorkLoop();
    int MasterLoop();
    int ReplicaLoop();
    int LoadData(int iIdx, int iTotalProc);
private:
    GR_Proxy();    
    // 子进程信息初始化
    int InitChildrenInfo(int iIdx);
    int LoadRdbFile(string strRdbFile);
    int LoadAofFile(string strAofFile);
public:
    static GR_Proxy* m_pInstance;

public:
    pid_t               m_iParentPid;           // master进程id
    pid_t               m_iPid;                 // 自己进程id
    int                 m_iFlag;                // 是master还是work进程
    int                 m_iStatus;              // 进程状态

    static uint64       m_ulNextId;             // 下一个子进程
    uint64              m_ulInnerId;            // 本进程的下标

    GR_AccessMgr        m_AccessMgr;            // 和前端的连接管理
    GR_Config           m_Config;               // 配置文件管理
    GR_AdminMgr         m_AdminMgr;             // 管理接口

    // 子进程信息
    GR_WorkChannelEvent   *m_pWorkChannelEvent;

    int                 m_iChildIdx = 0;
    GR_Shm              m_Shm;

    GR_REDIS_ROUTE_TYPE  m_iRouteMode;
    PROXY_SVR_TYPE       m_iSvrType;
};

#endif
