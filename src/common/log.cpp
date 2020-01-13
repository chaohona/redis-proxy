#include "log.h"
#include "utils.h"
#include "define.h"
#include "gr_proxy_global.h"
#include "config.h"

#include <stdio.h>

GR_Log *GR_Log::m_pInstance = new GR_Log();
GR_LOG_LEVEL GR_Log::m_iLogLevel = GR_LOG_LEVEL_NONE;


GR_Log::GR_Log()
{
}

GR_Log::~GR_Log()
{
    //google::ShutdownGoogleLogging();
}

void GR_Log::SetLogLevel(GR_LOG_LEVEL logLevel)
{
    m_iLogLevel = logLevel;
}

GR_Log* GR_Log::Instance()
{
    return m_pInstance;
}

bool GR_Log::Init(const char *szPath, char *szName)
{
    //google::InitGoogleLogging(szName);
    //google::SetLogDestination(google::INFO, szPath);
    //google::SetLogDestination(google::WARNING, szPath);
    //google::SetLogDestination(google::ERROR, szPath);
    return true;
}

// 需要修改glog增加一个debug级别的日志
void GR_Log::Debug(const char *szFile, int iLine, const char * szArgs, ...)
{
    int len, size, errno_save;
    char buf[4 * LOG_MAX_LEN];
    va_list args;
    ssize_t n;

    errno_save = errno;
    len = 0;                /* length of output buffer */
    size = 4 * LOG_MAX_LEN; /* size of output buffer */

    uint64 ulNow = CURRENT_MS();
    va_start(args, szArgs);
    len += snprintf(buf+len, size-len, "%d.%ld %s:%d] ", gr_work_pid, ulNow, szFile, iLine);
    len += GR_VscnPrintf(buf+len, size-len, szArgs, args);
    va_end(args);

    buf[len++] = '\n';
    buf[len] = '\0';

    GR_Write(STDERR_FILENO, buf, len);
}

void GR_Log::Info(const char *szFile, int iLine, const char * szArgs, ...)
{
    int len, size, errno_save;
    char buf[4 * LOG_MAX_LEN];
    va_list args;
    ssize_t n;

    errno_save = errno;
    len = 0;                /* length of output buffer */
    size = 4 * LOG_MAX_LEN; /* size of output buffer */

    uint64 ulNow = CURRENT_MS();
    va_start(args, szArgs);
    len += snprintf(buf+len, size-len, "%d.%ld %s:%d] ", gr_work_pid, ulNow, szFile, iLine);
    len += GR_VscnPrintf(buf+len, size-len, szArgs, args);
    va_end(args);

    buf[len++] = '\n';
    buf[len] = '\0';

    GR_Write(STDERR_FILENO, buf, len);
}

void GR_Log::Error(const char *szFile, int iLine, const char *szArgs, ...)
{
    int len, size, errno_save;
    char buf[4 * LOG_MAX_LEN];
    va_list args;
    ssize_t n;

    errno_save = errno;
    len = 0;                /* length of output buffer */
    size = 4 * LOG_MAX_LEN; /* size of output buffer */

    uint64 ulNow = CURRENT_MS();
    va_start(args, szArgs);
    len += snprintf(buf+len, size-len, "%d.%ld %s:%d] ", gr_work_pid, ulNow, szFile, iLine);
    len += GR_VscnPrintf(buf+len, size-len, szArgs, args);
    va_end(args);

    buf[len++] = '\n';
    buf[len] = '\0';

    GR_Write(STDERR_FILENO, buf, len);
}

void GR_Log::StdErr(const char *szFile, int iLine, const char *szArgs, ...)
{
    int len, size, errno_save;
    char buf[4 * LOG_MAX_LEN];
    va_list args;
    ssize_t n;

    errno_save = errno;
    len = 0;                /* length of output buffer */
    size = 4 * LOG_MAX_LEN; /* size of output buffer */

    uint64 ulNow = CURRENT_MS();
    va_start(args, szArgs);
    len += snprintf(buf+len, size-len, "%d.%ld %s:%d] ", gr_work_pid, ulNow, szFile, iLine);
    len += GR_VscnPrintf(buf+len, size-len, szArgs, args);
    va_end(args);

    buf[len++] = '\n';
    buf[len] = '\0';

    GR_Write(STDERR_FILENO, buf, len);

    return;
}


