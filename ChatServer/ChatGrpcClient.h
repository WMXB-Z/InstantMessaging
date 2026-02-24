#pragma once
#include "const.h"
#include "Singleton.h"
#include "ConfigMgr.h"
#include "message.grpc.pb.h"
#include "message.pb.h"
#include <grpcpp/grpcpp.h>
#include <queue>
#include "data.h"
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include <condition_variable>
#include <string>
using grpc::Channel;
using grpc::Status;
using grpc::ClientContext;

using message::AddFriendReq;
using message::AddFriendRsp;
using message::AddFriendMsg;

using message::AuthFriendReq;
using message::AuthFriendRsp;

using message::GetChatServerRsp;
using message::LoginRsp;
using message::LoginReq;
using message::ChatService;

using message::TextChatMsgReq;
using message::TextChatMsgRsp;
using message::TextChatData;

using message::KickUserReq;
using message::KickUserRsp;
class ChatConPool {
public:
	ChatConPool(size_t poolsize, std::string host, std::string port) :b_stop_(false),poolSize_(poolsize),host_(host),port_(port) {
		for (int i = 0; i < poolSize_; ++i) {
			std::shared_ptr < Channel > channel = grpc::CreateChannel(host_+":"+port_,grpc::InsecureChannelCredentials());
			connections_.push(ChatService::NewStub(channel));
		}
	}
	~ChatConPool() {
		std::lock_guard<std::mutex> lock(mutex_);
		Close();
		while (!connections_.empty()) {
			connections_.pop();
		}
	}

	std::unique_ptr<ChatService::Stub> getConnection() {
		std::unique_lock<std::mutex> lock(mutex_);
		cond_.wait(lock,[this](){
			if (b_stop_) {
				return true;
			}
			return !connections_.empty();
			});

		if (b_stop_) {
			return nullptr;
		}
		auto context = std::move(connections_.front());
		connections_.pop();
		return context;
	}
	void returnConnection(std::unique_ptr<ChatService::Stub> stub) {
		std::lock_guard<std::mutex> lock(mutex_);
		if (b_stop_) {
			return;
		}
		connections_.push(std::move(stub));
		cond_.notify_one();
	}
	void Close() {
		b_stop_ = true;
		cond_.notify_all();
	}
private:
	std::atomic<bool> b_stop_;
	size_t poolSize_;
	std::string host_;
	std::string port_;
	std::mutex mutex_;
	std::condition_variable cond_;
	std::queue<std::unique_ptr<ChatService::Stub>> connections_;

};
class ChatGrpcClient:public Singleton<ChatGrpcClient>
{
	friend class Singleton<ChatGrpcClient>;
public:
	~ChatGrpcClient() {}
	//跨服转发“发起加好友申请”
	AddFriendRsp NotifyAddFriend(std::string server_ip, const AddFriendReq& req);
	//跨服转发“审核/同意/拒绝好友申请”。
	AuthFriendRsp NotifyAuthFriend(std::string server_ip, const AuthFriendReq& req);
	//查询用户基础信息
	bool GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userinfo);
	//跨服投递一条文本消息
	TextChatMsgRsp NotifyTextChatMsg(std::string server_ip, const TextChatMsgReq& req, const Json::Value& rtvalue);
	//跨服踢人
	KickUserRsp NotifyKickUser(std::string server_ip, const KickUserReq& req);
private:
	ChatGrpcClient();
	std::unordered_map<std::string, std::unique_ptr<ChatConPool>> _pools;
};

