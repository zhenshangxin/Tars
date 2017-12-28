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

#include <errno.h>
#include <fstream>
#include "util/tc_config.h"
#include "util/tc_common.h"

namespace tars
{

TC_ConfigDomain::TC_ConfigDomain(const string &sLine)
{
    _name = TC_Common::trim(sLine);
}

TC_ConfigDomain::~TC_ConfigDomain()
{
    destroy();
}

TC_ConfigDomain::TC_ConfigDomain(const TC_ConfigDomain &tcd)
{
    (*this) = tcd;
}

TC_ConfigDomain& TC_ConfigDomain::operator=(const TC_ConfigDomain &tcd)
{
    if(this != &tcd)
    {
        destroy();

        _name  = tcd._name;
        _param = tcd._param;
        _key   = tcd._key;
        _domain= tcd._domain;
        _line  = tcd._line;

        const map<string, TC_ConfigDomain*> & m = tcd.getDomainMap();
        map<string, TC_ConfigDomain*>::const_iterator it = m.begin();
        while(it != m.end())
        {
            _subdomain[it->first] = it->second->clone();
            ++it;
        }
    }
    return *this;
}

TC_ConfigDomain::DomainPath TC_ConfigDomain::parseDomainName(const string& path, bool bWithParam)
{
    TC_ConfigDomain::DomainPath dp;

    if(bWithParam)
    {
        string::size_type pos1 = path.find_first_of(TC_CONFIG_PARAM_BEGIN);
        if(pos1 == string::npos)
        {
            throw TC_Config_Exception("[TC_Config::parseDomainName] : param path '" + path + "' is invalid!" );
        }

        if(path[0] != TC_CONFIG_DOMAIN_SEP)
        {
            throw TC_Config_Exception("[TC_Config::parseDomainName] : param path '" + path + "' must start with '/'!" );
        }

        string::size_type pos2 = path.find_first_of(TC_CONFIG_PARAM_END);
        if(pos2 == string::npos)
        {
            throw TC_Config_Exception("[TC_Config::parseDomainName] : param path '" + path + "' is invalid!" );
        }

        dp._domains = TC_Common::sepstr<string>(path.substr(1, pos1-1), TC_Common::tostr(TC_CONFIG_DOMAIN_SEP));
        dp._param = path.substr(pos1+1, pos2 - pos1 - 1);
    }
    else
    {
//        if(path.length() <= 1 || path[0] != TC_CONFIG_DOMAIN_SEP)
        if(path[0] != TC_CONFIG_DOMAIN_SEP)
        {
            throw TC_Config_Exception("[TC_Config::parseDomainName] : param path '" + path + "' must start with '/'!" );
        }

        dp._domains = TC_Common::sepstr<string>(path.substr(1), TC_Common::tostr(TC_CONFIG_DOMAIN_SEP));
    }

    return dp;
}

TC_ConfigDomain* TC_ConfigDomain::addSubDomain(const string& name)
{
    // 若在子域中没找到这个域
    if(_subdomain.find(name) == _subdomain.end())
    {
        // 在_domain的最后插入name 记录域的插入顺序
        _domain.push_back(name);
        // 新建一个TC_ConfigDomain 插入子域的map中
        _subdomain[name] = new TC_ConfigDomain(name);
    }
    return _subdomain[name];
}

string TC_ConfigDomain::getParamValue(const string &name) const
{
    map<string, string>::const_iterator it = _param.find(name);
    if( it == _param.end())
    {
        throw TC_ConfigNoParam_Exception("[TC_ConfigDomain::getParamValue] param '" + name + "' not exits!");
    }

    return it->second;
}

TC_ConfigDomain *TC_ConfigDomain::getSubTcConfigDomain(vector<string>::const_iterator itBegin, vector<string>::const_iterator itEnd)
{
    if(itBegin == itEnd)
    {
        return this;
    }

    map<string, TC_ConfigDomain*>::const_iterator it = _subdomain.find(*itBegin);

    //根据匹配规则找不到匹配的子域
    if(it == _subdomain.end())
    {
        return NULL;
    }

    //继续在子域下搜索
    return it->second->getSubTcConfigDomain(itBegin + 1, itEnd);
}

const TC_ConfigDomain *TC_ConfigDomain::getSubTcConfigDomain(vector<string>::const_iterator itBegin, vector<string>::const_iterator itEnd) const
{
    if(itBegin == itEnd)
    {
        return this;
    }

    map<string, TC_ConfigDomain*>::const_iterator it = _subdomain.find(*itBegin);

    //根据匹配规则找不到匹配的子域
    if(it == _subdomain.end())
    {
        return NULL;
    }

    //继续在子域下搜索
    return it->second->getSubTcConfigDomain(itBegin + 1, itEnd);
}

void TC_ConfigDomain::insertParamValue(const map<string, string> &m)
{
    _param.insert(m.begin(),  m.end());

    map<string, string>::const_iterator it = m.begin();
    while(it != m.end())
    {
        size_t i = 0;
        for(; i < _key.size(); i++)
        {
            if(_key[i] == it->first)
            {
                break;
            }
        }

        //没有该key, 则添加到最后
        if(i == _key.size())
        {
            _key.push_back(it->first);
        }

        ++it;
    }
}

void TC_ConfigDomain::setParamValue(const string &name, const string &value)
{
    // 将键和值放入_param中
    _param[name] = value;

    //如果key在_key中已经存在（多次配置同一个属性） ,则删除之前的
    for(vector<string>::iterator it = _key.begin(); it != _key.end(); ++it)
    {
        if(*it == name)
        {
            _key.erase(it);
            break;
        }
    }
    // 在_key的最后插入name 用于记录配置项的插入顺序
    _key.push_back(name);
}

// 设置参数/值对
void TC_ConfigDomain::setParamValue(const string &line)
{
    // line为空 返回
    if(line.empty())
    {
        return;
    }

    // 将line 放入line中
    _line.push_back(line);

    string::size_type pos = 0;
    // 遍历这一行
    for(; pos <= line.length() - 1; pos++)
    {
        // 找到了等号
        if (line[pos] == '=')
        {
            // 若等号前的一个字符为"\" 不处理
            if(pos > 0 && line[pos-1] == '\\')
            {
                continue;
            }
            // 获取属性名
            string name  = parse(TC_Common::trim(line.substr(0, pos), " \r\n\t"));

            // 获取属性值
            string value;
            if(pos < line.length() - 1)
            {
                value = parse(TC_Common::trim(line.substr(pos + 1), " \r\n\t"));
            }
            // 设置参数 键值对
            setParamValue(name, value);
            return;
        }
    }
    // 否则键为整行 值为空
    setParamValue(line, "");
}

string TC_ConfigDomain::parse(const string& s)
{
    if(s.empty())
    {
        return "";
    }

    string param;
    string::size_type pos = 0;
    for(; pos <= s.length() - 1; pos++)
    {
        char c;
        if(s[pos] == '\\' && pos < s.length() - 1)
        {
            switch (s[pos+1])
            {
            case '\\':
                c = '\\';
                pos++;
                break;
            case 'r':
                c = '\r';
                pos++;
                break;
            case 'n':
                c = '\n';
                pos++;
                break;
            case 't':
                c = '\t';
                pos++;
                break;
            case '=':
                c = '=';
                pos++;
                break;
            default:
                throw TC_Config_Exception("[TC_ConfigDomain::parse] '" + s + "' is invalid, '" + TC_Common::tostr(s[pos]) + TC_Common::tostr(s[pos+1]) + "' couldn't be parse!" );
            }

            param += c;
        }
        else if (s[pos] == '\\')
        {
            throw TC_Config_Exception("[TC_ConfigDomain::parse] '" + s + "' is invalid, '" + TC_Common::tostr(s[pos]) + "' couldn't be parse!" );
        }
        else
        {
            param += s[pos];
        }
    }

    return param;
}

string TC_ConfigDomain::reverse_parse(const string &s)
{
    if(s.empty())
    {
        return "";
    }

    string param;
    string::size_type pos = 0;
    for(; pos <= s.length() - 1; pos++)
    {
        string c;
        switch (s[pos])
        {
        case '\\':
            param += "\\\\";
            break;
        case '\r':
            param += "\\r";
            break;
        case '\n':
            param += "\\n";
            break;
        case '\t':
            param += "\\t";
            break;
            break;
        case '=':
            param += "\\=";
            break;
        case '<':
        case '>':
            throw TC_Config_Exception("[TC_ConfigDomain::reverse_parse] '" + s + "' is invalid, couldn't be parse!" );
        default:
            param += s[pos];
        }
    }

    return param;
}

string TC_ConfigDomain::getName() const
{
    return _name;
}

void TC_ConfigDomain::setName(const string& name)
{
    _name = name;
}

vector<string> TC_ConfigDomain::getKey() const
{
    return _key;
}

vector<string> TC_ConfigDomain::getLine() const
{
    return _line;
}

vector<string> TC_ConfigDomain::getSubDomain() const
{
    return _domain;
}

void TC_ConfigDomain::destroy()
{
    _param.clear();
    _key.clear();
    _line.clear();
    _domain.clear();

    map<string, TC_ConfigDomain*>::iterator it = _subdomain.begin();
    while(it != _subdomain.end())
    {
        delete it->second;
        ++it;
    }

    _subdomain.clear();
}

string TC_ConfigDomain::tostr(int i) const
{
    string sTab;
    for(int k = 0; k < i; ++k)
    {
        sTab += "\t";
    }

    ostringstream buf;

    buf << sTab << "<" << reverse_parse(_name) << ">" << endl;;

    for(size_t n = 0; n < _key.size(); n++)
    {
        map<string, string>::const_iterator it = _param.find(_key[n]);

        assert(it != _param.end());

        //值为空, 则不打印出=
        if(it->second.empty())
        {
            buf << "\t" << sTab << reverse_parse(_key[n]) << endl;
        }
        else
        {
            buf << "\t" << sTab << reverse_parse(_key[n]) << "=" << reverse_parse(it->second) << endl;
        }
    }

    ++i;

    for(size_t n = 0; n < _domain.size(); n++)
    {
        map<string, TC_ConfigDomain*>::const_iterator itm = _subdomain.find(_domain[n]);

        assert(itm != _subdomain.end());

        buf << itm->second->tostr(i);
    }


    buf << sTab << "</" << reverse_parse(_name) << ">" << endl;

    return buf.str();
}

/********************************************************************/
/*        TC_Config implement                                            */
/********************************************************************/

TC_Config::TC_Config() : _root("")
{
}

TC_Config::TC_Config(const TC_Config &tc)
: _root(tc._root)
{

}

TC_Config& TC_Config::operator=(const TC_Config &tc)
{
    if(this != &tc)
    {
        _root = tc._root;
    }

    return *this;
}

void TC_Config::parse(istream &is)
{
    // 先清空root 根域
    _root.destroy();

    // container adaptor 先进后出 如果没有指定使用哪一种容器 那么就使用dequene（双端队列）
    stack<TC_ConfigDomain*> stkTcCnfDomain;
    // 放入root
    stkTcCnfDomain.push(&_root);

    string line;
    // 读取一行（以遇到\n为标准） 放入line中
    while(getline(is, line))
    {
        // 去掉字符串头部和尾部的每一个 \r \n \t
        line = TC_Common::trim(line, " \r\n\t");
        // 如果去掉后长度为0 则读取下一行
        if(line.length() == 0)
        {
            continue;
        }
        // 开头为# 注释 读取下一行
        if(line[0] == '#')
        {
            continue;
        }
        else if(line[0] == '<')     // < 开头 一个域的开始或结尾
        {
            // 找到对应的>
            string::size_type posl = line.find_first_of('>');
            // 没找到 报错
            if(posl == string::npos)
            {
                throw TC_Config_Exception("[TC_Config::parse]:parse error! line : " + line);
            }
            // 若第二个字符为/ 表示域的结尾
            if(line[1] == '/')
            {
                // 获取从第三个字符开始的子字符串
                string sName(line.substr(2, (posl - 2)));
                // 若栈的大小小于等于0 或者栈顶的字符串与此字符串不匹配 报错
                if(stkTcCnfDomain.size() <= 0)
                {
                    throw TC_Config_Exception("[TC_Config::parse]:parse error! <" + sName + "> hasn't matched domain.");
                }

                if(stkTcCnfDomain.top()->getName() != sName)
                {
                    throw TC_Config_Exception("[TC_Config::parse]:parse error! <" + stkTcCnfDomain.top()->getName() + "> hasn't match <" + sName +">.");
                }

                //栈弹出
                stkTcCnfDomain.pop();
            }
            else
            {
                // 一个域的开始
                // 获取该域的名字
                string name(line.substr(1, posl - 1));
                // 新建一个子域 并入栈
                stkTcCnfDomain.push(stkTcCnfDomain.top()->addSubDomain(name));
            }
        }
        else
        {
            // 域中间的配置行
            stkTcCnfDomain.top()->setParamValue(line);
        }
    }
    // 如果到最后域的大小不为1 说明还有的栈未弹出 还有域没有结束 报错
    if(stkTcCnfDomain.size() != 1)
    {
        throw TC_Config_Exception("[TC_Config::parse]:parse error : hasn't match");
    }
}

void TC_Config::parseFile(const string &sFileName)
{
    // 若长度为0 抛出异常
    if(sFileName.length() == 0)
    {
        throw TC_Config_Exception("[TC_Config::parseFile]:file name is empty");
    }

    // Input file stream class
    ifstream ff;
    // 打开文件 使流与这个文件相关联
    ff.open(sFileName.c_str());
    // 打开失败 抛出异常
    if (!ff)
    {
        throw TC_Config_Exception("[TC_Config::parseFile]:fopen fail: " + sFileName, errno);
    }

    parse(ff);
}

void TC_Config::parseString(const string& buffer)
{
    istringstream iss;
    iss.str(buffer);

    parse(iss);
}

string TC_Config::operator[](const string &path)
{
    TC_ConfigDomain::DomainPath dp = TC_ConfigDomain::parseDomainName(path, true);

    TC_ConfigDomain *pTcConfigDomain = searchTcConfigDomain(dp._domains);

    if(pTcConfigDomain == NULL)
    {
        throw TC_ConfigNoParam_Exception("[TC_Config::operator[]] path '" + path + "' not exits!");
    }

    return pTcConfigDomain->getParamValue(dp._param);
}

string TC_Config::get(const string &sName, const string &sDefault) const
{
    try
    {
        TC_ConfigDomain::DomainPath dp = TC_ConfigDomain::parseDomainName(sName, true);

        const TC_ConfigDomain *pTcConfigDomain = searchTcConfigDomain(dp._domains);

        if(pTcConfigDomain == NULL)
        {
            throw TC_ConfigNoParam_Exception("[TC_Config::get] path '" + sName + "' not exits!");
        }

        return pTcConfigDomain->getParamValue(dp._param);
    }
    catch ( TC_ConfigNoParam_Exception &ex )
    {
        return sDefault;
    }
}

    // 获取域下面的参数值对 放入map中
bool TC_Config::getDomainMap(const string &path, map<string, string> &m) const
{
    TC_ConfigDomain::DomainPath dp = TC_ConfigDomain::parseDomainName(path, false);

    const TC_ConfigDomain *pTcConfigDomain = searchTcConfigDomain(dp._domains);

    if(pTcConfigDomain == NULL)
    {
        return false;
    }

    m = pTcConfigDomain->getParamMap();

    return true;
}

map<string, string> TC_Config::getDomainMap(const string &path) const
{
    map<string, string> m;

    TC_ConfigDomain::DomainPath dp = TC_ConfigDomain::parseDomainName(path, false);

    const TC_ConfigDomain *pTcConfigDomain = searchTcConfigDomain(dp._domains);

    if(pTcConfigDomain != NULL)
    {
        m = pTcConfigDomain->getParamMap();
    }

    return m;
}

vector<string> TC_Config::getDomainKey(const string &path) const
{
    vector<string> v;

    TC_ConfigDomain::DomainPath dp = TC_ConfigDomain::parseDomainName(path, false);

    const TC_ConfigDomain *pTcConfigDomain = searchTcConfigDomain(dp._domains);

    if(pTcConfigDomain != NULL)
    {
        v = pTcConfigDomain->getKey();
    }

    return v;
}

vector<string> TC_Config::getDomainLine(const string &path) const
{
    vector<string> v;

    TC_ConfigDomain::DomainPath dp = TC_ConfigDomain::parseDomainName(path, false);

    const TC_ConfigDomain *pTcConfigDomain = searchTcConfigDomain(dp._domains);

    if(pTcConfigDomain != NULL)
    {
        v = pTcConfigDomain->getLine();
    }

    return v;
}

bool TC_Config::getDomainVector(const string &path, vector<string> &vtDomains) const
{
    TC_ConfigDomain::DomainPath dp = TC_ConfigDomain::parseDomainName(path, false);

    //根域, 特殊处理
    if(dp._domains.empty())
    {
        vtDomains = _root.getSubDomain();
        return !vtDomains.empty();
    }

    const TC_ConfigDomain *pTcConfigDomain = searchTcConfigDomain(dp._domains);

    if(pTcConfigDomain == NULL)
    {
        return false;
    }

    vtDomains = pTcConfigDomain->getSubDomain();

    return true;
}

vector<string> TC_Config::getDomainVector(const string &path) const
{
    TC_ConfigDomain::DomainPath dp = TC_ConfigDomain::parseDomainName(path, false);

    //根域, 特殊处理
    if(dp._domains.empty())
    {
        return _root.getSubDomain();
    }

    const TC_ConfigDomain *pTcConfigDomain = searchTcConfigDomain(dp._domains);

    if(pTcConfigDomain == NULL)
    {
        return vector<string>();
    }

    return pTcConfigDomain->getSubDomain();
}


TC_ConfigDomain *TC_Config::newTcConfigDomain(const string &sName)
{
    return new TC_ConfigDomain(sName);
}

TC_ConfigDomain *TC_Config::searchTcConfigDomain(const vector<string>& domains)
{
    return _root.getSubTcConfigDomain(domains.begin(), domains.end());
}

const TC_ConfigDomain *TC_Config::searchTcConfigDomain(const vector<string>& domains) const
{
    return _root.getSubTcConfigDomain(domains.begin(), domains.end());
}

int TC_Config::insertDomain(const string &sCurDomain, const string &sAddDomain, bool bCreate)
{
    TC_ConfigDomain::DomainPath dp = TC_ConfigDomain::parseDomainName(sCurDomain, false);

    TC_ConfigDomain *pTcConfigDomain = searchTcConfigDomain(dp._domains);

    if(pTcConfigDomain == NULL)
    {
        if(bCreate)
        {
            pTcConfigDomain = &_root;

            for(size_t i = 0; i < dp._domains.size(); i++)
            {
                pTcConfigDomain = pTcConfigDomain->addSubDomain(dp._domains[i]);
            }
        }
        else
        {
            return -1;
        }
    }

    pTcConfigDomain->addSubDomain(sAddDomain);

    return 0;
}

int TC_Config::insertDomainParam(const string &sCurDomain, const map<string, string> &m, bool bCreate)
{
    TC_ConfigDomain::DomainPath dp = TC_ConfigDomain::parseDomainName(sCurDomain, false);

    TC_ConfigDomain *pTcConfigDomain = searchTcConfigDomain(dp._domains);

    if(pTcConfigDomain == NULL)
    {
        if(bCreate)
        {
            pTcConfigDomain = &_root;

            for(size_t i = 0; i < dp._domains.size(); i++)
            {
                pTcConfigDomain = pTcConfigDomain->addSubDomain(dp._domains[i]);
            }
        }
        else
        {
            return -1;
        }
    }

    pTcConfigDomain->insertParamValue(m);

    return 0;
}

string TC_Config::tostr() const
{
    string buffer;

    map<string, TC_ConfigDomain*> msd = _root.getDomainMap();
    map<string, TC_ConfigDomain*>::const_iterator it = msd.begin();
    while (it != msd.end())
    {
        buffer += it->second->tostr(0);
        ++it;
    }

    return buffer;
}

void TC_Config::joinConfig(const TC_Config &cf, bool bUpdate)
{
    string buffer;
    if(bUpdate)
    {
        buffer = tostr() + cf.tostr();
    }
    else
    {
        buffer = cf.tostr() + tostr();
    }
    parseString(buffer);
}

}

