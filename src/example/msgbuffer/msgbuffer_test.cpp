#include "include.h"
#include "msgbuffer.h"

int main()
{
    GR_RingMsgBufferMgr buffer;
    buffer.Init(512);

    char *szSrc = "0123456789";
    for(int i=0; i<51; i++)
    {
        buffer.Write(szSrc, 10);
    }
    return 0;
}
