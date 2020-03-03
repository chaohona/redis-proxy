#include "twemroute.h"
#include "include.h"

#include <algorithm>
#include <string>
#include <yaml-cpp/yaml.h>
#include <iostream>


GR_TwemRoute::GR_TwemRoute()
{
}

GR_TwemRoute::~GR_TwemRoute()
{
}

int GR_TwemRoute::Init(GR_Config *pConfig)
{
    ASSERT(pConfig!=nullptr);
    const char *szCfgPath = pConfig->m_strTwemCfg.c_str();
    try
    {
        this->m_vServers = new GR_RedisServer*[MAX_REDIS_GROUP];

        YAML::Node node = YAML::LoadFile(szCfgPath);

        auto server = node.begin();
        if (server == node.end())
        {
            GR_LOGE("the file is empty:%s", szCfgPath);
            return GR_ERROR;
        }
        GR_LOGI("begin to parse redis twemproxy cluster:%s", server->first.as<std::string>().c_str());
        return this->ConfigParse(server->second);
    }
    catch(exception &e)
    {
        cout << "parse config file:" << szCfgPath <<", got exception:" << e.what() << endl;
        return GR_ERROR;
    }
    return GR_OK;
}

bool GR_TwemRoute::GetListenInfo(string &strIP, uint16 &uiPort, int &iBackLog)
{
    strIP = this->m_Listen.strName;
    uiPort = this->m_Listen.iPort;
    iBackLog = 1024;
    return true;
}

int GR_TwemRoute::DelRedis(GR_RedisEvent *pEvent)
{
    return GR_OK;
}

int GR_TwemRoute::ConfigParse(YAML::Node& node)
{
    if (node["listen"])
    {
        this->m_strListen = node["listen"].as<string>();
    }
    if (node["hash"])
    {
        this->m_strHash = node["hash"].as<string>();
        transform(this->m_strHash.begin(),this->m_strHash.end(),this->m_strHash.begin(),::tolower);
    }
    if (node["hash_tag"])
    {
        this->m_bHasHashTag = true;
        this->m_strHashTag = node["hash_tag"].as<string>();
        transform(this->m_strHashTag.begin(),this->m_strHashTag.end(),this->m_strHashTag.begin(),::tolower);
    }
    if (node["distribution"])
    {
        this->m_strDist = node["distribution"].as<string>();
        transform(this->m_strDist.begin(),this->m_strDist.end(),this->m_strDist.begin(),::tolower);
    }
    if (node["redis"])
    {
        this->m_bRedis = node["redis"].as<bool>();
    }
    if (node["auto_eject_hosts"])
    {
        this->m_bAutoEject = node["auto_eject_hosts"].as<bool>();
    }
    if (node["use_sentinel"])
    {
        this->m_bSentinel = node["use_sentinel"].as<bool>();
    }
    if (!node["servers"])
    {
        GR_LOGE("there is not redis info should be configed in servers");
        return GR_ERROR;
    }

    // 解析hash函数
    if (this->m_strHash == "md5")
    {
        this->m_FuncHash = GR_Hash::Md5;
    }
    else if (this->m_strHash == "crc16")
    {
        this->m_FuncHash = GR_Hash::Crc16;
    }
    else if (this->m_strHash == "crc32")
    {
        this->m_FuncHash = GR_Hash::Crc32;
    }
    else if (this->m_strHash == "crc32a")
    {
        this->m_FuncHash = GR_Hash::Crc32a;
    }
    else if (this->m_strHash == "fnv1_64")
    {
        this->m_FuncHash = GR_Hash::Fnv164;
    }
    else if (this->m_strHash == "fnv1a_64")
    {
        this->m_FuncHash = GR_Hash::Fnv1a64;
    }
    else if (this->m_strHash == "fnv1_32")
    {
        this->m_FuncHash = GR_Hash::Fnv132;
    }
    else if (this->m_strHash == "fnv1a_32")
    {
        this->m_FuncHash = GR_Hash::Fnv1a32;
    }
    else if (this->m_strHash == "hsieh")
    {
        this->m_FuncHash = GR_Hash::Hsieh;
    }
    else if (this->m_strHash == "murmur")
    {
        this->m_FuncHash = GR_Hash::Murmur;
    }
    else if (this->m_strHash == "jenkins")
    {
        this->m_FuncHash = GR_Hash::Jenkins;
    }
    else
    {
        GR_LOGE("ivalid hash:%s", this->m_strHash.c_str());
        return GR_ERROR;
    }

    if (this->m_bHasHashTag)
    {
        if (this->m_strHashTag.length() != 2)
        {
            GR_LOGE("invalid hash_tag:%s", this->m_strHashTag.c_str());
            return GR_ERROR;
        }
        this->m_cLeftTag = this->m_strHashTag[0];
        this->m_cRightTag = this->m_strHashTag[1];
    }

    // 解析监听地址
    if (GR_OK !=  this->ParseListen())
    {
        GR_LOGE("parse listen address failed:%s", this->m_strListen.c_str());
        return GR_ERROR;
    }
    
    // 解析redis地址
    if (this->m_bSentinel)
    {
        if (GR_OK != this->ParseYamlSentinelInfo(node["sentinel_info"]))
        {
            GR_LOGE("parse sentinel info failed.");
            return GR_ERROR;
        }
    }
    else if (GR_OK != this->ParseYamlRedisInfo(node["servers"]))
    {
        GR_LOGE("parse redis info failed");
        return GR_ERROR;
    }
    // 解析分配配置
    if (this->m_strDist == "ketama")
    {
        this->m_FuncDispatch = GR_Dispatch::Ketama;
        if (GR_OK != GR_Dispatch::KetamaUpdate(this->m_vServers, this->m_vContinuum, this->m_iSrvNum, this->ncontinuum))
        {
            GR_LOGE("ketama update failed");
            return GR_ERROR;
        }
    }
    else if(this->m_strDist == "modula")
    {
        this->m_FuncDispatch = GR_Dispatch::Modula;
        if (GR_OK != GR_Dispatch::ModulaUpdate(this->m_vServers, this->m_vContinuum,this->m_iSrvNum, this->ncontinuum))
        {
            GR_LOGE("mudula update failed");
            return GR_ERROR;
        }
    }
    else if (this->m_strDist == "random")
    {
        this->m_FuncDispatch = GR_Dispatch::Random;
    }
    else
    {
        GR_LOGE("invalid distribution:%s", this->m_strDist.c_str());
        return GR_ERROR;
    }
    return GR_OK;
}

int GR_TwemRoute::ParseListen()
{
    auto results = split(this->m_strListen, ":");
    if (results.size() != 2)
    {
        return GR_ERROR;
    }
    this->m_Listen.strName = results[0];
    this->m_Listen.iPort = stoi(results[1], 0, 10);
    return GR_OK;
}

int GR_TwemRoute::AddSentinel(string &&strSentinelInfo)
{
    if (strSentinelInfo == "")
    {
        GR_LOGE("invalid sentinel address %s", strSentinelInfo.c_str());
        return GR_ERROR;
    }

    auto  results = split(strSentinelInfo, ":");
    if (results.size() != 2)
    {
        GR_LOGE("invalid sentinel address %s", strSentinelInfo.c_str());
        return GR_ERROR;
    }

    GR_RedisServer *pServer = new GR_RedisServer();
    pServer->strAddr = strSentinelInfo;
    pServer->strHostname = results[0];
    pServer->iPort = stoi(results[1], 0, 10);
    this->m_listSentinels.push_back(pServer);
    return GR_OK;
}

int GR_TwemRoute::AddSentinelRedis(string &&strRedis)
{
    if (this->m_iSrvNum+1 == MAX_REDIS_GROUP)
    {
        GR_LOGE("too many redis configed");
        return GR_ERROR;
    }

    if (strRedis.length() > 130)
    {
        GR_LOGE("redis alis is too long, should no longer than 128.");
        return GR_ERROR;
    }
    auto  results = split(strRedis, ":");
    if (results.size() != 2)
    {
        GR_LOGE("invalid sentinel_info.servers %s", strRedis.c_str());
        return GR_ERROR;
    }
    GR_RedisServer *pServer = new GR_RedisServer();
    this->m_vServers[this->m_iSrvNum] = pServer;
    this->m_iSrvNum+=1;
    pServer->strSentinelName = results[0];
    if (pServer->strSentinelName == "")
    {
        GR_LOGE("invalid sentinel_info.servers %s", strRedis.c_str());
        return GR_ERROR;
    }
    pServer->iWeight = stoi(results[1], 0, 10);
    return GR_OK;
}

int GR_TwemRoute::ParseYamlSentinelInfo(YAML::Node &&node)
{
    if (!node["sentinels"])
    {
        GR_LOGE("sentinels not configed.");
        return GR_ERROR;
    }

    auto sentinels = node["sentinels"];
    if (sentinels.size() == 0)
    {
        GR_LOGE("sentinels not configed.");
        return GR_ERROR;
    }
    for (int i=0; i<sentinels.size(); i++)
    {
        if (GR_OK != this->AddSentinel(sentinels[i].as<string>()))
        {
            GR_LOGE("parse sentinels info failed.");
            return GR_ERROR;
        }
    }

    auto servers = node["servers"];
    if (servers.size() == 0)
    {
        GR_LOGE("sentinel_info.servers not configed.");
        return GR_ERROR;
    }
    for(int i=0; i<servers.size(); i++)
    {
        if (GR_OK != this->AddSentinelRedis(servers[i].as<string>()))
        {
            GR_LOGE("parse sentinel_info.servers failed.");
            return GR_ERROR;
        }
    }
    
    return GR_OK;
}

int GR_TwemRoute::ParseYamlRedisInfo(YAML::Node &&node)
{
    if (node.size() == 0)
    {
        GR_LOGE("redis server list is empty");
        return GR_ERROR;
    }
    for(int i=0; i<node.size(); i++)
    {
        string redisInfo = node[i].as<string>();
        if (GR_OK != this->AddYamlRedis(redisInfo))
        {
            GR_LOGE("parse redis info failed:%s", redisInfo.c_str());
            return GR_ERROR;
        }
    }
    
    return GR_OK;
}

int GR_TwemRoute::AddYamlRedis(string &strRedisInfo)
{
    int iResult = GR_OK;
    GR_RedisServer *pServer = this->AddRedis(iResult);
    if (pServer == nullptr || iResult != GR_OK)
    {
        GR_LOGE("too many redis configed");
        return GR_ERROR;
    }

    auto results = split(strRedisInfo, " ");
    if (results.size() == 0)
    {
        GR_LOGE("redis server is invalid:%s", strRedisInfo.c_str());
        return GR_ERROR;
    }
    pServer->strInfo = strRedisInfo;
    pServer->strPName = results[0];
    if (results.size() > 1)
    {
        pServer->strName = results[1];
    }
    results = split(pServer->strPName, ":");
    if (results.size() != 3)
    {
        GR_LOGE("redis server is invalid:%s", strRedisInfo.c_str());
        return GR_ERROR;
    }
    pServer->strAddr = results[0]+":"+results[1];
    pServer->strHostname = results[0];
    pServer->iPort = stoi(results[1], 0, 10);
    pServer->iWeight = stoi(results[2], 0, 10);
    return GR_OK;
}

#define TWEM_HASH_KEY(TMPKEY, TMPLEN)\
if (this->m_bHasHashTag) \
{\
    char *szTagStart = nullptr;\
    char *szTagEnd = nullptr;\
    szTagStart = gr_strchr(szTmpKey, szTmpKey+iTmpLen, this->m_cLeftTag);\
    if (szTagStart != nullptr)\
    {\
        szTagEnd = gr_strchr(szTagStart+1, szTmpKey+iTmpLen, this->m_cRightTag);\
        if (szTagEnd != nullptr && szTagEnd - szTagStart > 1)\
        {\
            szTmpKey = szTagStart + 1;\
            iTmpLen = szTagEnd - szTmpKey;\
        }\
    }\
}\
uiKey = this->m_FuncHash(szTmpKey, iTmpLen);


GR_RedisEvent* GR_TwemRoute::Route(char *szKey, int iLen, int &iError)
{
    iError = GR_OK;
    uint32 uiKey = 0;
    char *szTmpKey = szKey;
    int iTmpLen = iLen;
    TWEM_HASH_KEY(szTmpKey, iTmpLen);
    return this->m_FuncDispatch(this->m_vServers, this->m_vContinuum, this->ncontinuum, uiKey);
}

GR_RedisEvent* GR_TwemRoute::Route(GR_MsgIdenty *pIdenty, GR_MemPoolData  *pData, GR_RedisMsg &msg, int &iError)
{
    iError = GR_OK;
    uint32 uiKey = 0;
    char *szTmpKey = msg.m_Info.szKeyStart;
    int iTmpLen = msg.m_Info.iKeyLen;
    if (this->m_bHasHashTag) 
    {
        char *szTagStart = nullptr;
        char *szTagEnd = nullptr;
        szTagStart = gr_strchr(szTmpKey, szTmpKey+iTmpLen, this->m_cLeftTag);
        if (szTagStart != nullptr)
        {
            szTagEnd = gr_strchr(szTagStart+1, szTmpKey+iTmpLen, this->m_cRightTag);
            if (szTagEnd != nullptr && szTagEnd - szTagStart > 1)
            {
                szTmpKey = szTagStart + 1;
                iTmpLen = szTagEnd - szTmpKey;
            }
        }
    }
    uiKey = this->m_FuncHash(szTmpKey, iTmpLen);
    return this->m_FuncDispatch(this->m_vServers, this->m_vContinuum, this->ncontinuum, uiKey);
}


