#include "gr_shm.h"
#include <sys/ipc.h>
#include <sys/shm.h>

GR_Shm::GR_Shm()
{
}

GR_Shm::~GR_Shm()
{
}

int GR_Shm::Alloc(size_t size)
{
    try
    {
        int id;
        id = shmget(IPC_PRIVATE, size, (SHM_R|SHM_W|IPC_CREAT));
        if (id == -1)
        {
            GR_LOGE("shmget failed %d", size);
            return GR_ERROR;
        }
        this->m_iShmId = id;
    }
    catch(exception &e)
    {
        GR_LOGE("alloc share memory got exception:%s", e.what());
        return GR_ERROR;
    }
    return GR_OK;
}

int GR_Shm::ShmAt()
{
    try
    {
        // 绑定共享内存
        this->m_szAddr = (char*)shmat(this->m_iShmId, NULL, 0);
        if (this->m_szAddr == (void*)-1)
        {
            GR_LOGE("shmat failed %d", this->m_iShmId);
            return GR_ERROR;
        }
    }
    catch(exception &e)
    {
        GR_LOGE("shmat got exception:%s", e.what());
        return GR_ERROR;
    }
    return GR_OK;
}

void GR_Shm::Free()
{
    try
    {
        // 打上删除标记
        if (shmctl(this->m_iShmId, IPC_RMID, NULL) == -1)
        {
            GR_LOGE("shmctl faild %d", this->m_iShmId);
            return;
        }
        // 从进程中分离出去
        if (shmdt(this->m_szAddr) == -1)
        {
            GR_LOGE("shmdt failed %p", this->m_szAddr);
        }
    }
    catch(exception &e)
    {
        GR_LOGE("shmat got exception:%s", e.what());
        return;
    }
    return;
}

