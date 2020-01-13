#include "define.h"
#include "include.h"
#include "proxy.h"
#include "log.h"
#include "options.h"
#include "daemonize.h"
#include "proxyhelp.h"
#include "gr_signal.h"
#include "gr_proxy_global.h"
#include "replicaevent.h"
#include "gr_loadevent.h"
#include "replicaevent.h"

#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>

void GR_SendSignal(int signal);

using namespace std;

GR_Proxy* GR_Proxy::m_pInstance = new GR_Proxy();

GR_Proxy::GR_Proxy()
{
}

GR_Proxy* GR_Proxy::Instance()
{
	return m_pInstance;
}

GR_Proxy::~GR_Proxy()
{
    try
    {
        this->m_Shm.Free();
    }
    catch(exception &e)
    {
        GR_LOGE("destroy proxy got exception:%s", e.what());
    }
}

int GR_Proxy::Init()
{
    try
    {
        this->m_iPid = getpid();
        if (GR_OK != this->m_Config.Load(GR_Options::Instance()->confFileName))
        {
            cout<<"load config file failed:"<< GR_Options::Instance()->confFileName<< endl;;
            return GR_ERROR;
        }
        this->m_iRouteMode = this->m_Config.m_iRouteMode;
        this->m_iSvrType = this->m_Config.m_iSvrType;;
        if  (GR_OK != this->m_Shm.Alloc(GR_ProxyShareInfo::Instance()->Size()))
        {
            cout<< "init share memery failed." << endl;
            return GR_ERROR;
        }
        if (GR_OK != this->m_Shm.ShmAt())
        {
            cout << "process shmat failed." << endl;
            return GR_ERROR;
        }
    }
    catch(exception &e)
    {
        cout << "init proxy got exception"<< e.what() << endl;
        return GR_ERROR;
    }

    return GR_OK;
}

int GR_Proxy::InitChildrenInfo(int iIdx)
{
    m_iParentPid = m_iPid;
    m_iPid = getpid();
    m_ulInnerId = iIdx;
    gr_work_pid = m_iPid;
    GR_ProxyShareInfo::Instance()->UpdateTimes();

    auto LogPath = this->m_Config.m_strLogFile;
    if (!GR_LOG_INIT(LogPath.c_str(), "gredis-proxy"))
    {
        cout << "init log failed:" << iIdx << endl;
        return GR_ERROR;
    }

    // TODO 解除信号屏蔽，注册信号处理函数
    sigset_t set;
    sigemptyset(&set);
    if (sigprocmask(SIG_SETMASK, &set, NULL) == -1) {
        cout <<"sigprocmask failed errmsg:"<< strerror(errno) << endl;
        return GR_ERROR;
    }

    int status = GR_SignalInit();
    if (status != GR_OK)
    {
        GR_STDERR("init signal failed.");
        return GR_ERROR;
    }

    return GR_OK;
}

int GR_Proxy::WorkPrepare(int iIdx)
{
    gr_process = PROXY_SVR_TYPE_WORK;
    //this->m_iSvrType = PROXY_SVR_TYPE_WORK;
    if (GR_OK != this->InitChildrenInfo(iIdx))
    {
        cout << "init child info failed:" << getpid() << endl;
        return GR_ERROR;
    }

    if (!GR_Epoll::Instance()->Init(MAX_EVENT_POOLS)) // 重新初始化epoll，销毁主进程继承过来的事件
    {
        cout << "epoll init failed." << endl;
        return GR_ERROR;
    }

    int status;   
    // 将父进程继承过来的无用的句柄都关闭并从epoll触发器中删除
    // 1、关闭admin监听句柄
    if (this->m_AdminMgr.m_iListenFD > 0)
    {
        close(this->m_AdminMgr.m_iListenFD);
        this->m_AdminMgr.m_iListenFD = 0;
    }
    // 2、关闭继承过来的channel句柄
    GR_ProxyShareInfo* pShareInfo = GR_ProxyShareInfo::Instance();
    GR_WorkProcessInfo *pWorkInfo = nullptr;
    int iTmpFD = 0;

    // 和后端redis建立连接
    status = GR_RedisMgr::Instance()->Init(&this->m_Config);
    if (GR_OK != status)
    {
        GR_LOGE("redis mgr init failed:%d, %d", iIdx, status);
        return status;
    }

    status = m_AccessMgr.Init();
    if (GR_OK != status)
    {
        GR_LOGE("access mgr init failed:%d, %d", iIdx, status);
        return status;
    }

    if (this->m_iSvrType == PROXY_SVR_TYPE_REPLICA)
    {
        if (GR_OK != GR_ReplicaMgr::Instance()->Init(&this->m_Config))
        {
            cout << "replicate init failed." << endl;
            return GR_ERROR;
        }
    }

    return GR_OK;
}

int GR_Proxy::LoadRdbFile(string strRdbFile)
{
    return GR_ERROR;
}

int GR_Proxy::LoadAofFile(string strAofFile)
{
    FILE *fp = nullptr;
    static GR_LoadAofEvent *pEvent = nullptr;
    try
    {
        // 1、打开文件
        fp = fopen(strAofFile.c_str(), "r");
        if (fp == nullptr)
        {
            GR_LOGE("open aof file for read failed %s", strerror(errno));
            goto proc_err;
        }
        if (pEvent == nullptr)
        {
            pEvent = new GR_LoadAofEvent();
            if (GR_OK != pEvent->Init())
            {
                GR_LOGE("init event failed:%s", strAofFile.c_str());
                goto proc_err;
            }
        }
        // 2、校验文件头
        if (GR_OK != pEvent->Loading(fp, strAofFile))
        {
            GR_LOGE("loading data from aof got failed:%s", strAofFile.c_str());
            goto proc_err;
        }
        // 3、开始读取加载文件内存
        int iRet;
        while(1)
        {
            if (pEvent->m_iPending != 0)
            {
                pEvent->m_iPending = 0;
                iRet = pEvent->ClearPending();
                if (iRet != GR_OK)
                {
                    if (iRet == GR_FULL)
                    {
                        pEvent->m_iPending = 1;
                    }
                    else
                    {
                        goto proc_err;
                    }
                }
            }
            else if (pEvent->m_iProcFlag != GR_LOAD_FINISH_READ)
            {
                if (GR_OK != pEvent->Read())
                {
                    goto proc_err;
                }
            }
            GR_Epoll::Instance()->EventLoopProcess(100);
            GR_Epoll::Instance()->ProcEventNotType(GR_LISTEN_EVENT);
            if (pEvent->m_iProcFlag == GR_LOAD_FINISH || pEvent->m_iProcFlag == GR_LOAD_ERROR)
            {
                break;
            }
        }
        if (pEvent->m_iProcFlag == GR_LOAD_ERROR)
        {
            goto proc_err;
        }
        // 4、关闭文件
        fclose(fp);
        fp = nullptr;
    }
    catch(exception &e)
    {
        if (pEvent!=nullptr && pEvent->m_iFD > 0)
        {
            pEvent->m_iFD = 0;
        }
        // 进程直接退出了，不需要手动清除资源，防止再抛出异常
        GR_LOGE("load aof file got exception:%s", e.what());
        return GR_ERROR;
    }
    
    return GR_OK;

proc_err:
    if (pEvent!=nullptr && pEvent->m_iFD > 0)
    {
        pEvent->m_iFD = 0;
    }
    if (fp != nullptr)
    {
        fclose(fp);
    }
    
    return GR_ERROR;
}

int GR_Proxy::WorkLoop()
{
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);

    int     iRet = GR_OK;
    timeval tv, *tvp;
    uint64  ulMix;
    uint64  ulSkip;
    uint64  ulNow;
    bool    bAcceptSuccess = false;
    int     iAcceptNum = 0;
    int     iLowClients = m_Config.m_iWorkConnections / 10;
    GR_Epoll *pEpoll = GR_Epoll::Instance();
    GR_ProxyShareInfo *pShareInfo = GR_ProxyShareInfo::Instance();
    uint64  ulLoop = 0;
    uint64  ulAccessCheckMS = 0;
    uint64  ulRedisCheckMS = 0;
    for(;;)
    {
        ulSkip = ulMix - ulNow;
        if (ulSkip < 100)
        {
            ulSkip = 100;
        }
        pEpoll->EventLoopProcess(ulSkip); 
        m_AccessMgr.ProcPendingEvents();
        iAcceptNum = pEpoll->ProcEvent(GR_LISTEN_EVENT);
        pEpoll->ProcEventNotType(GR_LISTEN_EVENT);
        ulNow = CURRENT_MS();
        GR_Timer::Instance()->Loop(ulNow, ulMix);
        if (ulNow - ulRedisCheckMS > 10*1000)
        {
            ulRedisCheckMS = ulNow;
            GR_RedisMgr::Instance()->LoopCheck();
        }
        if (ulNow - ulAccessCheckMS > 60*10*1000)
        {
            ulAccessCheckMS = ulNow;
            // 检查客户端连接超时
            this->m_AccessMgr.LoopCheck();
        }
        // 收到退出信号
        if (gr_global_signal.Exit == 1)
        {
            break;
        }
    }

    return GR_OK;
}

int GR_Proxy::MasterLoop()
{
    GR_Epoll::Instance()->EventLoopProcess(10);
    GR_Epoll::Instance()->ProcEvent(GR_LISTEN_EVENT);
    GR_Epoll::Instance()->ProcEventNotType(GR_LISTEN_EVENT);
    return GR_OK;
}

int GR_Proxy::ReplicaLoop()
{
    GR_Epoll::Instance()->EventLoopProcess(100);
    GR_Epoll::Instance()->ProcEventNotType(GR_LISTEN_EVENT);
    return GR_OK;
}

int GR_Proxy::LoadData(int iIdx, int iTotalProc)
{
    if (GR_OK != this->InitChildrenInfo(0))
    {
        cout << "init child info failed:" << getpid() << endl;
        return GR_ERROR;
    }
    if (!GR_Epoll::Instance()->Init(MAX_EVENT_POOLS)) // 重新初始化epoll，销毁主进程继承过来的事件
    {
        GR_LOGE("epoll init failed.");
        return GR_ERROR;
    }

    // 和后端redis建立连接
    int status = GR_RedisMgr::Instance()->Init(&this->m_Config);
    if (GR_OK != status)
    {
        GR_LOGE("redis mgr init failed %d", status);
        return status;
    }
    
    GR_Config *pConfig = &this->m_Config;
    vector<string> vFiles;
    if (pConfig->m_iSvrType==PROXY_SVR_TYPE_RDB)
    {
        vFiles = pConfig->m_listRdbFiles;
    }
    else
    {
        vFiles = pConfig->m_listAofFiles;
    }
    ASSERT(iIdx>=0 && iIdx<iTotalProc && iTotalProc<=vFiles.size());

    for(; iIdx<vFiles.size(); iIdx+=iTotalProc)
    {
        string strFilePath = vFiles[iIdx];
        if (pConfig->m_iSvrType==PROXY_SVR_TYPE_RDB)
        {
            if (GR_OK != this->LoadRdbFile(strFilePath))
            {
                GR_LOGE("load rdb file failed %s", strFilePath.c_str());
                return GR_ERROR;
            }
        }
        else
        {
            if (GR_OK != this->LoadAofFile(strFilePath))
            {
                GR_LOGE("load aof file failed %s", strFilePath.c_str());
                return GR_ERROR;
            }
        }
    }
    return GR_OK;
}

// 单进程模式运行
static void
GR_SingleMode()
{
    GR_Proxy *pProxy = GR_Proxy::Instance();
    GR_AdminMgr *pAdmin = &pProxy->m_AdminMgr;
    if (GR_OK != pAdmin->Init())
    {
        GR_LOGE("admin init failed.");
        return;
    }
    int iIdx = 0;
    if (GR_OK != pProxy->WorkPrepare(iIdx))
    {
        GR_LOGE("work process prepare failed:%d", iIdx);
        return;
    }
    pProxy->WorkLoop();
    GR_LOGI("work process exit %d,%d", iIdx, pProxy->m_iPid);
    return;
}

// 子进程加载完在共享内存中更新状态为加载完成，然后退出，父进程收到子进程退出消息后检测状态
void GR_StartLoadProcess(int iIdx, int iTotalProx)
{
    if (iIdx >= MAX_WORK_PROCESS_NUM)
    {
        cout << "-------------create work process faild, too many work process--------------" << endl;
        return;
    }

    GR_ProxyShareInfo *pShareInfo = GR_ProxyShareInfo::Instance();
    GR_WorkProcessInfo *pWorkInfo = &pShareInfo->Info->Works[iIdx];
    GR_Proxy *pProxy = GR_Proxy::Instance();

    pid_t pid = fork();
    switch (pid)
    {
        case -1:    // 出错
        {
            GR_STDERR("create work process failed:%d", iIdx);
            return;
        }
        case 0:     // 子进程
        {
            try
            {
                setproctitle("gredis-proxy load-process %d", iIdx);
                if (GR_OK == pProxy->LoadData(iIdx, iTotalProx))
                {
                    GR_LOGI("load work process exit, success %d,%d", iIdx, pid);
                    pWorkInfo->iStatus = PROXY_STATUS_LOAD_SUCCESS;
                }
                else
                {
                    GR_LOGE("load work process exit, failed %d,%d", iIdx, pid);
                    pWorkInfo->iStatus = PROXY_STATUS_LOAD_ERR;
                }
            }
            catch(exception &e)
            {
                cout << "start work process failed:" << iIdx << endl;
                abort();
            }
            exit(0);
        }
        default:    // 父进程
        {
            try
            {
                pWorkInfo->pid = pid;
                pWorkInfo->iStatus = PROXY_STATUS_RUN;
                GR_STDERR("create load work process success index:%d, pid:%d", iIdx, pid);
            }
            catch(exception &e)
            {
                GR_STDERR("master start work process got exception:%s", e.what());
            }
            return;
        }
    }
    return;
}

void GR_MasterLoadProcess(int iTotalProcNum)
{
    sigset_t set;
    sigemptyset(&set);
    if (sigprocmask(SIG_SETMASK, &set, NULL) == -1) {
        GR_STDERR("sigprocmask failed, errno:%d, errmsg:%s", errno, strerror(errno));
        return;
    }
    int iStatus = GR_SignalInit();
    if (iStatus != GR_OK)
    {
        GR_STDERR("init signal failed.");
        return;
    }
    
    gr_process = PROXY_SVR_TYPE_MASTER;
    setproctitle("gredis-proxy load-master");
    GR_Proxy* pProxy = GR_Proxy::Instance();

    GR_ProxyShareInfo *pShareInfo = GR_ProxyShareInfo::Instance();
    int     iFinishNum = 0;
    for (;;)
    {
        try
        {
            pause();
            // 有子进程退出了
            if (gr_global_signal.ChildExit == 1)
            {
                pid_t pid;
                GR_GInfo* pInfo = pShareInfo->Info;
                for(;;)
                {
                    pid = waitpid(-1, &iStatus, WNOHANG);
                    if (pid == 0)
                    {
                        GR_LOGE("waitpid got 0.");
                        break;
                    }
                    if (pid == -1)
                    {
                        if (errno == EINTR)
                        {
                            continue;
                        }
                        GR_LOGE("waitpid failed, errno:%d, errmsg:%s", errno, strerror(errno));
                        return;
                    }
                    // 更新子进程状态
                    for(int i=0; i<iTotalProcNum; i++)
                    {
                       if (pInfo->Works[i].pid == pid)
                       {
                            if (PROXY_STATUS_LOAD_SUCCESS != pInfo->Works[i].iStatus)
                            {
                                GR_LOGE("load data from file failed, please clear the data loaded and check...");
                                GR_SendSignal(SIGQUIT);
                                break;
                            }
                            else
                            {
                                iFinishNum+=1;
                                GR_LOGI("one of the load processes finish %d/%d", iFinishNum, iTotalProcNum);
                                if (iFinishNum >= iTotalProcNum)
                                {
                                    GR_LOGI("load data finish...");
                                    break;
                                }
                            }
                       }
                    }
                }
            }
            // 向所有子进程发出退出信号
            if (gr_global_signal.Exit == 1)
            {
                GR_SendSignal(SIGQUIT);
                break;
            }

            gr_global_signal.Reset();
        }
        catch(exception &e)
        {
            GR_LOGE("master process got exception:%s", e.what());
        }
    }
}

// 最多启动cpu个数的进程，给每个进程平均分配aof文件
void GR_LoadMode(PROXY_SVR_TYPE      svrType)
{
    int iProcNum = get_nprocs();
    GR_Proxy *pProxy = GR_Proxy::Instance();
    GR_Config *pConfig = &pProxy->m_Config;
    switch(svrType)
    {
        case PROXY_SVR_TYPE_AOF:
        {
            if (iProcNum > pConfig->m_listAofFiles.size())
            {
                iProcNum = pConfig->m_listAofFiles.size();
            }
            break;
        }
        case PROXY_SVR_TYPE_RDB:
        {
            if (iProcNum > pConfig->m_listRdbFiles.size())
            {
                iProcNum = pConfig->m_listRdbFiles.size();
            }
            break;
        }
    }

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGUSR2);
    sigaddset(&set, SIGALRM);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGTERM);
    if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) 
    {
        GR_STDERR("sigprocmask failed, errno:%d, errmsg:%s", errno, strerror(errno));
        return;
    }
    if (iProcNum > MAX_WORK_PROCESS_NUM)
    {
        iProcNum = MAX_WORK_PROCESS_NUM;
    }
    for (int idx=0; idx<iProcNum; idx++)
    {
        GR_StartLoadProcess(idx, iProcNum);
    }
    GR_MasterLoadProcess(iProcNum);
    return;
}

// 启动子工作进程
static void 
GR_StartWorkProcess(int iIdx, bool bNewStart=true)
{
    if (iIdx >= MAX_WORK_PROCESS_NUM)
    {
        cout << "-------------create work process faild, too many work process--------------" << endl;
        return;
    }

    GR_ProxyShareInfo *pShareInfo = GR_ProxyShareInfo::Instance();
    GR_WorkProcessInfo *pWorkInfo = &pShareInfo->Info->Works[iIdx];
    // 父子进程通信通道初始化
    if (GR_OK != pWorkInfo->Init())
    {
        GR_STDERR("init channle failed:%d", iIdx);
        return;
    }

    GR_Proxy *pProxy = GR_Proxy::Instance();

    pid_t pid = fork();
    switch (pid)
    {
        case -1:    // 出错
        {
            GR_STDERR("create work process failed:%d", iIdx);
            return;
        }
        case 0:     // 子进程
        {
            try
            {
                setproctitle("gredis-proxy workprocess %d", iIdx);
                if (GR_OK != pProxy->WorkPrepare(iIdx))
                {
                    GR_STDERR("work process prepare failed:%d", iIdx);
                    exit(0);
                }
            }
            catch(exception &e)
            {
                cout << "start work process failed:" << iIdx << endl;
                exit(0);
            }
            pProxy->WorkLoop();
            GR_LOGI("work process exit %d,%d", iIdx, pid);
            exit(0);
        }
        default:    // 父进程
        {
            try
            {
                pWorkInfo->pid = pid;
                pWorkInfo->iStatus = PROXY_STATUS_RUN;
                GR_STDERR("create work process success index:%d, new start:%d, pid:%d", iIdx, bNewStart, pid);
            }
            catch(exception &e)
            {
                GR_STDERR("master start work process got exception:%s", e.what());
            }
            return;
        }
    }
    return;
}

void GR_GetChildStatus()
{
    int iStatus;
    pid_t pid;
    GR_GInfo* pInfo = GR_ProxyShareInfo::Instance()->Info;
    for(;;)
    {
        pid = waitpid(-1, &iStatus, WNOHANG);
        if (pid == 0)
        {
            return;
        }
        if (pid == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            GR_LOGE("waitpid failed, errno:%d, errmsg:%s", errno, strerror(errno));
            return;
        }
        // 更新子进程状态
        for(int i=0; i<GR_Proxy::Instance()->m_iChildIdx; i++)
        {
           if (pInfo->Works[i].pid == pid)
           {
                pInfo->Works[i].iStatus = PROXY_STATUS_EXIT;
           }
        }
    }

}

void GR_RestartChild()
{
    GR_GInfo* pInfo = GR_ProxyShareInfo::Instance()->Info;
    for(int i=0; i<GR_Proxy::Instance()->m_iChildIdx; i++)
    {
       if (pInfo->Works[i].iStatus == PROXY_STATUS_EXIT)
       {
            // 重新启动进程
            GR_StartWorkProcess(i, false);
       }
    }
}

void GR_SendSignal(int signal)
{
    GR_GInfo* pInfo = GR_ProxyShareInfo::Instance()->Info;
    for(int i=0; i<GR_Proxy::Instance()->m_iChildIdx; i++)
    {
        kill(pInfo->Works[i].pid, signal);
    }
}

int GR_StartNewChild()
{
    GR_Proxy *pProxy = GR_Proxy::Instance();
    if (pProxy->m_iChildIdx >= MAX_WORK_PROCESS_NUM)
    {
        GR_STDERR("can not create new child proxy, got maximal.");
        return GR_ERROR;
    }
    GR_StartWorkProcess(pProxy->m_iChildIdx);
    pProxy->m_iChildIdx += 1;
    return GR_OK;
}

// 监控进程
// 1、检查子进程有没有挂，挂了则清除子进程的accept锁
// 2、收到信号增加新的子进程，提高服务能力
static void
GR_MasterProcess()
{
    sigset_t set;
    sigemptyset(&set);
    if (sigprocmask(SIG_SETMASK, &set, NULL) == -1) {
        GR_STDERR("sigprocmask failed, errno:%d, errmsg:%s", errno, strerror(errno));
        return;
    }
    int status = GR_SignalInit();
    if (status != GR_OK)
    {
        GR_STDERR("init signal failed.");
        return;
    }
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    
    // TODO 检查进程状态，如果有子进程没有启动成功则退出，防止因为进程bug一直down机导致无限重启
    // kill(pid,0);
    gr_process = PROXY_SVR_TYPE_MASTER;
    setproctitle("gredis-proxy master");
    GR_Proxy* pProxy = GR_Proxy::Instance();
    int iTimePrecision = pProxy->m_Config.m_iTimePrecision/2;
    if (iTimePrecision < 1)
    {
        iTimePrecision = 1;
    }
    SetSysTimer(iTimePrecision);

    GR_AdminMgr *pAdmin = &pProxy->m_AdminMgr;
    if (GR_OK != pAdmin->Init())
    {
        GR_LOGE("admin init failed.");
        return;
    }
    GR_ProxyShareInfo *pShareInfo = GR_ProxyShareInfo::Instance();
    int64  ulStartMS = pShareInfo->GetCurrentMS(true);
    int64  ulNowMS = ulStartMS;
    for (;;)
    {
        try
        {
            pProxy->MasterLoop();
            
            pShareInfo->UpdateTimes();
            // 检查accept锁时间，超时则清除
            ulNowMS =  pShareInfo->GetCurrentMS(true);
            if (ulNowMS-2*GR_EVENT_TT > pShareInfo->Info->ShareLock.GetValue())
            {
                pShareInfo->Info->ShareLock.SetValue(0);
            }
            // 起一个新的进程
            if (gr_global_signal.NewChild == 1)
            {
                GR_StartNewChild();
            }
            // 有子进程退出了
            if (gr_global_signal.ChildExit == 1)
            {
                // TODO 防止子进程一直down，一直重启死循环
                // 如果启动五分钟之内就有进程down了，则系统退出
                if (ulNowMS - ulStartMS < 1000 * 60 *1)
                {
                    GR_SendSignal(SIGQUIT);
                    GR_LOGE("child work process exist at start %ld", ulStartMS);
                    break;
                }
                GR_GetChildStatus();
                GR_RestartChild();
            }
            // 向所有子进程发出退出信号
            if (gr_global_signal.Exit == 1)
            {
                GR_SendSignal(SIGQUIT);
                break;
            }

            gr_global_signal.Reset();
        }
        catch(exception &e)
        {
            GR_LOGE("master process got exception:%s", e.what());
        }
    }
}

static int GR_PreRun()
{
    int status;
    if (GR_OK != GR_MsgProcess::Instance()->Init())
    {
       cout << "error desc init failed." << endl;
       return GR_ERROR;
    }

    GR_Proxy *pProxy = GR_Proxy::Instance();
    if (GR_OK != pProxy->Init())
    {
        cout << "init proxy failed." << endl;
        return GR_ERROR;
    }

    if (!GR_Epoll::Instance()->Init(MAX_EVENT_POOLS)) // 父进程使用的epoll，子进程销毁重新创建epoll句柄
    {
        GR_STDERR("init epoll failed.");
        return GR_ERROR;
    }

    GR_ProxyShareInfo *pShareInfo = GR_ProxyShareInfo::Instance();
    pShareInfo->Bind(pProxy->m_Shm.m_szAddr); // 共享内存在此处绑定，子进程直接用
    pShareInfo->UpdateTimes();
    GR_WorkProcessInfo *pTmpWorkInfo = nullptr;
    GR_MasterChannelEvent *vEventList = new GR_MasterChannelEvent[MAX_WORK_PROCESS_NUM]; // 不会重复申请，不主动释放，主进程结束系统自动释放
    for(int i=0; i<MAX_WORK_PROCESS_NUM; i++)
    {
        pTmpWorkInfo = &pShareInfo->Info->Works[i];
        pTmpWorkInfo->pEvent = &vEventList[i];
        vEventList[i].m_iChildIdx = i;
    }
    
    gr_work_pid = getpid();
    return GR_OK;
}

// 启动子进程,iWorkProcess为子进程个数
static int GR_Run(int iWorkProcess)
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGUSR2);
    sigaddset(&set, SIGALRM);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGTERM);
    if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) {
        GR_STDERR("sigprocmask failed, errno:%d, errmsg:%s", errno, strerror(errno));
        return GR_ERROR;
    }
    if (iWorkProcess > MAX_WORK_PROCESS_NUM)
    {
        iWorkProcess = MAX_WORK_PROCESS_NUM;
    }
    // 启动子进程
    for (auto i=0; i<iWorkProcess; i++)
    {
        GR_StartNewChild();
    }
    GR_MasterProcess();
    return GR_OK;
}

static int GR_PostRun()
{
    return GR_OK;
}

static int GR_GetOptions(int argc, char **argv)
{
    int c, value;
    opterr = 0;
    for(;;)
    {
        c = getopt_long(argc, argv, short_options, long_options, NULL);
        if (c == -1) 
        {
            /* no more options */
            break;
        }
        switch (c)
        {
        case 'w':
        {
            sscanf(optarg, "%d", &GR_Options::Instance()->works);
            if (GR_Options::Instance()->works < 1)
            {
                GR_STDERR("gredis: invalid option -- '%c'", optopt);
                return GR_ERROR;
            }
            break;
        }
        case 'h':
            GR_Options::Instance()->showHelp = 1;
            break;
        case 'c':
            GR_Options::Instance()->confFileName = optarg;
            break;
        case 'd':
            GR_Options::Instance()->daemonize = 1;
            break;
        case 'p':
            GR_Options::Instance()->pidFile = optarg;
            break;
        default:
            GR_STDERR("gredis: invalid option -- '%c'", optopt);
            return GR_ERROR;
        }

    }
    return GR_OK;
}

void nomorememory()
{
    cout << "unable to satisfy request for memory" << endl;

    abort();
}

int main(int argc, char **argv)
{
    try
    {
        set_new_handler(nomorememory);
        spt_init(argc, argv);
        int status = 0;
        status = GR_SignalInit();
        if (status != GR_OK)
        {
            cout << "init signal failed" << endl;
            return -1;
        }
        status = GR_GetOptions(argc, argv);
        if (status != GR_OK)
        {
            cout <<"gredis: get option failed" << endl;;
            return -1;
        }

        // 帮助信息
        if (GR_Options::Instance()->showHelp == 1)
        {
            ShowHelp();
            return 0;
        }

        if (GR_Options::Instance()->daemonize == 1)
        {
            status = GR_Daemonize(1);
            if (status != GR_OK)
            {
                return status;
            }
        }

        status = GR_PreRun();
        if (status != GR_OK)
        {
            cout << "pre run failed" << endl;
            return -1;
        }
    }
    catch(exception &e)
    {
        cout << "start gredis-proxy got exception:" << e.what() << endl;
    }

    GR_Proxy *pProxy = GR_Proxy::Instance();
    switch (pProxy->m_iSvrType)
    {
        case PROXY_SVR_TYPE_AOF:
        case PROXY_SVR_TYPE_RDB:
        {
            GR_LoadMode(pProxy->m_iSvrType);
            break;
        }
        default:
        {
            GR_Config* pConfig = &pProxy->m_Config;
            int iWorkNum = pConfig->m_iWorkProcess;
            if (pProxy->m_iSvrType == PROXY_SVR_TYPE_REPLICA)
            {
                iWorkNum = 1;
            }
            if (iWorkNum == 0)
            {
                GR_SingleMode(); // just for test
            }
            else
            {
                GR_Run(iWorkNum);
            }
        }
    }
    
    GR_PostRun();
    
    return 0;
}

