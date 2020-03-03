#include <stdio.h>
#include <stdlib.h>
#include <hiredis/hiredis.h>	//会使用到so的动态库
#include <string.h>
#include <stdbool.h>
//#include "crc64.h"		//在reids解压目录下的src目录下
 
#define  hash_slot 16384
 
 
//redis结构体，存储当前连接reids的信息
struct redis
{
    redisContext *conn;
    redisReply *reply;
    char mip[10];			//IP
    int mport;				//端口
};
 
//全局变量
struct redis gredis;
 
//获取变量
struct redis* redis_malloc()
{
    gredis.conn=NULL;
    gredis.reply=NULL;
    memset(gredis.mip,0,sizeof(gredis.mip));
    gredis.mport=0;
    return &gredis;
}
 
//初始化
bool redis_init(struct redis *predis,char* IP,int PORT)
{
    if(predis!= &gredis)
	return false;
	if(predis->conn!=NULL || predis->reply != NULL)
    {
        redisFree(predis->conn);
    }
    predis->mport=PORT;
    strcpy(predis->mip,IP);
    predis->conn = redisConnect(IP,PORT);
    if(predis->conn->err)
    {
        printf("redisConnect error\n");
        return false;
    }
    return true;
}
 
//写入reids，如果写入redis与当前连接的redis不对应，切换
bool redis_write(struct redis *predis,char* key,char *value)
{
    char ip [20]="";
    int port =0;
    char text [10]="";
    char cmd[1024]="";
    sprintf(cmd,"set %s %s",key,value);
    predis->reply =redisCommand(predis->conn,cmd);
	//如果key与当前连接reids不对应，切换
    if(strcmp(predis->reply->str,"OK")!=0)
    {
        printf("%s\n",predis->reply->str);
		//截取IP和端口
        sscanf(predis->reply->str,"%s %s %[0-9.]:%d",text,text,ip,&port);
		//释放，释放之后数据会清空，所以截取要在释放前
		freeReplyObject(predis->reply);
        if(!redis_init(predis,ip,port))
            return false;
		//再次写入
		predis->reply =redisCommand(predis->conn,cmd);
    }
    freeReplyObject(predis->reply);
    return true;
}
 
//断开连接
bool redis_clear(struct  redis *predis)
{
     if(predis!= &gredis)
        return false;
     redisFree(predis->conn);
}
 
//显示当前连接的redis的端口和IP
void redis_show(struct redis *predis)
{
    printf("****IP:%s Port:%d****\n",predis->mip,predis->mport);
}
 
//返回对应的槽值，使用了crc16.c文件，在redis解压文件src文件夹下
unsigned int get16crc(unsigned char *key)
{
    unsigned int gcrc =crc16(key,strlen(key));
    return (gcrc%hash_slot);
}
 
 
int main()
{
	//获取全局变量
    struct redis* predis=redis_malloc();
    printf("crc16: %d\n",get16crc("1314"));
 
	//任意连接一台
    if(false==redis_init(predis,"127.0.0.1",7001))
    {
        printf("redis_init error!!!!!\n");
        return false;
    }
    int a=1;
    char key[100]="";
    char val[100]="";
	
    for(a=0;a<10;a++)
    {
        memset(key,0,sizeof(key));
        memset(val,0,sizeof(val));
        sprintf(key,"%d",a);
        sprintf(val,"%d%d",a,a);
        redis_write(predis,key,val);
        printf("@@%d\n",a);
    }
 
    redis_show(predis);
 
    redis_clear(predis);
 
    printf("EOP!\n");
    return 0;
}
