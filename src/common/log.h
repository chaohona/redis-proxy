#ifndef _GR_LOG_H__
#define _GR_LOG_H__

#include <glog/logging.h>
#include "define.h"

#define LOG_MAX_LEN 256 /* max length of log message */

#define GR_LOG_INIT(PATH, NAME)\
GR_Log::Init(PATH, NAME)

#define GR_LOGE(...)\
if (GR_Log::m_iLogLevel>=GR_LOG_LEVEL_ERROR)\
GR_Log::Error(__FILE__, __LINE__, __VA_ARGS__)

#define GR_LOGI(...)\
if (GR_Log::m_iLogLevel>=GR_LOG_LEVEL_INFO)\
GR_Log::Info(__FILE__, __LINE__, __VA_ARGS__)

#define GR_LOGD(...)\
if (GR_Log::m_iLogLevel>=GR_LOG_LEVEL_DEBUG)\
GR_Log::Debug(__FILE__, __LINE__, __VA_ARGS__)

#define GR_LOGW(...)\
if (GR_Log::m_iLogLevel>=GR_LOG_LEVEL_WARNING)\
GR_Log::Error(__FILE__, __LINE__, __VA_ARGS__)


#define GR_STDERR(...)\
GR_Log::StdErr(__FILE__, __LINE__, __VA_ARGS__)

class GR_Log
{
public:
    ~GR_Log();
    static bool Init(const char *szPath, char *szName);

    static void Debug(const char *szFile, int iLine, const char *szArgs, ...);
    static void Info(const char *szFile, int iLine, const char *szArgs, ...);
    static void Error(const char *szFile, int iLine, const char *szArgs, ...);
    static void StdErr(const char *szFile, int iLine, const char *szArgs, ...);
    
    static GR_Log* Instance();
    static void SetLogLevel(GR_LOG_LEVEL logLevel);


    static GR_LOG_LEVEL m_iLogLevel;

private:
    GR_Log();

    static GR_Log* m_pInstance; 
};
#endif
