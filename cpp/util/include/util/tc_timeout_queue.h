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

#ifndef __TC_TIMEOUT_QUEUE_H
#define __TC_TIMEOUT_QUEUE_H

#include <deque>
//#include <map>
#include <ext/hash_map>
#include <iostream>
#include <list>
#include <cassert>
#include "util/tc_autoptr.h"
#include "util/tc_monitor.h"
#include "util/tc_functor.h"
#include "util/tc_timeprovider.h"

using namespace std;
using namespace __gnu_cxx;
 
namespace tars
{
/////////////////////////////////////////////////
/** 
 * @file tc_timeout_queue.h
 * @brief 超时队列(模板元素只能是智能指针).
 * 
 */            
/////////////////////////////////////////////////

template<class T>
class TC_TimeoutQueue: public TC_ThreadMutex, public TC_HandleBase
{
public:

    struct PtrInfo;

    struct NodeInfo;

   // typedef map<uint32_t, PtrInfo> data_type;
    typedef hash_map<uint32_t, PtrInfo> data_type;

    typedef list<NodeInfo>         time_type;

    typedef TC_Functor<void, TL::TYPELIST_1(T&)> data_functor;

    struct PtrInfo
    {
        T ptr;

        // 保存的是list的迭代器
        typename time_type::iterator timeIter;
    };

    struct NodeInfo
    {
        bool hasPoped;

        int64_t createTime;

        typename data_type::iterator dataIter;
    };

    /**
     * @brief 超时队列，缺省5s超时. 
     *  
     * @param timeout 超时设定时间
     * @param size
     */
    TC_TimeoutQueue(int timeout = 5*1000,size_t size = 100 ) : _uniqId(0), _timeout(timeout) 
    {
        // 指向list的最后一个元素
        _firstNoPopIter=_time.end();
        // 增加hashmap中桶的数量 size为桶的数量
        _data.resize(size);
    }

    /**
     * @brief  产生该队列的下一个ID
     */
    uint32_t generateId();

    /**
     * @brief 获取指定id的数据. 
     *  
     * @param id 指定的数据的id
     * @return T 指定id的数据
     */
    T get(uint32_t uniqId, bool bErase = true);

    /**
     * @brief 删除. 
     *  
     * @param uniqId 要删除的数据的id
     * @return T     被删除的数据
     */
    T erase(uint32_t uniqId);

    /**
     * @brief 设置消息到队列尾端. 
     *  
     * @param ptr        要插入到队列尾端的消息
     * @return uint32_t id号
     */
    bool push(T& ptr, uint32_t uniqId);

    /**
     * @brief 超时删除数据
     */
    void timeout();

    /**
     * @brief 删除超时的数据，并用df对数据做处理 
     */
    void timeout(data_functor &df);

    /**
     * @brief 取出队列头部的消息. 
     *  
     * @return T 队列头部的消息
     */
    T pop();

    /**
     * @brief 取出队列头部的消息(减少一次copy).
     * 
     * @param t 
     */
    bool pop(T &t);

    /**
     * @brief 交换数据. 
     *  
     * @param q
     * @return bool
     */
    bool swap(deque<T> &q);

    /**
     * @brief 设置超时时间(毫秒).
     *  
     * @param timeout
     */
    void setTimeout(int timeout) { _timeout = timeout; }

    /**
     * @brief 队列中的数据.
     * 
     * @return size_t
     */
    size_t size() const { TC_LockT<TC_ThreadMutex> lock(*this); return _data.size(); }

protected:
    uint32_t                        _uniqId;
    time_t                          _timeout;
    data_type                       _data;
    time_type                       _time;
    typename time_type::iterator    _firstNoPopIter;
};

// 获取到指定ID的数据
template<typename T> T TC_TimeoutQueue<T>::get(uint32_t uniqId, bool bErase)
{
    TC_LockT<TC_ThreadMutex> lock(*this);

    // 从hash map 中取
    typename data_type::iterator it = _data.find(uniqId);

    if(it == _data.end())
    {
        return NULL;
    }

    T ptr = it->second.ptr;

    if(bErase)
    {
        // 释放掉此元素
        // 若指向第一个没有被pop的数据的指针刚好指向这个数据 那么就将这个指针向后移动一位
        if(_firstNoPopIter == it->second.timeIter)
        {
            ++_firstNoPopIter;
        }
        _time.erase(it->second.timeIter);
        _data.erase(it);
    }

    return ptr;    
}

// 产生一个ID
template<typename T> uint32_t TC_TimeoutQueue<T>::generateId()
{
    TC_LockT<TC_ThreadMutex> lock(*this);

    while (++_uniqId == 0);

    return _uniqId;
}

// 将消息插入到尾端
template<typename T> bool TC_TimeoutQueue<T>::push(T& ptr, uint32_t uniqId)
{
    TC_LockT<TC_ThreadMutex> lock(*this);

    // 生成ptr info
    PtrInfo pi;

    pi.ptr = ptr;

    pair<typename data_type::iterator, bool> result;
    // 插入hash map 中
    result = _data.insert(make_pair(uniqId, pi));

    if (result.second == false) return false;

    // 被插入节点的迭代器
    typename data_type::iterator it = result.first;

    // 生成ni
    NodeInfo ni;
    struct timeval tv;
    TC_TimeProvider::getInstance()->getNow(&tv);

    ni.createTime = tv.tv_sec * (int64_t)1000 + tv.tv_usec/1000;

    ni.dataIter = it;

    ni.hasPoped = false;
    // 将ni 放到队列最后
    _time.push_back(ni);

    // 指向列表的最后一个元素 也就是刚刚被插入的那个元素
    typename time_type::iterator tmp = _time.end();

    --tmp;

    it->second.timeIter = tmp;

    // 如果指向end 则让它指向最后一个元素 否则不变 也就是列表为空的情况下
    if (_firstNoPopIter == _time.end())
    {
        _firstNoPopIter = tmp;
    }

    return true;
}

// 超时删除数据
template<typename T> void TC_TimeoutQueue<T>::timeout()
{
    // 获取当前时间
    struct timeval tv;
    TC_TimeProvider::getInstance()->getNow(&tv);

    while(true)
    {
        TC_LockT<TC_ThreadMutex> lock(*this);

        typename time_type::iterator it = _time.begin();

        // 开始遍历 若超过所设置的超时时间
        if(it != _time.end() && tv.tv_sec*(int64_t)1000+tv.tv_usec/1000-it->createTime>_timeout)
        {
            // 删除
            _data.erase(it->dataIter);

            if(_firstNoPopIter == it)
            {   // 若指向第一个没有被Pop 的数据的指针指向当前对象 则将此指针向后移动一位
                ++_firstNoPopIter;
            }
            // 然后再从list中释放此对象
            _time.erase(it);
        }
        else
        {
            break;
        }
    }
}

// 超时删除数据 并使用df对数据进行处理
template<typename T> void TC_TimeoutQueue<T>::timeout(data_functor &df)
{
    struct timeval tv;
    TC_TimeProvider::getInstance()->getNow(&tv);
    while(true)
    {
        T ptr;

        {
            TC_LockT<TC_ThreadMutex> lock(*this);

            typename time_type::iterator it = _time.begin();

            if(it != _time.end() && tv.tv_sec*(int64_t)1000+tv.tv_usec/1000 - it->createTime > _timeout)
            {
                ptr = (*it->dataIter).second.ptr;

                _data.erase(it->dataIter);

                if(_firstNoPopIter == it)
                {
                    _firstNoPopIter++;
                }
                _time.erase(it);
            }
            else
            {
                break;
            }
        }

        try { df(ptr); } catch(...) { }
    }
}

// 删除数据
template<typename T> T TC_TimeoutQueue<T>::erase(uint32_t uniqId)
{
    TC_LockT<TC_ThreadMutex> lock(*this);

    typename data_type::iterator it = _data.find(uniqId);

    if(it == _data.end())
    {
        return NULL;
    }

    T ptr = it->second.ptr;

    if(_firstNoPopIter == it->second.timeIter)
    {
        _firstNoPopIter++;
    }
    _time.erase(it->second.timeIter);

    _data.erase(it);

    return ptr;    
}


template<typename T> T TC_TimeoutQueue<T>::pop()
{
    T ptr;

    return pop(ptr) ? ptr : NULL;
}

// 取出队列中的数据 但是没有删除
template<typename T> bool TC_TimeoutQueue<T>::pop(T &ptr)
{
    TC_LockT<TC_ThreadMutex> lock(*this);


    // 列表为空 返回错误
    if(_time.empty())
    {
        return false;
    }

    // 指向列表的头
    typename time_type::iterator it = _time.begin();

    if (it->hasPoped == true)
    {
        it = _firstNoPopIter;
    }

    if (it == _time.end())
    {
        return false;
    }

    assert(it->hasPoped == false);

    ptr = it->dataIter->second.ptr;

    it->hasPoped = true;

    _firstNoPopIter = it;

    ++_firstNoPopIter;

    return true;
}

// 将数据拷贝到q中
template<typename T> bool TC_TimeoutQueue<T>::swap(deque<T> &q)
{
    TC_LockT<TC_ThreadMutex> lock(*this);

    if(_time.empty())
    {
        return false;
    }

    typename time_type::iterator it = _time.begin();

    while(it != _time.end())
    {
        // 遍历
        if (it->hasPoped == true)
        {
            it = _firstNoPopIter;
        }

        if (it == _time.end())
        {
            break;
        }
    
        assert(it->hasPoped == false);
    
        T ptr = it->dataIter->second.ptr;
    
        it->hasPoped = true;
    
        _firstNoPopIter = it;
    
        ++_firstNoPopIter;

        q.push_back(ptr);

        ++it;
    }

    if(q.empty())
    {
        return false;
    }

    return true;
}
/////////////////////////////////////////////////////////////////
}
#endif
