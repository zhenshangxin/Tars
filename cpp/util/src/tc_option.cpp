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

#include "util/tc_option.h"
#include "util/tc_common.h"

namespace tars
{

void TC_Option::decode(int argc, char *argv[])
{
    // 存放 参数名 值的一个map
    _mParam.clear();

    // 将argv放入v中
    vector<string> v;
    for(int i = 1; i < argc; i++)
    {
        v.push_back(argv[i]);
    }

    for(size_t i = 0; i < v.size(); i++)
    {
        // 如果字符串的长度大于0 且前两个字符为--
        if(v[i].length() > 2 && v[i].substr(0,2) == "--")
        {
            // 解析每个命令行参数 放入_mParam中
            parse(v[i]);
        }
        else
        {
            // 放入存放普通参数的vector中
            _vSingle.push_back(v[i]);
        }
    }
}

void TC_Option::parse(const string &s)
{
    // 返回第一次出现=号的位置 如果没找到则返回npos 代表这个string所能容纳的最大size_t 通常用来表示没有匹配
    string::size_type pos = s.find('=');

    // 如果找到了
    if( pos != string::npos)
    {
        // 将name 和 Value 都放入_mParam中
        _mParam[s.substr(2, pos-2)] = s.substr(pos+1);
    }
    else
    {
        // 未找到 value为空
        _mParam[s.substr(2, pos-2)] = "";
    }
}

string TC_Option::getValue(const string &sName)
{
    if(_mParam.find(sName) != _mParam.end())
    {
        return _mParam[sName];
    }
    return "";
}

bool TC_Option::hasParam(const string &sName)
{
    return _mParam.find(sName) != _mParam.end();
}

vector<string>& TC_Option::getSingle()
{
    return _vSingle;
}

map<string, string>& TC_Option::getMulti()
{
    return _mParam;
}

}


