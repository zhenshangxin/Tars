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

#ifndef __TARS_COMMUNICATOR_FACTORY_H_
#define __TARS_COMMUNICATOR_FACTORY_H_

#include "servant/Global.h"
#include "servant/Communicator.h"

namespace tars
{
//////////////////////////////////////////////////////////////////////////////
/**
 * 创建CommunicatorPtr对象
 */
class CommunicatorFactory : public TC_Singleton<CommunicatorFactory>, public TC_HandleBase, public TC_ThreadRecMutex
{
public:
    /**
     * 构造函数
     * @param comm
     */
    CommunicatorFactory(){};

    /**
     * 析构
     */
    ~CommunicatorFactory(){};

    /**
     * 获取CommunicatorPtr对象
     * @param name
     * @return ServantPrx
     */
    CommunicatorPtr getCommunicator(const string& name = "default")
    {
        // 构造时上锁 析构时解锁
        TC_LockT<TC_ThreadRecMutex> lock(*this);

        map<string, CommunicatorPtr>::iterator it = _comms.find(name);

        if (it == _comms.end())
        {
            // 若没找到 则新建一个Communicator
            _comms[name] = new Communicator();

            it = _comms.find(name);
        }
        return it->second;
    }
    
     /**
     * 获取CommunicatorPtr对象 
     * @param conf 
     * @param name
     * @return ServantPrx
     */
    CommunicatorPtr getCommunicator(TC_Config& conf, const string& name = "default")
    {
        TC_LockT<TC_ThreadRecMutex> lock(*this);
        // 查找名为name的Communicator name默认为default
        map<string, CommunicatorPtr>::iterator it = _comms.find(name);

        if (it == _comms.end())
        {
            // 没找到 新建一个 并加入_comms中
            _comms[name] = new Communicator(conf);

            it = _comms.find(name);

            return it->second;
        }

        string s = "";

        // 根据配置文件来设置属性
        it->second->setProperty(conf);
        // 重新加载属性
        it->second->reloadProperty(s);

        return it->second;
    }

private:
  
    /**
     * 已创建的对象
     */
    map<string, CommunicatorPtr> _comms;
};
//////////////////////////////////////////////////////
}

#endif
