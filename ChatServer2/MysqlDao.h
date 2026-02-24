#pragma once
#include "const.h"
#include "data.h"
#include <jdbc/mysql_connection.h>
#include <jdbc/mysql_driver.h>
#include <jdbc/cppconn/prepared_statement.h>
#include <jdbc/cppconn/resultset.h>
#include <jdbc/cppconn/statement.h>
#include <jdbc/cppconn/exception.h>
//MySQL Connector/C++ 官方库的接口设计上“借鉴”了 Java JDBC 的命名和用法
class SqlConnection {
public:
    SqlConnection(sql::Connection* con,int64_t lasttime):_con(con),_last_oper_time(lasttime){}
    std::unique_ptr<sql::Connection> _con;
    int64_t _last_oper_time;

};
class MySqlPool {
public:
    MySqlPool(const std::string& url, const std::string& user, const std::string& pass, const std::string& schema, int poolSize)
        : url_(url), user_(user), pass_(pass), schema_(schema), poolSize_(poolSize), b_stop_(false),_fail_count(0) {
        try {
            for (int i = 0; i < poolSize; i++) {
                sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
                auto* con = driver->connect(url_, user_, pass_);
                con->setSchema(schema_);
                auto  currentTime = std::chrono::system_clock::now().time_since_epoch();
                long long timestamp = std::chrono::duration_cast<std::chrono::seconds>(currentTime).count();
                pool_.push(std::make_unique<SqlConnection>(con, timestamp));
            }

            _check_thread = std::thread([this]() {
                int count = 0;
                while (!b_stop_) {
                    if (count >= 60) {
                        checkConnectionPro();
                        count = 0;
                    }
                    count++;
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
                });
            _check_thread.detach();
        }
        catch (sql::SQLException& e) {
            std::cout << "mysql pool init failed,error is " << e.what() << std::endl;
        }
    }
    void checkConnection() {
        std::lock_guard<std::mutex> guard(mutex_);
        int poolsize = pool_.size();//size必须提前取出来 因为pop时候会变
        auto  currentTime = std::chrono::system_clock::now().time_since_epoch();
        long long timestamp = std::chrono::duration_cast<std::chrono::seconds>(currentTime).count();
        for (int i = 0; i < poolsize; i++) {
            auto con = std::move(pool_.front());
            pool_.pop();
            Defer defer([this, &con]() {//每次循环结束之后 会执行Defer的析构，析构调用lamada表达式
                pool_.push(std::move(con));
                });
            if (timestamp - con->_last_oper_time < 5) {//小于5秒 就不重新连接  大于就有被断开的风险 主动发送查询给mysql说一声我还在
                continue;
            }
            try {
                std::unique_ptr<sql::Statement>stmt(con->_con->createStatement());
                stmt->executeQuery("SELECT 1");
                con->_last_oper_time = timestamp;
                //std::cout << "execute timer alive query , cur is " << timestamp << std::endl;
            }
            catch (sql::SQLException& e) {
                std::cout << "Error keeping connection alive:" << e.what() << std::endl;
                sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
                auto* newcon = driver->connect(url_, user_, pass_);
                newcon->setSchema(schema_);
                con->_con.reset(newcon);
                con->_last_oper_time = timestamp;

            }
        }
    }
    void checkConnectionPro() {
        // 1)先读取“目标处理数”
        size_t targetCount;
        {
            std::lock_guard<std::mutex> guard(mutex_);
            targetCount = pool_.size();
        }

        //2 当前已经处理的数量
        size_t processed = 0;

        //3 时间戳
        auto now = std::chrono::system_clock::now().time_since_epoch();
        long long timestamp = std::chrono::duration_cast<std::chrono::seconds>(now).count();//转换为秒

        while (processed < targetCount) {
            std::unique_ptr<SqlConnection> con;
            {
                std::lock_guard<std::mutex> guard(mutex_);
                if (pool_.empty()) {
                    break;
                }
                con = std::move(pool_.front());
                pool_.pop();
            }

            bool healthy = true;
            //解锁后做检查/重连逻辑
            if (timestamp - con->_last_oper_time >= 5) {
                try {
                    std::unique_ptr<sql::Statement> stmt(con->_con->createStatement());
                    stmt->executeQuery("SELECT 1");
                    con->_last_oper_time = timestamp;
                }
                catch (sql::SQLException& e) {
                    std::cout << "Error keeping connection alive: " << e.what() << std::endl;
                    healthy = false;
                    _fail_count++;
                }

            }

            if (healthy)
            {
                std::lock_guard<std::mutex> guard(mutex_);
                pool_.push(std::move(con));
                cond_.notify_one();
            }

            ++processed;
        }
        //根据失败的连接数量，重新创建新的对应数量的连接
        while (_fail_count > 0) {
            auto b_res = reconnect(timestamp);
            if (b_res) {
                _fail_count--;
            }
            else {
                break;
            }
        }

    }
    bool reconnect(long long timestamp) {
        try {
            sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
            auto* con = driver->connect(url_, user_, pass_);
            con->setSchema(schema_);

            auto newCon = std::make_unique<SqlConnection>(con, timestamp);
            {
                std::lock_guard<std::mutex> guard(mutex_);
                pool_.push(std::move(newCon));
            }
            std::cout << "mysql connection reconnect success" << std::endl;
            return true;
        }
        catch (sql::SQLException& e) {
            std::cout << "Reconnect failed, error is " << e.what() << std::endl;
            return false;
        }
    }
    std::unique_ptr<SqlConnection> getConnection() {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this]() {
            if (b_stop_) {
                return true;
            }
            return !pool_.empty();
            });
        if (b_stop_) {
            return nullptr;
        }
        std::unique_ptr<SqlConnection> con(std::move(pool_.front()));
        pool_.pop();
        return con;
    }
    void returnConnection(std::unique_ptr<SqlConnection> con) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (b_stop_) {
            return;
        }
        pool_.push(std::move(con));
        cond_.notify_one();
    }
    void Close() {
        b_stop_ = true;
        cond_.notify_all();
    }
    ~MySqlPool() {
        std::unique_lock<std::mutex> lock(mutex_);
        while (!pool_.empty()) {
            pool_.pop();
        }
    }
private:
    std::string url_;
    std::string user_;
    std::string pass_;
    std::string schema_;
    int poolSize_;
    std::queue<std::unique_ptr<SqlConnection>> pool_;
    std::mutex mutex_;
    std::condition_variable cond_;
    std::atomic<bool> b_stop_;
    std::thread _check_thread;//心跳机制
    //int _fail_count;
    std::atomic<int> _fail_count;
};

//struct UserInfo {
//    std::string name;
//    std::string pwd;
//    int uid;
//    std::string email;
//};
class MysqlDao
{
public:
    MysqlDao();
    ~MysqlDao();
    int RegUser(const std::string& name, const std::string& email, const std::string& pwd);
    bool CheckEmail(const std::string& name, const std::string& email);
    bool UpdatePwd(const std::string& name, const std::string& newpwd);
    bool CheckPwd(const std::string& name, const std::string& pwd,UserInfo& userInfo);
    std::shared_ptr<UserInfo> GetUser(int uid);
    std::shared_ptr<UserInfo> GetUser(std::string name);
    bool AddFriendApply(const int& from, const int& to);
    bool AuthFriendApply(const int& from, const int& to);
    bool AddFriend(const int& from, const int& to, std::string back_name);
    bool GetApplyList(int touid, std::vector<std::shared_ptr<ApplyInfo>>& applyList, int offset, int limit);
    bool GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo> >& user_info);
private:
    std::unique_ptr<MySqlPool> pool_;
};
