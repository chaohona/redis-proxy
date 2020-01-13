#include "redismsg.h"
#include "define.h"
#include "include.h"
#include <iostream>

using namespace std;

int main()
{
    GR_RedisMsg redisMsg;
    char *szMsg = "*200\r\n*6\r\n$4\r\nkeys\r\n:2\r\n*2\r\n+readonly\r\n+sort_for_script\r\n:0\r\n:0\r\n:0\r\n*6\r\n$4\r\necho\r\n:2\r\n*1\r\n+fast\r\n:0\r\n:0\r\n:0\r\n*6\r\n$9\r\nreplicaof\r\n:3\r\n*3\r\n+admin\r\n+noscript\r\n+stale\r\n:0\r\n:0\r\n:0\r\n";

    redisMsg.Init(szMsg);
    redisMsg.Expand(strlen(szMsg));
    int iRet = redisMsg.ParseRsp();
    cout << "parse result is:" << iRet << endl;
    return 0;
}
