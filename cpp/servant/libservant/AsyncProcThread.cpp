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

#include "servant/AsyncProcThread.h"
#include "servant/Communicator.h"
#include "servant/StatReport.h"
#include "servant/TarsLogger.h"

namespace tars
{


AsyncProcThread::AsyncProcThread(size_t iQueueCap)
: _terminate(false)
{
    // 异步队列大小为10000
    // 循环队列
     _msgQueue = new ReqInfoQueue(iQueueCap);
}

AsyncProcThread::~AsyncProcThread()
{
    terminate();

    if(_msgQueue)
    {
        delete _msgQueue;
        _msgQueue = NULL;
    }
}

void AsyncProcThread::terminate()
{
    Lock lock(*this);

    _terminate = true;

    notifyAll();
}

void AsyncProcThread::push_back(ReqMessage * msg)
{
    bool bFlag = _msgQueue->push_back(msg);
    
    {
        TC_ThreadLock::Lock lock(*this);
        notify();
    }

    if(!bFlag)
    {
        TLOGERROR("[TARS][AsyncProcThread::push_back] async_queue full." << endl);
        delete msg;
        msg = NULL;
    }
}

void AsyncProcThread::run()
{
    while (!_terminate)
    {
        ReqMessage * msg = NULL;

        //异步请求回来的响应包处理

        // 若队列为空 则等待
        if(_msgQueue->empty())
        {
            TC_ThreadLock::Lock lock(*this);
            timedWait(1000);
        }

        //
        if (_msgQueue->pop_front(msg))
        {
            // 取出第一个ReqMessage

            ServantProxyThreadData * pServantProxyThreadData = ServantProxyThreadData::getData();
            assert(pServantProxyThreadData != NULL);
            // 根据ReqMessage来设置ServantProxyThreadData的染色信息
            pServantProxyThreadData->_dyeing  = msg->bDyeing;
            pServantProxyThreadData->_dyeingKey = msg->sDyeingKey;

            // 拷贝被调用的adapter的地址信息到_szHost中
            if(msg->adapter)
            {
                   snprintf(pServantProxyThreadData->_szHost, sizeof(pServantProxyThreadData->_szHost), "%s", msg->adapter->endpoint().desc().c_str());
            }

            try
            {
                // 放入智能指针中
                ReqMessagePtr msgPtr = msg;
                // 异步调用时的回调对象
                msg->callback->onDispatch(msgPtr);
            }
            catch (exception& e)
            {
                TLOGERROR("[TARS][AsyncProcThread exception]:" << e.what() << endl);
            }
            catch (...)
            {
                TLOGERROR("[TARS][AsyncProcThread exception.]" << endl);
            }
        }
    }
}
/////////////////////////////////////////////////////////////////////////
}
