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

#include "util/tc_logger.h"
#include "util/tc_functor.h"
#include <iostream>
#include <string.h>

namespace tars
{
    bool TC_LoggerRoll::_bDyeingFlag = false;
    TC_ThreadMutex  TC_LoggerRoll::_mutexDyeing;
    //set<pthread_t>  TC_LoggerRoll::_setThreadID;
    hash_map<pthread_t, string>  TC_LoggerRoll::_mapThreadID;
    const string TarsLogByDay::FORMAT = "%Y%m%d";
    const string TarsLogByHour::FORMAT = "%Y%m%d%H";
    const string TarsLogByMinute::FORMAT = "%Y%m%d%H%M";

    void TC_LoggerRoll::setupThread(TC_LoggerThreadGroup *pThreadGroup)
    {
        assert(pThreadGroup != NULL);

        unSetupThread();

        // 加锁 向ThreadGroup中插入自己
        TC_LockT<TC_ThreadMutex> lock(_mutex);

        _pThreadGroup = pThreadGroup;

        TC_LoggerRollPtr self = this;

        _pThreadGroup->registerLogger(self);
    }

    void TC_LoggerRoll::unSetupThread()
    {
        // 加锁
        TC_LockT<TC_ThreadMutex> lock(_mutex);

        if (_pThreadGroup != NULL)
        {
            // 先清空buffer中的数据 将他们写入到文件中
            _pThreadGroup->flush();

            TC_LoggerRollPtr self = this;

            // 从ThreadGroup中去掉自己
            _pThreadGroup->unRegisterLogger(self);

            _pThreadGroup = NULL;
        }
        // 刷新
        flush();
    }

    void TC_LoggerRoll::write(const pair<int, string> &buffer)
    {
        pthread_t ThreadID = 0;
        if (_bDyeingFlag)
        {
            //  若开启了染色

            TC_LockT<TC_ThreadMutex> lock(_mutexDyeing);
            // 获取当前的线程ID
            pthread_t tmp = pthread_self();
            //if (_setThreadID.count(tmp) == 1)

            // 统计hash_map是否出现了tmp 要么是1（即出现了该元素），要么是0（即没出现这样的元素）
            if (_mapThreadID.count(tmp) == 1)
            {
                ThreadID = tmp;
            }
        }

        // 若有线程组了
        if (_pThreadGroup)
        {
            // 插入线程ID 和要写的内容
            _buffer.push_back(make_pair(ThreadID, buffer.second));
        }
        else
        {
            //同步记录日志
            deque<pair<int, string> > ds;
            ds.push_back(make_pair(ThreadID, buffer.second));
            // 记日志并滚动
            roll(ds);
        }
    }

    void TC_LoggerRoll::flush()
    {
        // 新建一个队列
        TC_ThreadQueue<pair<int, string> >::queue_type qt;
        // 交换两个队列
        _buffer.swap(qt);

        // 若qt不为空
        if (!qt.empty())
        {
            // 记录日志
            roll(qt);
        }
    }

//////////////////////////////////////////////////////////////////
//
    // 写日志线程组
    TC_LoggerThreadGroup::TC_LoggerThreadGroup() : _bTerminate(false)
    {
    }

    TC_LoggerThreadGroup::~TC_LoggerThreadGroup()
    {
        flush();

        _bTerminate = true;

        {
            Lock lock(*this);
            // 通知其他线程
            notifyAll();
        }

        // 停止线程池
        _tpool.stop();
        _tpool.waitForAllDone();
    }

    void TC_LoggerThreadGroup::start(size_t iThreadNum)
    {
        // 初始化线程池 创建线程
        _tpool.init(iThreadNum);
        // 启动
        _tpool.start();

        // 包装TC_LoggerThreadGroup的run函数
        TC_Functor<void> cmd(this, &TC_LoggerThreadGroup::run);
        TC_Functor<void>::wrapper_type wrapper(cmd);

        for (size_t i = 0; i < _tpool.getThreadNum(); i++)
        {
            // 将此函数添加到任务队列中执行
            _tpool.exec(wrapper);
        }
    }

    // 注册一个loggerRoll对象
    void TC_LoggerThreadGroup::registerLogger(TC_LoggerRollPtr &l)
    {
        Lock lock(*this);
        // 在logger set中插入此对象
        _logger.insert(l);
    }

    // 删除一个logger对象
    void TC_LoggerThreadGroup::unRegisterLogger(TC_LoggerRollPtr &l)
    {
        Lock lock(*this);

        _logger.erase(l);
    }

    void TC_LoggerThreadGroup::flush()
    {
        // logger roll 的set
        logger_set logger;

        {
            // 获取logger set
            Lock lock(*this);
            logger = _logger;
        }

        // 遍历 flush每一个
        logger_set::iterator it = logger.begin();
        while (it != logger.end())
        {
            try
            {
                it->get()->flush();
            }
            catch (...)
            {
            }
            ++it;
        }
    }

    void TC_LoggerThreadGroup::run()
    {
        while (!_bTerminate)
        {
            //100ms
            {
                Lock lock(*this);
                timedWait(100);
            }

            // 不停的flush 每一个
            flush();
        }
    }

//////////////////////////////////////////////////////////////////////////////////

    LoggerBuffer::LoggerBuffer() : _buffer(NULL), _buffer_len(0)
    {
    }

    LoggerBuffer::LoggerBuffer(TC_LoggerRollPtr roll, size_t buffer_len) : _roll(roll), _buffer(NULL), _buffer_len(buffer_len)
    {
        //设置get buffer, 无效, 不适用
        setg(NULL, NULL, NULL);

        //设置put buffer
        if (_roll)
        {
            //分配空间 char* 设置缓冲区的大小
            _buffer = new char[_buffer_len];
            // 设置输出的缓冲区的区间
            setp(_buffer, _buffer + _buffer_len);
        }
        else
        {
            setp(NULL, NULL);
            _buffer_len = 0;
        }
    }

    LoggerBuffer::~LoggerBuffer()
    {
        sync();
        if (_buffer)
        {
            delete[] _buffer;
        }
    }

    streamsize LoggerBuffer::xsputn(const char_type* s, streamsize n)
    {
        if (!_roll)
        {
            return n;
        }

        return std::basic_streambuf<char>::xsputn(s, n);
    }

    void LoggerBuffer::reserve(std::streamsize n)
    {
        if (n <= _buffer_len)
        {
            return;
        }

        //不超过最大大小
        if (n > MAX_BUFFER_LENGTH)
        {
            n = MAX_BUFFER_LENGTH;
        }

        int len = pptr() - pbase();
        char_type * p = new char_type[n];
        memcpy(p, _buffer, len);
        delete[] _buffer;
        _buffer     = p;
        _buffer_len = n;

        setp(_buffer, _buffer + _buffer_len);

        pbump(len);

        return;
    }

    std::basic_streambuf<char>::int_type LoggerBuffer::overflow(std::basic_streambuf<char>::int_type c)
    {
        if (!_roll)
        {
            return 0;
        }

        if (_buffer_len >= MAX_BUFFER_LENGTH)
        {
            sync();
        }
        else
        {
            reserve(_buffer_len * 2);
        }

        if (std::char_traits<char_type>::eq_int_type(c,std::char_traits<char_type>::eof()) )
        {
            return std::char_traits<char_type>::not_eof(c);
        }
        else
        {
            return sputc(c);
        }
        return 0;
    }

    int LoggerBuffer::sync()
    {
        //有数据
        if (pptr() > pbase())
        {
            std::streamsize len = pptr() - pbase();

            if (_roll)
            {
                //具体的写逻辑
                _roll->write(make_pair(0, string(pbase(), len)));
            }

            //重新设置put缓冲区, pptr()重置到pbase()处
            setp(pbase(), epptr());
        }
        return 0;
    }

////////////////////////////////////////////////////////////////////////////////////
//

}


