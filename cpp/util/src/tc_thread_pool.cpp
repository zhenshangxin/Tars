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

#include "util/tc_thread_pool.h"
#include "util/tc_common.h"

#include <iostream>

namespace tars
{

TC_ThreadPool::ThreadWorker::ThreadWorker(TC_ThreadPool *tpool)
: _tpool(tpool)
, _bTerminate(false)
{
}

void TC_ThreadPool::ThreadWorker::terminate()
{
    _bTerminate = true;
    _tpool->notifyT();
}

void TC_ThreadPool::ThreadWorker::run()
{
    //从 startquene 任务队列中获取一个任务
    TC_FunctorWrapperInterface *pst = _tpool->get();
    if(pst)
    {
        try
        {
            (*pst)();
        }
        catch ( ... )
        {
        }
        // 执行完就删除
        delete pst;
        pst = NULL;
    }

    //从任务对列中获取一个任务 将此线程加入繁忙的线程队列中
    while (!_bTerminate)
    {
        TC_FunctorWrapperInterface *pfw = _tpool->get(this);
        if(pfw != NULL)
        {
            auto_ptr<TC_FunctorWrapperInterface> apfw(pfw);

            try
            {
                (*pfw)();
            }
            catch ( ... )
            {
            }

            _tpool->idle(this);
        }
    }

    //结束
    _tpool->exit();
}

//////////////////////////////////////////////////////////////
//
//

// 类内静态变量g_key_initialize的初始化
TC_ThreadPool::KeyInitialize TC_ThreadPool::g_key_initialize;
pthread_key_t TC_ThreadPool::g_key;

void TC_ThreadPool::destructor(void *p)
{
    ThreadData *ttd = (ThreadData*)p;
    if(ttd)
    {
        delete ttd;
    }
}

void TC_ThreadPool::exit()
{
    // 线程私有数据
    TC_ThreadPool::ThreadData *p = getThreadData();
    if(p)
    {
        delete p;
        // 删掉线程私有数据
        int ret = pthread_setspecific(g_key, NULL);
        if(ret != 0)
        {
            throw TC_ThreadPool_Exception("[TC_ThreadPool::setThreadData] pthread_setspecific error", ret);
        }
    }
    // 清空队列
    _jobqueue.clear();
}

// 设置线程私有数据 删除之前的 赋值一个新的
void TC_ThreadPool::setThreadData(TC_ThreadPool::ThreadData *p)
{
    TC_ThreadPool::ThreadData *pOld = getThreadData();
    if(pOld != NULL && pOld != p)
    {
        delete pOld;
    }

    int ret = pthread_setspecific(g_key, (void *)p);
    if(ret != 0)
    {
        throw TC_ThreadPool_Exception("[TC_ThreadPool::setThreadData] pthread_setspecific error", ret);
    }
}

TC_ThreadPool::ThreadData* TC_ThreadPool::getThreadData()
{
    return (ThreadData *)pthread_getspecific(g_key);
}

void TC_ThreadPool::setThreadData(pthread_key_t pkey, ThreadData *p)
{
    TC_ThreadPool::ThreadData *pOld = getThreadData(pkey);
    if(pOld != NULL && pOld != p)
    {
        delete pOld;
    }

    int ret = pthread_setspecific(pkey, (void *)p);
    if(ret != 0)
    {
        throw TC_ThreadPool_Exception("[TC_ThreadPool::setThreadData] pthread_setspecific error", ret);
    }
}

TC_ThreadPool::ThreadData* TC_ThreadPool::getThreadData(pthread_key_t pkey)
{
    return (ThreadData *)pthread_getspecific(pkey);
}

TC_ThreadPool::TC_ThreadPool()
: _bAllDone(true)
{
}

TC_ThreadPool::~TC_ThreadPool()
{
    stop();
    clear();
}

void TC_ThreadPool::clear()
{
    // 清空vector 和 set 删掉所有线程
    std::vector<ThreadWorker*>::iterator it = _jobthread.begin();
    while(it != _jobthread.end())
    {
        delete (*it);
        ++it;
    }

    _jobthread.clear();
    _busthread.clear();
}

void TC_ThreadPool::init(size_t num)
{
    // 先停止所有线程
    stop();
    // 加锁
    Lock sync(*this);
    // 清空所有线程
    clear();

    for(size_t i = 0; i < num; i++)
    {
        // 新建线程并加入vector中
        _jobthread.push_back(new ThreadWorker(this));
    }
}

void TC_ThreadPool::stop()
{
    Lock sync(*this);
    // 遍历工作线程 停止
    std::vector<ThreadWorker*>::iterator it = _jobthread.begin();
    while(it != _jobthread.end())
    {
        if ((*it)->isAlive())
        {
            (*it)->terminate();
            (*it)->getThreadControl().join();
        }
        ++it;
    }
    _bAllDone = true;
}

void TC_ThreadPool::start()
{
    Lock sync(*this);

    std::vector<ThreadWorker*>::iterator it = _jobthread.begin();
    while(it != _jobthread.end())
    {
        (*it)->start();
        ++it;
    }
    _bAllDone = false;
}

bool TC_ThreadPool::finish()
{
    return _startqueue.empty() && _jobqueue.empty() && _busthread.empty() && _bAllDone;
}

bool TC_ThreadPool::waitForAllDone(int millsecond)
{
    // 加锁
    Lock sync(_tmutex);

start1:
    //任务队列和繁忙线程都是空的
    if (finish())
    {
        return true;
    }

    //永远等待 直到所有任务完成
    if(millsecond < 0)
    {
        _tmutex.timedWait(1000);
        goto start1;
    }

    // 获取当前时间的毫秒数
    int64_t iNow= TC_Common::now2ms();
    int m       = millsecond;
start2:
    // 等待事件通知
    bool b = _tmutex.timedWait(millsecond);
    //完成处理了
    if(finish())
    {
        return true;
    }

    if(!b)
    {
        return false;
    }

    millsecond = max((int64_t)0, m  - (TC_Common::now2ms() - iNow));
    goto start2;

    return false;
}

TC_FunctorWrapperInterface *TC_ThreadPool::get(ThreadWorker *ptw)
{
    TC_FunctorWrapperInterface *pFunctorWrapper = NULL;
    if(!_jobqueue.pop_front(pFunctorWrapper, 1000))
    {
        return NULL;
    }

    {
        Lock sync(_tmutex);
        // 若获取到任务 将此线程插入到繁忙的线程中
        _busthread.insert(ptw);
    }

    return pFunctorWrapper;
}

TC_FunctorWrapperInterface *TC_ThreadPool::get()
{
    TC_FunctorWrapperInterface *pFunctorWrapper = NULL;
    if(!_startqueue.pop_front(pFunctorWrapper))
    {
        return NULL;
    }

    return pFunctorWrapper;
}

void TC_ThreadPool::idle(ThreadWorker *ptw)
{
    // 从繁忙的线程中将此线程删除
    Lock sync(_tmutex);
    _busthread.erase(ptw);

    //无繁忙线程, 通知等待在线程池结束的线程醒过来
    if(_busthread.empty())
    {
        _bAllDone = true;
        _tmutex.notifyAll();
    }
}

void TC_ThreadPool::notifyT()
{
    _jobqueue.notifyT();
}



}
