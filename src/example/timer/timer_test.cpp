#include "gr_timer.h"
#include "utils.h"
#include <iostream>
#include <string>
#include <unistd.h>

using namespace std;

int TimerProcess(GR_TimerMeta *pMeta, void *pCB)
{
    return 0;
}

int main()
{
    return 0;
    GR_Timer::Instance()->AddTimer(TimerProcess, "12345",10, true);
    GR_Timer::Instance()->AddTimer(TimerProcess, "1234567", 10);
    uint64 ulIndex = GR_Timer::Instance()->AddTimer(TimerProcess, "123456", 10);
    //GR_Timer::Instance()->DelTimer(ulIndex);

    int iLoop = 0;
    for(;iLoop<1000;iLoop++)
    {
        usleep(100*11);
        uint64 ulNow = GR_GetNowMS();
        uint64 ulMix = 0;
        GR_Timer::Instance()->Loop(ulNow, ulMix);
        cout << "now is:" << ulNow << ", mix is:" << ulMix << endl;
    }
    return 0;
}
