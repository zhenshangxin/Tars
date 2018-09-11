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

#ifndef  _TC_LOOP_QUEUE_H_
#define  _TC_LOOP_QUEUE_H_

#include <vector>
#include <stdlib.h>
#include <string.h>

using namespace std;

namespace tars
{
/////////////////////////////////////////////////
/**
 * @file tc_loop_queue.h 
 * @brief 循环队列,大小固定 . 
 *  
 */
/////////////////////////////////////////////////

// 默认大小为5

// 保存的是ReqMessage的指针
template<typename T, int Size=5>
class TC_LoopQueue
{
public:

    // queue_type为vector<ReqMessage*>
    typedef vector<T> queue_type;

    // 构造函数
    TC_LoopQueue(uint32_t iSize=Size)
    {
        //做个保护 最多不能超过 1000000
        assert(iSize<1000000);
        // 整形
        _iBegin = 0;
        _iEnd = 0;

        // 下标的大小
        _iCapacitySub = iSize;
        // 容量
        _iCapacity = iSize + 1;

        // 分配_iCapacity个T的空间（最大容量所占用的空间）
        _p=(T*)malloc(_iCapacity*sizeof(T));
        //_p= new T[_iCapacity];
    }
    ~TC_LoopQueue()
    {
        free(_p);
        //delete _p;
    }

    bool push_back(const T &t,bool & bEmpty,uint32_t & iBegin,uint32_t & iEnd)
    {
        bEmpty = false;
        //uint32_t iEnd = _iEnd;
        iEnd = _iEnd;
        iBegin = _iBegin;
        // begin在后 end在前
        //  0    1    2    3    4
        //  |    |
        //  end  begin
        // 若队列满 返回false
        if((iEnd > _iBegin && iEnd - _iBegin < 2) ||
                ( _iBegin > iEnd && _iBegin - iEnd > (_iCapacity-2) ) )
        {
            return false;
        }
        else
        {
            // 将t拷贝至对应的位置
            memcpy(_p+_iBegin,&t,sizeof(T));
            //*(_p+_iBegin) = t;

            if(_iEnd == _iBegin)
                bEmpty = true;

            // begin加一 若到了最大就变为0
            if(_iBegin == _iCapacitySub)
                _iBegin = 0;
            else
                _iBegin++;

            if(!bEmpty && 1 == size())
                bEmpty = true;

            return true;
        }
    }

    bool push_back(const T &t,bool & bEmpty)
    {
        bEmpty = false;
        // 获取到尾指针的位置
        uint32_t iEnd = _iEnd;

        // iEnd > _iBegin && iEnd - _iBegin < 2 代表尾指针在头指针之前一个位置的地方
        // _iBegin > iEnd && _iBegin - iEnd > (_iCapacity-2) 代表头指针在最开始 尾指针在最后
        if((iEnd > _iBegin && iEnd - _iBegin < 2) ||
                ( _iBegin > iEnd && _iBegin - iEnd > (_iCapacity-2) ) )
        {
            // 队列满 返回
            return false;
        }
        else
        {
            // 拷贝
            memcpy(_p+_iBegin,&t,sizeof(T));
            //*(_p+_iBegin) = t;

            // 如果头尾指针在一起 那么当前队列为空
            if(_iEnd == _iBegin)
                bEmpty = true;

            if(_iBegin == _iCapacitySub)
                _iBegin = 0;
            else
                _iBegin++;

            if(!bEmpty && 1 == size())
                bEmpty = true;
#if 0
            if(1 == size())
                bEmpty = true;
#endif

            return true;
        }
    }

    bool push_back(const T &t)
    {
        bool bEmpty;
        return push_back(t,bEmpty);
    }


    // 插入一整个vector
    bool push_back(const queue_type &vt)
    {
        uint32_t iEnd=_iEnd;
        // 若队列放不下这么多 返回
        if(vt.size()>(_iCapacity-1) ||
                (iEnd>_iBegin && (iEnd-_iBegin)<(vt.size()+1)) ||
                ( _iBegin>iEnd && (_iBegin-iEnd)>(_iCapacity-vt.size()-1) ) )
        {
            return false;
        }
        else
        {
            // 遍历并插入
            for(uint32_t i=0;i<vt.size();i++)
            {
                memcpy(_p+_iBegin,&vt[i],sizeof(T));
                //*(_p+_iBegin) = vt[i];
                if(_iBegin == _iCapacitySub)
                    _iBegin = 0;
                else
                    _iBegin++;
            }
            return true;
        }
    }

    bool pop_front(T &t)
    {
        // 对列为空
        if(_iEnd==_iBegin)
        {
            return false;
        }

        // 拷贝一个元素到t中
        memcpy(&t,_p+_iEnd,sizeof(T));
        //t = *(_p+_iEnd);

        if(_iEnd == _iCapacitySub)
            _iEnd = 0;
        else
            _iEnd++;
        return true;
    }

    bool pop_front()
    {
        if(_iEnd==_iBegin)
        {
            return false;
        }
        if(_iEnd == _iCapacitySub)
            _iEnd = 0;
        else
            _iEnd++;
        return true;
    }

    // 获取第一个 此获取的只是一份拷贝 并不是引用
    bool get_front(T &t)
    {
        if(_iEnd==_iBegin)
        {
            return false;
        }
        memcpy(&t,_p+_iEnd,sizeof(T));
        //t = *(_p+_iEnd);
        return true;
    }

    bool empty()
    {
        if(_iEnd == _iBegin)
        {
            return true;
        }
        return false;
    }

    // 获取队列的大小
    uint32_t size()
    {
        uint32_t iBegin=_iBegin;
        uint32_t iEnd=_iEnd;
        if(iBegin<iEnd)
            return iBegin+_iCapacity-iEnd;
        return iBegin-iEnd;
    }

    uint32_t getCapacity()
    {
        return _iCapacity;
    }

private:
    T * _p;
    uint32_t _iCapacity;
    uint32_t _iCapacitySub;
    uint32_t _iBegin;
    uint32_t _iEnd;
};

}

#endif

