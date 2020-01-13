#ifndef _GR_OPTIONS_H__
#define _GR_OPTIONS_H__
#include "define.h"
#include <getopt.h>

static struct option long_options[] = {
    { "help",           no_argument,        NULL,   'h' },
    { "version",        no_argument,        NULL,   'V' },
    { "daemonize",      no_argument,        NULL,   'd' },
    { "config",         required_argument,  NULL,   'c' },
    { "pid-file",       required_argument,  NULL,   'p' }, // pid
    { "addr",           required_argument,  NULL,   'a' }, // 监听地址
    { "works",          required_argument,  NULL,   'w' }, // 工作子进程个数
    { NULL,             0,                  NULL,    0  }
};

static char short_options[] = "hVd:c:p:a:";

class GR_Options
{
public:
    static GR_Options* Instance();
public:
    int showHelp = 0;
    int showVersion = 0;
    int daemonize = 0;
    int works   = 1;
    char *pidFile = nullptr;

    char *confFileName = "./conf/gredis.yml";
private:
    GR_Options();
    static GR_Options *m_pInstance;
};

#endif
