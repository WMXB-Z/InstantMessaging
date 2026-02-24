#pragma once
#include "const.h"
class RedisConPool {
public:
    RedisConPool(size_t poolsize, const char* host, int port, const char* pwd);
    ~RedisConPool();

    void ClearConnections();
    redisContext* getConnection();
    redisContext* getConNonBlock();
    void returnConnection(redisContext* context);
    void Close();
private:
    bool reconnect();
    void checkThreadPro();
    void checkThread();
private:
    std::atomic<bool> b_stop_;
    size_t poolSize_;
    const char* host_;
    const char* pwd_;
    int port_;
    std::queue<redisContext*> connections_;
    std::atomic<int> fail_count_;
    std::mutex mutex_;
    std::condition_variable cond_;
    std::thread check_thread_;
    std::atomic<int> counter_;
};
class RedisMgr : public Singleton<RedisMgr>,
    public std::enable_shared_from_this<RedisMgr>
{
    friend class Singleton<RedisMgr>;
public:
    ~RedisMgr();
    bool Get(const std::string& key, std::string& value);
    bool Set(const std::string& key, const std::string& value);
    bool LPush(const std::string& key, const std::string& value);
    bool LPop(const std::string& key, std::string& value);
    bool RPush(const std::string& key, const std::string& value);
    bool RPop(const std::string& key, std::string& value);
    bool HSet(const std::string& key, const std::string& hkey, const std::string& value);
    bool HSet(const char* key, const char* hkey, const char* hvalue, size_t hvaluelen);
    bool HDel(const std::string& key, const std::string& field);
    std::string HGet(const std::string& key, const std::string& hkey);
    bool Del(const std::string& key);
    bool ExistsKey(const std::string& key);
    void Close();
    std::string acquireLock(const std::string& lockName, int lockTimeout, int acquireTimeout);
    bool releaseLock(const std::string& lockName, const std::string& identifier);
    void InitCount(std::string server_name);
    void DelCount(std::string server_name);
private:
    RedisMgr();

    std::unique_ptr<RedisConPool> _con_pool;
};
