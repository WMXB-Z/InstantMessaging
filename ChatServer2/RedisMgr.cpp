#include "RedisMgr.h"
#include "ConfigMgr.h"
#include "DistLock.h"
void RedisMgr::Close()
{
    _con_pool->Close();
}

RedisMgr::RedisMgr()
{
    auto& gCfgMgr = ConfigMgr::Inst();
    std::string host = gCfgMgr["Redis"]["Host"];
    std::string port = gCfgMgr["Redis"]["Port"];
    std::string pwd = gCfgMgr["Redis"]["Passwd"];
    /*std::cout << host << port << pwd<<std::endl;*/
    _con_pool.reset(new RedisConPool(10, host.c_str(), atoi(port.c_str()), pwd.c_str()));
}
RedisMgr::~RedisMgr()
{
    Close();
}

bool RedisMgr::Get(const std::string& key, std::string& value)
{
    auto connect = _con_pool->getConnection();
    if (connect == nullptr) {
        return false;
    }
    auto reply= (redisReply*)redisCommand(connect, "GET %s", key.c_str());
    if (reply == NULL) {
        std::cout << "[ GET  " << key << " ] failed" << std::endl;
        //freeReplyObject(reply);
        _con_pool->returnConnection(connect);
        return false;
    }

    if (reply->type != REDIS_REPLY_STRING) {
        std::cout << "[ GET  " << key << " ] failed" << std::endl;
        freeReplyObject(reply);
        _con_pool->returnConnection(connect);
        return false;
    }

    value = reply->str;
    freeReplyObject(reply);

    std::cout << "Succeed to execute command [ GET " << key << "  ]" << std::endl;
    _con_pool->returnConnection(connect);
    return true;
}
bool RedisMgr::Set(const std::string& key, const std::string& value) {
    //执行redis命令行
    auto connect = _con_pool->getConnection();
    if (connect == nullptr) {
        return false;
    }
    auto reply = (redisReply*)redisCommand(connect, "SET %s %s", key.c_str(), value.c_str());

    //如果返回NULL则说明执行失败
    if (NULL == reply)
    {
        std::cout << "Execut command [ SET " << key << "  " << value << " ] failure ! " << std::endl;
        //freeReplyObject(reply);
        _con_pool->returnConnection(connect);
        return false;
    }

    //如果执行失败则释放连接
    if (!(reply->type == REDIS_REPLY_STATUS && (strcmp(reply->str, "OK") == 0 || strcmp(reply->str, "ok") == 0)))
    {
        std::cout << "Execut command [ SET " << key << "  " << value << " ] failure ! " << std::endl;
        freeReplyObject(reply);
        _con_pool->returnConnection(connect);
        return false;
    }

    //执行成功 释放redisCommand执行后返回的redisReply所占用的内存
    freeReplyObject(reply);
    std::cout << "Execut command [ SET " << key << "  " << value << " ] success ! " << std::endl;
    _con_pool->returnConnection(connect);
    return true;
}

bool RedisMgr::LPush(const std::string& key, const std::string& value)
{
    auto connect = _con_pool->getConnection();
    if (connect == nullptr) {
        return false;
    }
    auto reply = (redisReply*)redisCommand(connect, "LPUSH %s %s", key.c_str(), value.c_str());
    if (NULL == reply)
    {
        std::cout << "Execut command [ LPUSH " << key << "  " << value << " ] failure ! " << std::endl;
        //freeReplyObject(reply);
        _con_pool->returnConnection(connect);
        return false;
    }

    if (reply->type != REDIS_REPLY_INTEGER || reply->integer <= 0) {
        std::cout << "Execut command [ LPUSH " << key << "  " << value << " ] failure ! " << std::endl;
        freeReplyObject(reply);
        _con_pool->returnConnection(connect);
        return false;
    }

    std::cout << "Execut command [ LPUSH " << key << "  " << value << " ] success ! " << std::endl;
    freeReplyObject(reply);
    _con_pool->returnConnection(connect);
    return true;
}
bool RedisMgr::LPop(const std::string& key, std::string& value) {
    auto connect = _con_pool->getConnection();
    if (connect == nullptr) {
        return false;
    }
    auto reply = (redisReply*)redisCommand(connect, "LPOP %s ", key.c_str());
    if (reply == nullptr || reply->type == REDIS_REPLY_NIL) {
        std::cout << "Execut command [ LPOP " << key << " ] failure ! " << std::endl;
        //freeReplyObject(reply);
        _con_pool->returnConnection(connect);
        return false;
    }
    value = reply->str;
    std::cout << "Execut command [ LPOP " << key << " ] success ! " << std::endl;
    freeReplyObject(reply);
    _con_pool->returnConnection(connect);
    return true;
}
bool RedisMgr::RPush(const std::string& key, const std::string& value) {
    auto connect = _con_pool->getConnection();
    if (connect == nullptr) {
        return false;
    }
    auto reply = (redisReply*)redisCommand(connect, "RPUSH %s %s", key.c_str(), value.c_str());
    if (NULL == reply)
    {
        std::cout << "Execut command [ RPUSH " << key << "  " << value << " ] failure ! " << std::endl;
        //freeReplyObject(reply);
        _con_pool->returnConnection(connect);
        return false;
    }

    if (reply->type != REDIS_REPLY_INTEGER || reply->integer <= 0) {
        std::cout << "Execut command [ RPUSH " << key << "  " << value << " ] failure ! " << std::endl;
        freeReplyObject(reply);
        _con_pool->returnConnection(connect);
        return false;
    }

    std::cout << "Execut command [ RPUSH " << key << "  " << value << " ] success ! " << std::endl;
    freeReplyObject(reply);
    _con_pool->returnConnection(connect);
    return true;
}
bool RedisMgr::RPop(const std::string& key, std::string& value) {
    auto connect = _con_pool->getConnection();
    if (connect == nullptr) {
        return false;
    }
    auto reply = (redisReply*)redisCommand(connect, "RPOP %s ", key.c_str());
    if (reply == nullptr || reply->type == REDIS_REPLY_NIL) {
        std::cout << "Execut command [ RPOP " << key << " ] failure ! " << std::endl;
        //freeReplyObject(reply);
        _con_pool->returnConnection(connect);
        return false;
    }
    value = reply->str;
    std::cout << "Execut command [ RPOP " << key << " ] success ! " << std::endl;
    freeReplyObject(reply);
    _con_pool->returnConnection(connect);
    return true;
}
bool RedisMgr::HSet(const std::string& key, const std::string& hkey, const std::string& value) {
    auto connect = _con_pool->getConnection();
    if (connect == nullptr) {
        return false;
    }
    auto reply = (redisReply*)redisCommand(connect, "HSET %s %s %s", key.c_str(), hkey.c_str(), value.c_str());
    if (reply == nullptr || reply->type != REDIS_REPLY_INTEGER) {
        std::cout << "Execut command [ HSet " << key << "  " << hkey << "  " << value << " ] failure ! " << std::endl;
        //freeReplyObject(reply);
        _con_pool->returnConnection(connect);
        return false;
    }
    std::cout << "Execut command [ HSet " << key << "  " << hkey << "  " << value << " ] success ! " << std::endl;
    freeReplyObject(reply);
    _con_pool->returnConnection(connect);
    return true;
}


bool RedisMgr::HSet(const char* key, const char* hkey, const char* hvalue, size_t hvaluelen)
{
    auto connect = _con_pool->getConnection();
    if (connect == nullptr) {
        return false;
    }
    const char* argv[4];
    size_t argvlen[4];
    argv[0] = "HSET";
    argvlen[0] = 4;
    argv[1] = key;
    argvlen[1] = strlen(key);
    argv[2] = hkey;
    argvlen[2] = strlen(hkey);
    argv[3] = hvalue;
    argvlen[3] = hvaluelen;
    
    auto reply = (redisReply*)redisCommandArgv(connect, 4, argv, argvlen);
    if (reply == nullptr || reply->type != REDIS_REPLY_INTEGER) {
        std::cout << "Execut command [ HSet " << key << "  " << hkey << "  " << hvalue << " ] failure ! " << std::endl;
        //freeReplyObject(reply);
        _con_pool->returnConnection(connect);
        return false;
    }
    std::cout << "Execut command [ HSet " << key << "  " << hkey << "  " << hvalue << " ] success ! " << std::endl;
    freeReplyObject(reply);
    _con_pool->returnConnection(connect);
    return true;
}
bool RedisMgr::HDel(const std::string& key, const std::string& field)
{
    auto connect = _con_pool->getConnection();
    if (connect == nullptr) {
        return false;
    }

    Defer defer([&connect, this]() {
        _con_pool->returnConnection(connect);
        });

    redisReply* reply = (redisReply*)redisCommand(connect, "HDEL %s %s", key.c_str(), field.c_str());
    if (reply == nullptr) {
        std::cerr << "HDEL command failed" << std::endl;
        return false;
    }

    bool success = false;
    if (reply->type == REDIS_REPLY_INTEGER) {
        success = reply->integer > 0;
    }

    freeReplyObject(reply);
    return success;
}
std::string RedisMgr::HGet(const std::string& key, const std::string& hkey)
{
    auto connect = _con_pool->getConnection();
    if (connect == nullptr) {
        return "";
    }
    const char* argv[3];
    size_t argvlen[3];
    argv[0] = "HGET";
    argvlen[0] = 4;
    argv[1] = key.c_str();
    argvlen[1] = key.length();
    argv[2] = hkey.c_str();
    argvlen[2] = hkey.length();
    
    auto reply = (redisReply*)redisCommandArgv(connect, 3, argv, argvlen);
    if (reply == nullptr || reply->type == REDIS_REPLY_NIL) {
        //freeReplyObject(reply);
        std::cout << "Execut command [ HGet " << key << " " << hkey << "  ] failure ! " << std::endl;
        _con_pool->returnConnection(connect);
        return "";
    }

    std::string value = reply->str;
    freeReplyObject(reply);
    std::cout << "Execut command [ HGet " << key << " " << hkey << " ] success ! " << std::endl;
    _con_pool->returnConnection(connect);
    return value;
}
bool RedisMgr::Del(const std::string& key)
{
    auto connect = _con_pool->getConnection();
    if (connect == nullptr) {
        return false;
    }
    auto reply = (redisReply*)redisCommand(connect, "DEL %s", key.c_str());
    if (reply == nullptr || reply->type != REDIS_REPLY_INTEGER) {
        std::cout << "Execut command [ Del " << key << " ] failure ! " << std::endl;
        //freeReplyObject(reply);
        _con_pool->returnConnection(connect);
        return false;
    }
    std::cout << "Execut command [ Del " << key << " ] success ! " << std::endl;
    freeReplyObject(reply);
    _con_pool->returnConnection(connect);
    return true;
}
bool RedisMgr::ExistsKey(const std::string& key)
{
    auto connect = _con_pool->getConnection();
    if (connect == nullptr) {
        return false;
    }
    auto reply = (redisReply*)redisCommand(connect, "exists %s", key.c_str());
    if (reply == nullptr || reply->type != REDIS_REPLY_INTEGER || reply->integer == 0) {
        std::cout << "Not Found [ Key " << key << " ]  ! " << std::endl;
        //freeReplyObject(reply);
        _con_pool->returnConnection(connect);
        return false;
    }
    std::cout << " Found [ Key " << key << " ] exists ! " << std::endl;
    freeReplyObject(reply);
    _con_pool->returnConnection(connect);
    return true;
}

std::string RedisMgr::acquireLock(const std::string& lockName,
    int lockTimeout, int acquireTimeout) {

    auto connect = _con_pool->getConnection();
    if (connect == nullptr) {
        return "";
    }

    Defer defer([&connect, this]() {
        _con_pool->returnConnection(connect);
        });

    return DistLock::Inst().acquireLock(connect, lockName, lockTimeout, acquireTimeout);
}
bool RedisMgr::releaseLock(const std::string& lockName,
    const std::string& identifier) {
    if (identifier.empty()) {
        return true;
    }
    auto connect = _con_pool->getConnection();
    if (connect == nullptr) {
        return false;
    }


    Defer defer([&connect, this]() {
        _con_pool->returnConnection(connect);
        });

    return DistLock::Inst().releaseLock(connect, lockName, identifier);
}


void RedisMgr::InitCount(std::string server_name) {
    auto lock_key = LOCK_COUNT;
    auto identifier = RedisMgr::GetInstance()->acquireLock(lock_key, LOCK_TIME_OUT, ACQUIRE_TIME_OUT);
    //利用defer解锁
    Defer defer2([this, identifier, lock_key]() {
        RedisMgr::GetInstance()->releaseLock(lock_key, identifier);
        });

    RedisMgr::GetInstance()->HSet(LOGIN_COUNT, server_name, "0");
}

void RedisMgr::DelCount(std::string server_name) {
    auto lock_key = LOCK_COUNT;
    auto identifier = RedisMgr::GetInstance()->acquireLock(lock_key, LOCK_TIME_OUT, ACQUIRE_TIME_OUT);
    //利用defer解锁
    Defer defer2([this, identifier, lock_key]() {
        RedisMgr::GetInstance()->releaseLock(lock_key, identifier);
        });

    RedisMgr::GetInstance()->HDel(LOGIN_COUNT, server_name);
}


RedisConPool::RedisConPool(size_t poolSize, const char* host, int port, const char* pwd)
    : poolSize_(poolSize), host_(host), port_(port), b_stop_(false), pwd_(pwd),
    counter_(0), fail_count_(0) {
    for (size_t i = 0; i < poolSize_; i++) {
        auto* context = redisConnect(host_, port_);
        if (context == nullptr || context->err != 0) {
            if (context != nullptr) {
                redisFree(context);
            }
            continue;
        }
        auto reply = (redisReply*)redisCommand(context, "AUTH %s", pwd);
        if (reply->type == REDIS_REPLY_ERROR) {
            std::cout << "认证失败" << std::endl;
            freeReplyObject(reply);
            continue;
        }
        freeReplyObject(reply);
        std::cout << "认证成功" << std::endl;
        connections_.push(context);
    }
    check_thread_ = std::thread([this]() {
        while (!b_stop_) {
            counter_++;
            if (counter_ >= 60) {
                checkThreadPro();
                counter_ = 0;
            }

            std::this_thread::sleep_for(std::chrono::seconds(1)); // 每隔 30 秒发送一次 PING 命令
        }
        });
}

RedisConPool::~RedisConPool()
{
    Close();
    ClearConnections();
}
void RedisConPool::ClearConnections() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!connections_.empty()) {
        auto* context = connections_.front();
        redisFree(context);
        connections_.pop();
    }
}
redisContext* RedisConPool::getConnection()
{
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait(lock, [this] {
        if (b_stop_) {
            return true;
        }
        return !connections_.empty();
        });
    //如果停止则直接返回空指针
    if (b_stop_) {
        return  nullptr;
    }
    auto* context = connections_.front();
    connections_.pop();
    return context;
}
redisContext* RedisConPool::getConNonBlock() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (b_stop_) {
        return nullptr;
    }

    if (connections_.empty()) {
        return nullptr;
    }

    auto* context = connections_.front();
    connections_.pop();
    return context;
}

void RedisConPool::returnConnection(redisContext* context)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (b_stop_) {
        return;
    }
    connections_.push(context);
    cond_.notify_one();
}

void RedisConPool::Close()
{
    b_stop_ = true;
    cond_.notify_all();
    if (check_thread_.joinable()) {
        check_thread_.join();
    }
}

bool RedisConPool::reconnect() {
    auto context = redisConnect(host_, port_);
    if (context == nullptr || context->err != 0) {
        if (context != nullptr) {
            redisFree(context);
        }
        return false;
    }

    auto reply = (redisReply*)redisCommand(context, "AUTH %s", pwd_);
    if (reply->type == REDIS_REPLY_ERROR) {
        std::cout << "认证失败" << std::endl;
        freeReplyObject(reply);
        redisFree(context);
        return false;
    }

    freeReplyObject(reply);
    std::cout << "认证成功" << std::endl;
    returnConnection(context);
    return true;
}

void RedisConPool::checkThreadPro() {
    size_t pool_size;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pool_size = connections_.size();
    }

    for (size_t i = 0; i < pool_size && !b_stop_; ++i) {
        redisContext* context = nullptr;
        context = getConNonBlock();
        if (context == nullptr) {
            break;
        }

        redisReply* reply = nullptr;
        try {
            reply = (redisReply*)redisCommand(context, "PING");

            if (context->err) {
                std::cout << "Connection error: " << context->err << std::endl;
                if (reply) {
                    freeReplyObject(reply);
                }
                redisFree(context);
                fail_count_++;
                continue;
            }

            if (!reply || reply->type == REDIS_REPLY_ERROR) {
                std::cout << "reply is null, redis ping failed" << std::endl;
                if (reply) {
                    freeReplyObject(reply);
                }
                redisFree(context);
                fail_count_++;
                continue;
            }

            freeReplyObject(reply);
            returnConnection(context);
        }
        catch (std::exception& exp) {
            if (reply) {
                freeReplyObject(reply);
            }
            redisFree(context);
            fail_count_++;
        }
    }

    // 执行重连操作
    while (fail_count_ > 0) {
        auto res = reconnect();
        if (res) {
            fail_count_--;
        }
        else {
            break;
        }
    }
}

void RedisConPool::checkThread() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (b_stop_) {
        return;
    }

    auto pool_size = connections_.size();
    for (size_t i = 0; i < pool_size && !b_stop_; i++) {
        auto* context = connections_.front();
        connections_.pop();
        try {
            auto reply = (redisReply*)redisCommand(context, "PING");
            if (!reply) {
                std::cout << "reply is null, redis ping failed" << std::endl;
                connections_.push(context);
                continue;
            }
            freeReplyObject(reply);
            connections_.push(context);
        }
        catch (std::exception& exp) {
            std::cout << "Error keeping connection alive: " << exp.what() << std::endl;
            redisFree(context);
            context = redisConnect(host_, port_);
            if (context == nullptr || context->err != 0) {
                if (context != nullptr) {
                    redisFree(context);
                }
                continue;
            }

            auto reply = (redisReply*)redisCommand(context, "AUTH %s", pwd_);
            if (reply->type == REDIS_REPLY_ERROR) {
                std::cout << "认证失败" << std::endl;
                freeReplyObject(reply);
                continue;
            }

            freeReplyObject(reply);
            std::cout << "认证成功" << std::endl;
            connections_.push(context);
        }
    }
}

