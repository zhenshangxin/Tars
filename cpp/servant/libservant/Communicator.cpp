/**
 * Tencent is pleased to support the open source community by making Tars available.
 *
 * Copyright (C) 2016THL A29 Limited, a Tencent company. All rights reserved.
 *
 * Licensed under the BSD 3-Clause License (the "License"); you may not use this file except 
 * in compliance with the License. You may obtain a copy of the License at
 *
 * https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing, software distributed 
 * under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR 
 * CONDITIONS OF ANY KIND, either express or implied. See the License for the 
 * specific language governing permissions and limitations under the License.
 */

#include "util/tc_file.h"
#include "servant/Communicator.h"
#include "servant/StatReport.h"
#include "servant/TarsLogger.h"

namespace tars
{

//////////////////////////////////////////////////////////////////////////////////////////////

string ClientConfig::LocalIp = "127.0.0.1";

string ClientConfig::ModuleName = "unknown";

set<string> ClientConfig::SetLocalIp;

bool ClientConfig::SetOpen = false;

string ClientConfig::SetDivision = "";

string ClientConfig::TarsVersion = string(TARS_VERSION);

//////////////////////////////////////////////////////////////////////////////////////////////

Communicator::Communicator()
: _initialized(false)
, _terminating(false)
, _clientThreadNum(1)
, _statReport(NULL)
, _timeoutLogFlag(true)
, _minTimeout(100)
{
    memset(_communicatorEpoll,0,sizeof(_communicatorEpoll));
}

// 根据配置来构造
Communicator::Communicator(TC_Config& conf, const string& domain/* = CONFIG_ROOT_PATH*/)
: _initialized(false)
, _terminating(false)
, _timeoutLogFlag(true)
{
    setProperty(conf, domain);
}

Communicator::~Communicator()
{
    terminate();
}

bool Communicator::isTerminating()
{
    return _terminating;
}

void Communicator::setProperty(TC_Config& conf, const string& domain/* = CONFIG_ROOT_PATH*/)
{
    TC_LockT<TC_ThreadRecMutex> lock(*this);

    // 获取域下面的参数值对 放到_properties中去 CONFIG_ROOT_PATH默认为/tars/application/client
    // 配置文件中的所有配置都在此读取
    conf.getDomainMap(domain, _properties);

    // 如果application域下面有 enableset 或 setdivision的值为默认值（或没有值）
    string defaultValue = "dft";
    if ((defaultValue == getProperty("enableset", defaultValue))
            || (defaultValue == getProperty("setdivision", defaultValue)))
    {
        // 从配置文件中获取
        _properties["enableset"] = conf.get("/tars/application<enableset>", "n");
        _properties["setdivision"] = conf.get("/tars/application<setdivision>", "NULL");
    }

    // 由_properties初始化Communicator的配置 此部分包含IP 进程名等
    initClientConfig();
}

void Communicator::initClientConfig()
{
    // 判断是否打开了set 默认为否
    ClientConfig::SetOpen = TC_Common::lower(getProperty("enableset", "n"))=="y"?true:false;

    if (ClientConfig::SetOpen)
    {
        // 客户端set分组
        ClientConfig::SetDivision = getProperty("setdivision");

        vector<string> vtSetDivisions = TC_Common::sepstr<string>(ClientConfig::SetDivision,".");

        string sWildCard = "*";

        if (vtSetDivisions.size()!=3
            || vtSetDivisions[0]==sWildCard
            || vtSetDivisions[1]==sWildCard)
        {
            //set分组名不对时默认没有打开set分组
            ClientConfig::SetOpen = false;
            setProperty("enableset","n");
            TLOGERROR( "[TARS][set division name error:" << ClientConfig::SetDivision << ", client failed to open set]" << endl);
        }
    }
    // 本地IP 默认为空
    ClientConfig::LocalIp = getProperty("localip", "");
    if (ClientConfig::SetLocalIp.empty())
    {
        // 若为空 获取本地所有IP
        vector<string> v = TC_Socket::getLocalHosts();
        for (size_t i = 0; i < v.size(); i++)
        {
            // 设置为第一个遇到的不是127.0.0.1的IP
            if (v[i] != "127.0.0.1" && ClientConfig::LocalIp.empty())
            {
                ClientConfig::LocalIp = v[i];
            }
            ClientConfig::SetLocalIp.insert(v[i]);
        }
    }

    //缺省采用进程名称
    string exe = "";

    try
    {
        exe = TC_File::extractFileName(TC_File::getExePath());
    }
    catch(TC_File_Exception & ex)
    {
        //取失败则使用ip代替进程名
        exe = ClientConfig::LocalIp;
    }

    // 客户端模块的名称 默认为进程名
    ClientConfig::ModuleName = getProperty("modulename", exe);
}

void Communicator::setProperty(const map<string, string>& properties)
{
    TC_LockT<TC_ThreadRecMutex> lock(*this);

    _properties = properties;

    initClientConfig();
}

void Communicator::setProperty(const string& name, const string& value)
{
    TC_LockT<TC_ThreadRecMutex> lock(*this);

    _properties[name] = value;

    initClientConfig();
}

string Communicator::getProperty(const string& name, const string& dft/* = ""*/)
{
    TC_LockT<TC_ThreadRecMutex> lock(*this);

    map<string, string>::iterator it = _properties.find(name);

    if (it != _properties.end())
    {
        return it->second;
    }
    return dft;
}

void Communicator::reloadLocator()
{
    for (size_t i = 0; i < _clientThreadNum; ++i)
    {
        _communicatorEpoll[i]->getObjectProxyFactory()->loadObjectLocator();
    }
}

    // 重新加载属性
int Communicator::reloadProperty(string & sResult)
{
    // 第一次调用时还未初始化 _clientThreadNum为0
    for(size_t i = 0; i < _clientThreadNum; ++i)
    {
        _communicatorEpoll[i]->getObjectProxyFactory()->loadObjectLocator();
    }

    int iReportInterval = TC_Common::strto<int>(getProperty("report-interval", "60000"));

    int iReportTimeout = TC_Common::strto<int>(getProperty("report-timeout", "5000"));

    int iSampleRate = TC_Common::strto<int>(getProperty("sample-rate", "1000"));

    int iMaxSampleCount = TC_Common::strto<int>(getProperty("max-sample-count", "100"));

    int iMaxReportSize = TC_Common::strto<int>(getProperty("max-report-size", "1400"));
    // 数据统计的地址 见Introduction.md
    string statObj = getProperty("stat", "");
    // 属性统计的地址
    string propertyObj = getProperty("property", "");

    // 数据统计代理
    StatFPrx statPrx = NULL;

    if (!statObj.empty())
    {
        statPrx = stringToProxy<StatFPrx>(statObj);
    }

    // 属性统计代理
    PropertyFPrx propertyPrx = NULL;

    if (!propertyObj.empty())
    {
        propertyPrx = stringToProxy<PropertyFPrx>(propertyObj);
    }

    string sSetDivision = ClientConfig::SetOpen?ClientConfig::SetDivision:"";
    _statReport->setReportInfo(statPrx, propertyPrx, ClientConfig::ModuleName, ClientConfig::LocalIp, sSetDivision, iReportInterval, iSampleRate, iMaxSampleCount, iMaxReportSize, iReportTimeout);

    // 结果
    sResult = "locator=" + getProperty("locator", "") + "\r\n" +
        "stat=" + statObj + "\r\n" + "property=" + propertyObj + "\r\n" +
        "SetDivision=" + sSetDivision + "\r\n" +
        "report-interval=" + TC_Common::tostr(iReportInterval) + "\r\n" +
        "report-timeout=" + TC_Common::tostr(iReportTimeout) + "\r\n" +
        "sample-rate=" + TC_Common::tostr(iSampleRate) + "\r\n" +
        "max-sample-count=" + TC_Common::tostr(iMaxSampleCount) + "\r\n";

    return 0;
}

void Communicator::initialize()
{
    TC_LockT<TC_ThreadRecMutex> lock(*this);

    // 若初始化过了 直接返回
    if (_initialized)
        return;

    _initialized = true;

    _servantProxyFactory = new ServantProxyFactory(this);


    //客户端网络线程 默认为1 最大线程数为64
    _clientThreadNum = TC_Common::strto<size_t>(getProperty("netthread","1"));
    if(0 == _clientThreadNum)
    {
        _clientThreadNum = 1;
    }
    else if(MAX_CLIENT_THREAD_NUM < _clientThreadNum)
    {
        _clientThreadNum = MAX_CLIENT_THREAD_NUM;
    }

    // 上报类 stat总是有对象, 保证getStat返回的对象总是有效
    _statReport = new StatReport(_clientThreadNum);

    for(size_t i = 0; i < _clientThreadNum; ++i)
    {
        // 根据线程数创建多个_communicatorEpoll 客户端网络处理的线程类
        _communicatorEpoll[i] = new CommunicatorEpoll(this, i);
        // start为tc_thread中的start 实际执行的是子类的run方法
        _communicatorEpoll[i]->start();
    }

    //初始化统计上报接口
    string statObj = getProperty("stat", "");

    string propertyObj = getProperty("property", "");

    string moduleName = getProperty("modulename", "");

    int iReportInterval = TC_Common::strto<int>(getProperty("report-interval", "60000"));

    int iReportTimeout = TC_Common::strto<int>(getProperty("report-timeout", "5000"));

    int iSampleRate = TC_Common::strto<int>(getProperty("sample-rate", "1000"));

    int iMaxSampleCount = TC_Common::strto<int>(getProperty("max-sample-count", "100"));

    int iMaxReportSize = TC_Common::strto<int>(getProperty("max-report-size", "1400"));

    _timeoutLogFlag = TC_Common::strto<bool>(getProperty("timeout-log-flag", "1"));

    _minTimeout = TC_Common::strto<int64_t>(getProperty("min-timeout", "100"));
    if(_minTimeout < 1)
        _minTimeout = 1;

    // 数据统计服务
    StatFPrx statPrx = NULL;
    if (!statObj.empty())
    {
        statPrx = stringToProxy<StatFPrx>(statObj);
    }

    //属性统计服务
    PropertyFPrx propertyPrx = NULL;
    if (!propertyObj.empty())
    {
        propertyPrx = stringToProxy<PropertyFPrx>(propertyObj);
    }

    string sSetDivision = ClientConfig::SetOpen?ClientConfig::SetDivision:"";
    _statReport->setReportInfo(statPrx, propertyPrx, ClientConfig::ModuleName, ClientConfig::LocalIp, sSetDivision, iReportInterval, iSampleRate, iMaxSampleCount, iMaxReportSize, iReportTimeout);
}


vector<TC_Endpoint> Communicator::getEndpoint(const string & objName)
{
    ServantProxy * pServantProxy = getServantProxy(objName);
    return pServantProxy->getEndpoint();
}

vector<TC_Endpoint> Communicator::getEndpoint4All(const string & objName)
{
    ServantProxy * pServantProxy = getServantProxy(objName);
    return pServantProxy->getEndpoint4All();
}

void Communicator::terminate()
{
    {
        TC_LockT<TC_ThreadRecMutex> lock(*this);

        _terminating = true;
    }

    if(_initialized)
    {
        for(size_t i = 0; i < _clientThreadNum; ++i)
        {
            _communicatorEpoll[i]->terminate();
            _communicatorEpoll[i]->getThreadControl().join();
            //delete _communicatorEpoll[i];
            //_communicatorEpoll[i] = NULL;
        }

        if(_statReport)
        {
            if (_statReport->isAlive())
            {
                _statReport->terminate();
                _statReport->getThreadControl().join();
            }
            delete _statReport;
            _statReport = NULL;
        }
    }

}
    // 获取通用的ServantProxy对象
ServantProxy * Communicator::getServantProxy(const string& objectName,const string& setName)
{
    // 在第一次调用getServantProxy时需要初始化
    Communicator::initialize();

    return _servantProxyFactory->getServantProxy(objectName,setName);
}

StatReport * Communicator::getStatReport()
{
    Communicator::initialize();

    return _statReport;
}

ServantProxyFactory* Communicator::servantProxyFactory()
{
    return _servantProxyFactory.get();
}
///////////////////////////////////////////////////////////////
}
