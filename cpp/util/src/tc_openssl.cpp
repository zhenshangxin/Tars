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

#if TARS_SSL

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "util/tc_openssl.h"
#include "util/tc_buffer.h"


namespace tars
{

TC_OpenSSL::~TC_OpenSSL()
{
    Release();
}

void TC_OpenSSL::Release()
{
    // 释放SSL对象
    if (_ssl)
    {
        SSL_free(_ssl);
        _ssl = NULL;
    }
    _bHandshaked = false;
    _err = 0;
}

// 通过一个SSL对象来初始化
void TC_OpenSSL::Init(SSL* ssl, bool isServer)
{
    assert (_ssl == NULL);
    _ssl = ssl;
    _bHandshaked = false;
    _isServer = isServer;
    _err = 0;
}

bool TC_OpenSSL::IsHandshaked() const
{
    return _bHandshaked;
}

bool TC_OpenSSL::HasError() const
{
    return _err != 0;
}
    
string* TC_OpenSSL::RecvBuffer()
{
    return &_plainBuf;
}

std::string TC_OpenSSL::DoHandshake(const void* data, size_t size)
{
    assert (!_bHandshaked);
    assert (_ssl);

    if (data && size)
    {
        // 写入ssl内存缓冲区
        // 将加密的数据写入 获取用来读的bio（input bio）
        BIO_write(SSL_get_rbio(_ssl), data, size);
    }

    ERR_clear_error();

    // 如果是server就accept 如果是client就connect

    // SSL_connect 和一个server初始化TLS/SSL握手
    // 如果底层的BIO是阻塞的，那么SSL connect只会再出错或者握手成功的时候返回
    int ret = _isServer ? SSL_accept(_ssl) : SSL_connect(_ssl);

    if (ret <= 0)
    {
        // 出错
        _err = SSL_get_error(_ssl, ret);
        // SSL_ERROR_WANT_READ 底层BIO有数据需要被读取
        if (_err != SSL_ERROR_WANT_READ)
        {

            return std::string();
        }
    }

    _err = 0;

    if (ret == 1)
    {
        _bHandshaked = true;
    }


    std::string out;
    TC_Buffer outdata;
    // 从写bio中读出数据 并放到out中
    GetMemData(SSL_get_wbio(_ssl), outdata);
    if (!outdata.IsEmpty()) 
    {
        out.assign(outdata.ReadAddr(), outdata.ReadableSize());
    }

    return out;
}

std::string TC_OpenSSL::Write(const void* data, size_t size)
{
    if (!_bHandshaked)
        return std::string((const char*)data, size); //握手数据不用加密
 
    // 会话数据需加密
    ERR_clear_error();

    // 写入到SSL中
    int ret = SSL_write(_ssl, data, size); 
    if (ret <= 0) 
    {
        _err = SSL_get_error(_ssl, ret);
        return std::string();
    }

    _err = 0;

    // 从写bio中读
    TC_Buffer toSend; 
    GetMemData(SSL_get_wbio(_ssl), toSend);
    return std::string(toSend.ReadAddr(), toSend.ReadableSize());
}

bool TC_OpenSSL::Read(const void* data, size_t size, std::string& out)
{
    bool usedData = false;
    if (!_bHandshaked)
    {
        // 未握手
        usedData = true;

        // 清空缓存
        _plainBuf.clear();

        // 先握手
        std::string out2 = DoHandshake(data, size);

        // 返回握手的回复
        out.swap(out2);

        if (_err != 0)
            return false;

        if (_bHandshaked)
            ; // TODO onHandshake
    }

    // 不要用else，因为数据可能紧跟着最后的握手而来
    if (_bHandshaked)
    {
        if (!usedData)
        {
            // 写入ssl内存缓冲区
            // 将数据写入到ssl的读bio中
            BIO_write(SSL_get_rbio(_ssl), data, size);
        }

        // 从ssl的写bio中将数据读出
        string data;
        if (DoSSLRead(_ssl, data))
        {
            _plainBuf.append(data.begin(), data.end());
        }
        else
        {
            _err = SSL_ERROR_SSL;
            return false;
        }
    }

    return true;
}

} // end namespace tars

#endif

