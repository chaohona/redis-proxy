#ifndef _GR_SIGNAL_H__
#define _GR_SIGNAL_H__

struct GR_Signal {
    int  signo;
    char *signame;
    int  flags;
    void (*handler)(int signo);
};

// 信号标记
struct GR_SignalFlag
{
public:
    void Reset();
    int ChildExit;  // 有子进程退出
    int NewChild;   // 起一个新的子进程
    int Expansion;  // 扩容标记
    int Exit;       // 进程退出
};

// 全局信号标记
extern GR_SignalFlag   gr_global_signal;

void
GR_SignalHandler(int signo);
int 
GR_SignalInit();

int SetSysTimer(int iMS);


#endif
