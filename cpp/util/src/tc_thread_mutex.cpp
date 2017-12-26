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

#include "util/tc_thread_mutex.h"
#include <string.h>
#include <iostream>
#include <cassert>
// 虽然没有包含pthread.h这个头文件 但是仍然能够通过编译 因为c++标准库中的文件已经包含了此头文件

namespace tars
{
// 构造
TC_ThreadMutex::TC_ThreadMutex()
{
    int rc;
    // 互斥量属性
    pthread_mutexattr_t attr;
    // 初始化此结构
    rc = pthread_mutexattr_init(&attr);
    assert(rc == 0);
    // 设置错误检查
    rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
    assert(rc == 0);
    // 初始化锁
    rc = pthread_mutex_init(&_mutex, &attr);
    assert(rc == 0);
    // 销毁属性
    rc = pthread_mutexattr_destroy(&attr);
    assert(rc == 0);

    if(rc != 0)
    {
        throw TC_ThreadMutex_Exception("[TC_ThreadMutex::TC_ThreadMutex] pthread_mutexattr_init error", rc);
    }
}

TC_ThreadMutex::~TC_ThreadMutex()
{
    int rc = 0;
    // 销毁锁
    rc = pthread_mutex_destroy(&_mutex);
    if(rc != 0)
    {
        cerr << "[TC_ThreadMutex::~TC_ThreadMutex] pthread_mutex_destroy error:" << string(strerror(rc)) << endl;
    }
//    assert(rc == 0);
}

void TC_ThreadMutex::lock() const
{
    // 加锁
    int rc = pthread_mutex_lock(&_mutex);
    if(rc != 0)
    {
        if(rc == EDEADLK)
        {
            throw TC_ThreadMutex_Exception("[TC_ThreadMutex::lock] pthread_mutex_lock dead lock error", rc);
        }
        else
        {
            throw TC_ThreadMutex_Exception("[TC_ThreadMutex::lock] pthread_mutex_lock error", rc);
        }
    }
}

bool TC_ThreadMutex::tryLock() const
{
    int rc = pthread_mutex_trylock(&_mutex);
    if(rc != 0 && rc != EBUSY)
    {
        if(rc == EDEADLK)
        {
            throw TC_ThreadMutex_Exception("[TC_ThreadMutex::tryLock] pthread_mutex_trylock dead lock error", rc);
        }
        else
        {
            throw TC_ThreadMutex_Exception("[TC_ThreadMutex::tryLock] pthread_mutex_trylock error", rc);
        }
    }
    return (rc == 0);
}

void TC_ThreadMutex::unlock() const
{
    // 解锁
    int rc = pthread_mutex_unlock(&_mutex);
    if(rc != 0)
    {
        throw TC_ThreadMutex_Exception("[TC_ThreadMutex::unlock] pthread_mutex_unlock error", rc);
    }
}

int TC_ThreadMutex::count() const
{
    return 0;
}

void TC_ThreadMutex::count(int c) const
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TC_ThreadRecMutex::TC_ThreadRecMutex()
: _count(0)
{
    int rc;

    pthread_mutexattr_t attr;
    rc = pthread_mutexattr_init(&attr);
    if(rc != 0)
    {
        throw TC_ThreadMutex_Exception("[TC_ThreadRecMutex::TC_ThreadRecMutex] pthread_mutexattr_init error", rc);
    }
    // 允许同一线程在互斥量解锁前对该互斥量进行多次加锁 在解锁次数与加锁次数不相同的情况下 不会释放锁
    rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    if(rc != 0)
    {
        throw TC_ThreadMutex_Exception("[TC_ThreadRecMutex::TC_ThreadRecMutex] pthread_mutexattr_settype error", rc);
    }

    rc = pthread_mutex_init(&_mutex, &attr);
    if(rc != 0)
    {
        throw TC_ThreadMutex_Exception("[TC_ThreadRecMutex::TC_ThreadRecMutex] pthread_mutex_init error", rc);
    }

    rc = pthread_mutexattr_destroy(&attr);
    if(rc != 0)
    {
        throw TC_ThreadMutex_Exception("[TC_ThreadRecMutex::TC_ThreadRecMutex] pthread_mutexattr_destroy error", rc);
    }
}

TC_ThreadRecMutex::~TC_ThreadRecMutex()
{
    // 执行count次unlock操作 直到解锁
    while (_count)
    {
        unlock();
    }

    int rc = 0;
    // 销毁锁
    rc = pthread_mutex_destroy(&_mutex);
    if(rc != 0)
    {
        cerr << "[TC_ThreadRecMutex::~TC_ThreadRecMutex] pthread_mutex_destroy error:" << string(strerror(rc)) << endl;
    }
//    assert(rc == 0);
}

int TC_ThreadRecMutex::lock() const
{
    // 加锁
    int rc = pthread_mutex_lock(&_mutex);
    if(rc != 0)
    {
        throw TC_ThreadMutex_Exception("[TC_ThreadRecMutex::lock] pthread_mutex_lock error", rc);
    }

    // 若已经加过锁了  增加引用计数并解锁
    if(++_count > 1)
    {
        rc = pthread_mutex_unlock(&_mutex);
        assert(rc == 0);
    }

    return rc;
}

int TC_ThreadRecMutex::unlock() const
{
    // 计数减1 若计数为0则解锁
    if(--_count == 0)
    {
        int rc = 0;
        rc = pthread_mutex_unlock(&_mutex);
        return rc;
    }
    return 0;
}

bool TC_ThreadRecMutex::tryLock() const
{
    int rc = pthread_mutex_trylock(&_mutex);
    if(rc != 0 )
    {
        if(rc != EBUSY)
        {
            throw TC_ThreadMutex_Exception("[TC_ThreadRecMutex::tryLock] pthread_mutex_trylock error", rc);
        }
    }
    else if(++_count > 1)
    {
        rc = pthread_mutex_unlock(&_mutex);
        if(rc != 0)
        {
            throw TC_ThreadMutex_Exception("[TC_ThreadRecMutex::tryLock] pthread_mutex_unlock error", rc);
        }
    }
    return (rc == 0);
}

bool TC_ThreadRecMutex::willUnlock() const
{
    return _count == 1;
}

int TC_ThreadRecMutex::count() const
{
    int c   = _count;
    _count  = 0;
    return c;
}

void TC_ThreadRecMutex::count(int c) const
{
    _count = c;
}

}

