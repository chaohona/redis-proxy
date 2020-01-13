#include "gr_shm.h"
#include "include.h"
#include "gr_atomic.h"
#include <iostream>

using namespace std;

int main()
{
    GR_Shm shm;
    shm.Alloc(sizeof(GR_ShmTx));
    shm.ShmAt();
    GR_ShmTx *pShTx = (GR_ShmTx *)shm.m_szAddr;
    for(int i=0; i<200; i++)
    {
        pid_t pid = fork();
        switch (pid)
        {
        case -1:
            {
                cout << "fork failed" << endl;
                break;
            }
        case 0:
            {
                /*for(int j=0; j<10000; j++)
                {
                    GR_AtomicFetchAdd(&pShTx->m_iLock, 1);
                }
                cout << "the result is:" << pShTx->m_iLock << endl;*/
                if (pShTx->TryLock(i))
                {
                    cout << "lock success:" << i << endl;
                }
                return 0;
            }
        default:
            {
            }
        }
    }

    return 0;
}
