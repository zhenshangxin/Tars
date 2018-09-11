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

#ifndef __TC_SINGLETON_H__
#define __TC_SINGLETON_H__

#include "util/tc_monitor.h"
#include <cassert>
#include <cstdlib>

namespace tars
{
/////////////////////////////////////////////////
/** 
 * @file tc_singleton.h 
 * @brief  单件类 .  
 *  
 * 单件实现类
 * 
 * 没有实现对单件生命周期的管理,使用示例代码如下:
 * 
 * class A : public TC_Singleton<A, CreateStatic,  DefaultLifetime>
 * 
 * {
 * 
 *  public:
 * 
 *    A(){cout << "A" << endl;}
 * 
 *   ~A()
 * 
 *   {
 * 
 *     cout << "~A" << endl;
 * 
 * 
 *    }
 *
 *    void test(){cout << "test A" << endl;}
 * 
 * };
 * 对象的创建方式由CreatePolicy指定, 有如下方式:
 * 
 * CreateUsingNew: 在堆中采用new创建
 * 
 * CreateStatic`: 在栈中采用static创建
 * 
 * 对象生命周期管理由LifetimePolicy指定, 有如下方式:
 * 
 * DefaultLifetime:缺省声明周期管理
 * 
 *如果单件对象已经析够, 但是还有调用, 会触发异常 
 * 
 * PhoneixLifetime:不死生命周期
 * 
 * 如果单件对象已经析够, 但是还有调用, 会再创建一个
 * 
 * NoDestroyLifetime:不析够
 * 
 * 对象创建后不会调用析够函数析够, 通常采用实例中的方式就可以了
 *
 */              
/////////////////////////////////////////////////

/**
 * @brief 定义CreatePolicy:定义对象创建策略
 */
template<typename T>
class CreateUsingNew
{
public:
    /**
     * @brief  创建.
     *  
     * @return T*
     */
    static T* create() 
    { 
        return new T; 
    }

    /**
     * @brief 释放. 
     *  
     * @param t
     */
    static void destroy(T *t) 
    { 
        delete t; 
    }
};

template<typename T>
class CreateStatic
{
public:
    /**
     * @brief   最大的空间
     */ 
    union MaxAlign 
    { 
        char t_[sizeof(T)]; 
        long double longDouble_; 
    }; 

    /**
     * @brief   创建. 
     * 
     * @return T*
     */
    static T* create() 
    { 
        static MaxAlign t; 
        return new(&t) T; 
    }

    /**
     * @brief   释放. 
     *  
     * @param t
     */
    static void destroy(T *t) 
    {
        t->~T();
    }
};

////////////////////////////////////////////////////////////////
/**
 * @brief 定义LifetimePolicy:定义对象的声明周期管理
 */
template<typename T>
class DefaultLifetime
{
public:
    static void deadReference()
    {
        throw std::logic_error("singleton object has dead.");
    }

    static void scheduleDestruction(T*, void (*pFun)())
    {
        // 登记在程序退出时执行的函数（main函数完成之后执行）
        std::atexit(pFun);
    }
};

template<typename T>
class PhoneixLifetime
{
public:
    static void deadReference()
    {
        _bDestroyedOnce = true;
    }

    static void scheduleDestruction(T*, void (*pFun)())
    {
        if(!_bDestroyedOnce)
            std::atexit(pFun);
    }
private:
    static bool _bDestroyedOnce; 
};
template <class T> 
bool PhoneixLifetime<T>::_bDestroyedOnce = false; 

template <typename T> 
struct NoDestroyLifetime 
{ 
    static void scheduleDestruction(T*, void (*)()) 
    {
    } 

    static void deadReference() 
    {
    } 
}; 

//////////////////////////////////////////////////////////////////////
// Singleton
    // 模板的模板参数
template
<
    typename T,
        // 默认使用CreateUsingNew 和 DefaultLifetime
    template<class> class CreatePolicy   = CreateUsingNew,
    template<class> class LifetimePolicy = DefaultLifetime
>
class TC_Singleton 
{
public:
    typedef T  instance_type;
    typedef volatile T volatile_type;

    /**
     * @brief 获取实例
     * 
     * @return T*
     */
    static T *getInstance()
    {

        //加锁, 双check机制, 保证正确和效率
        // 只有在未创建实例的时候才会加锁 实例创建完成后就不用加锁了
        if(!_pInstance)
        {
            // 若无实例 加锁
            TC_ThreadLock::Lock lock(_tl);

            // 第二次判断是为了防止这种情况：
            // 第一个进程加锁之后 正在进行初始化
            // 而第二个进程此时进入了第一次判断 正在等待锁
            // 若没有第二次检查 第二个进程会在第一个进程释放锁之后又初始化一次
            if(!_pInstance)
            {
                // 若无实例
                if(_destroyed)
                {
                    // 若已经析构
                    LifetimePolicy<T>::deadReference();
                    _destroyed = false;
                }
                // 创建对象
                _pInstance = CreatePolicy<T>::create();
                LifetimePolicy<T>::scheduleDestruction((T*)_pInstance, &destroySingleton);
            }
        }
        return (T*)_pInstance;
    }
    
    virtual ~TC_Singleton(){}; 

protected:
    // 删除对象
    static void destroySingleton()
    {
        assert(!_destroyed);
        CreatePolicy<T>::destroy((T*)_pInstance);
        _pInstance = NULL;
        _destroyed = true;
    }
protected:
    // 锁对象
    static TC_ThreadLock    _tl;
    // 静态 对象的指针
    static volatile T*      _pInstance;
    static bool             _destroyed;

protected:
    TC_Singleton(){}
    TC_Singleton (const TC_Singleton &); 
    TC_Singleton &operator=(const TC_Singleton &);
};


// 初始化类的静态变量
template <class T, template<class> class CreatePolicy, template<class> class LifetimePolicy> 
TC_ThreadLock TC_Singleton<T, CreatePolicy, LifetimePolicy>::_tl; 


template <class T, template<class> class CreatePolicy, template<class> class LifetimePolicy> 
bool TC_Singleton<T, CreatePolicy, LifetimePolicy>::_destroyed = false; 

// 指针初始化为空
template <class T, template<class> class CreatePolicy, template<class> class LifetimePolicy> 
volatile T* TC_Singleton<T, CreatePolicy, LifetimePolicy>::_pInstance = NULL; 

}

#endif
