#include "CSession.h"
#include <iostream>
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include "LogicSystem.h"
#include "CServer.h"
#include "RedisMgr.h"
CSession::CSession(boost::asio::io_context& io_context, CServer* server) :
    _socket(io_context), _server(server), _b_close(false), _b_head_parse(false) 
{
    boost::uuids::uuid a_uuid = boost::uuids::random_generator()();
    _session_id = boost::uuids::to_string(a_uuid);
    _recv_head_node = std::make_shared<MsgNode>(HEAD_TOTAL_LEN);
}


CSession::~CSession()
{
    std::cout << "~ CSession destruction" << std::endl;
}

tcp::socket& CSession::GetSocket()
{
    return _socket;
}

std::string& CSession::GetSessionId()
{
    return _session_id;
}

void CSession::SetUserId(int uid)
{
    _user_uid = uid;
}

int CSession::GetUserId()
{
    return _user_uid;
}

void CSession::Start()
{
    AsyncReadHead(HEAD_TOTAL_LEN);
}

void CSession::Send(char* msg, short max_length, short msgid)
{
    std::lock_guard<std::mutex> lock(_send_lock);
    int send_que_size = _send_que.size();
    if (send_que_size > MAX_SENDQUE) {
        std::cout << "session:  " << _session_id<<"send que fulled " << std::endl;
        return;
    }
    _send_que.push(std::make_shared<SendNode>(msg, max_length, msgid));//maxlen是数据的长度 不包括头部
    if (send_que_size > 0) {
        return;
    }
    auto& msgnode = _send_que.front();
    boost::asio::async_write(_socket, boost::asio::buffer(msgnode->_data, msgnode->_total_len),
        std::bind(&CSession::HandleWrite, this, std::placeholders::_1, SharedSelf()));
}

void CSession::Send(std::string msg, short msgid)
{
    std::lock_guard<std::mutex> lock(_send_lock);
    int send_que_size = _send_que.size();
    if (send_que_size > MAX_SENDQUE) {
        std::cout << "session:  " << _session_id << "send que fulled " << std::endl;
        return;
    }
    _send_que.push(std::make_shared<SendNode>(msg.c_str(), msg.length(), msgid));
    if (send_que_size > 0) {
        return;
    }
    auto& msgnode = _send_que.front();
    boost::asio::async_write(_socket, boost::asio::buffer(msgnode->_data, msgnode->_total_len),
        std::bind(&CSession::HandleWrite, this, std::placeholders::_1, SharedSelf()));
}

void CSession::Close()
{
    std::lock_guard<std::mutex> lock(_session_mtx);
    _socket.close();
    _b_close = true;
}

std::shared_ptr<CSession> CSession::SharedSelf()
{
    return shared_from_this();
}

void CSession::AsyncReadBody(int length)
{
    auto self = shared_from_this();
    asyncReadFull(length, [self, this, length](const boost::system::error_code& ec, std::size_t bytes_transfered) {
        try {
            if (ec) {
                std::cout << "handle read failed error is " << ec.what() << std::endl;
                Close();
                //加分布式锁清除session
                DealExceptionSession();
                return;
            }
            if (bytes_transfered < length) {//这个条件不会触发
                std::cout << "read length not match read [" << bytes_transfered << "], total [" << HEAD_TOTAL_LEN << "]" << std::endl;
                Close();
                _server->ClearSession(_session_id);
                return;
            }
            if (!_server->CheckValid(_session_id)) {
                Close();
                return;
            }
            memcpy(_recv_msg_node->_data, _data, bytes_transfered);
            _recv_msg_node->_cur_len += bytes_transfered;
            _recv_msg_node->_data[_recv_msg_node->_total_len] = '\0';
            std::cout << "receive data is" << _recv_msg_node->_data << std::endl;

            LogicSystem::GetInstance()->PostMsgToQue(std::make_shared<LogicNode>(self, _recv_msg_node));
            AsyncReadHead(HEAD_TOTAL_LEN);
            UpdateHeartbeat();
        }
        catch (std::exception& e) {
            std::cout << "Exception code is" << e.what() << std::endl;
        }
        });
}
//head ：  msg_id   msg_len
void CSession::AsyncReadHead(int total_len)
{
    auto self = shared_from_this();//与其他共享指针共享引用计数
    //捕获 self 的目的：保证回调执行时对象还活着
    //捕获 this 只是为了回调内部可以直接写：_recv_node / _socket / AsyncReadBody(...) 之类的成员
    asyncReadFull(HEAD_TOTAL_LEN, [self, this](const boost::system::error_code& ec, std::size_t bytes_transfered) {
        try {
            if (ec) {
                std::cout << "handle read failed error is " << ec.what() << std::endl;
                Close();
                //加分布式锁清除session
                DealExceptionSession();
                return;
            }
            if (bytes_transfered < HEAD_TOTAL_LEN) {//这个条件不会触发
                std::cout << "read length not match read [" << bytes_transfered << "], total [" << HEAD_TOTAL_LEN << "]" << std::endl;
                Close();
                _server->ClearSession(_session_id);
                return;
            }

            if (!_server->CheckValid(_session_id)) {
                Close();
                return;
            }

            _recv_head_node->Clear();
            memcpy(_recv_head_node->_data, _data, bytes_transfered);

            short msg_id = 0;
            memcpy(&msg_id, _recv_head_node->_data, HEAD_ID_LEN);
            //网络字节序 转化为本地字节序
            msg_id = boost::asio::detail::socket_ops::network_to_host_short(msg_id);
            std::cout << "msg_id is " << msg_id << std::endl;
            if (msg_id > MAX_LENGTH) {
                std::cout << "invalid data length is " << msg_id;
                _server->ClearSession(_session_id);
                return;
            }
            short msg_len = 0;
            memcpy(&msg_len, _recv_head_node->_data + HEAD_ID_LEN, HEAD_DATA_LEN);
            msg_len = boost::asio::detail::socket_ops::network_to_host_short(msg_len);
            std::cout << "msg_len is " << msg_len << std::endl;
            if (msg_len > MAX_LENGTH) {
                std::cout << "invalid data length is " << msg_len;
                _server->ClearSession(_session_id);
                return;
            }

            _recv_msg_node = std::make_shared<RecvNode>(msg_len, msg_id);
            AsyncReadBody(msg_len);
            UpdateHeartbeat();
        }
        catch (std::exception& e) {
            std::cout << "Execption code is " << e.what() << std::endl;
        }
        
        });

}
void CSession::NotifyOffline(int uid)
{
    Json::Value  rtvalue;
    rtvalue["error"] = ErrorCodes::Success;
    rtvalue["uid"] = uid;


    std::string return_str = rtvalue.toStyledString();

    Send(return_str, ID_NOTIFY_OFF_LINE_REQ);
    return;
}

bool CSession::IsHeartbeatExpired(std::time_t& now) {
    double diff_sec = std::difftime(now, _last_heartbeat);
    if (diff_sec > HEART_THRESHOLD) {
        std::cout << "heartbeat expired, session id is  " << _session_id << std::endl;
        return true;
    }

    return false;
}

void CSession::UpdateHeartbeat()
{
    time_t now = std::time(nullptr);
    _last_heartbeat = now;
}

//读取完整长度  为什么要分成两个函数？
void CSession::asyncReadFull(std::size_t maxLength, std::function<void(const boost::system::error_code&, std::size_t)> handler)
{
    memset(_data, 0, MAX_LENGTH);
    asyncReadLen(0, maxLength, handler);
}
//读取指定字节数
void CSession::asyncReadLen(std::size_t read_len, std::size_t total_len, 
    std::function<void(const boost::system::error_code&, std::size_t)> handler)
{
    auto self = shared_from_this();
    //从 socket 读一部分数据（有多少读多少，但不会超过你提供的 buffer 容量）
    _socket.async_read_some(boost::asio::buffer(_data + read_len, total_len - read_len),
        [read_len,total_len,handler,self](const boost::system::error_code& ec, std::size_t bytes_transfered) {
            //这里捕获session  就算Clear连接 也只是引用计数-1；
            if (ec) {
                handler(ec, read_len + bytes_transfered);
                return;
            }
            if (read_len + bytes_transfered >= total_len) {//不会大于
                //长度够了 就调用回调函数
                handler(ec, read_len + bytes_transfered);
                return;
            }
            self->asyncReadLen(read_len + bytes_transfered, total_len, handler);
        });
}

void CSession::HandleWrite(const boost::system::error_code& error, std::shared_ptr<CSession> shared_self)
{
    try {
        if (!error) {
            std::lock_guard<std::mutex> lock(_send_lock);
            _send_que.pop();
            if (!_send_que.empty()) {
                auto& msgnode = _send_que.front();
                boost::asio::async_write(_socket, boost::asio::buffer(msgnode->_data, msgnode->_total_len),
                    std::bind(&CSession::HandleWrite, this, std::placeholders::_1, SharedSelf()));
            }
        }
        else {
            std::cout << "handle write failed ,error is " << error.what() << std::endl;
            Close();
            DealExceptionSession();
        }
    }
    catch (std::exception &e) {
        std::cout << "Exception code :" << e.what() << std::endl;
    }
}

LogicNode::LogicNode(std::shared_ptr<CSession>session, std::shared_ptr<RecvNode>recvnode):_session(session),_recvnode(recvnode)
{

}
void CSession::DealExceptionSession()
{
    auto self = shared_from_this();
    //加锁清除session
    auto uid_str = std::to_string(_user_uid);
    auto lock_key = LOCK_PREFIX + uid_str;
    auto identifier = RedisMgr::GetInstance()->acquireLock(lock_key, LOCK_TIME_OUT, ACQUIRE_TIME_OUT);
    Defer defer([identifier, lock_key, self, this]() {
        _server->ClearSession(_session_id);
        RedisMgr::GetInstance()->releaseLock(lock_key, identifier);
        });

    if (identifier.empty()) {
        return;
    }
    std::string redis_session_id = "";
    auto bsuccess = RedisMgr::GetInstance()->Get(USER_SESSION_PREFIX + uid_str, redis_session_id);
    if (!bsuccess) {
        return;
    }

    if (redis_session_id != _session_id) {
        //说明有客户在其他服务器异地登录了
        return;
    }
    //清除用户连接信息
    RedisMgr::GetInstance()->Del(USER_SESSION_PREFIX + uid_str);
    //清除用户登录信息
    RedisMgr::GetInstance()->Del(USERIPPREFIX + uid_str);
    //清除用户token信息
    std::string token_key = USERTOKENPREFIX + uid_str;
    RedisMgr::GetInstance()->Del(token_key);
}