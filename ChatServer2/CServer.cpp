#include "CServer.h"
#include "AsioIOServicePool.h"
#include "UserMgr.h"
#include "RedisMgr.h"
#include "ConfigMgr.h"
//构造一个 TCP 端点（endpoint = IP地址 + 端口），地址用 IPv4 的“任意地址”，端口用你给的 port
//0.0.0.0:port
CServer::CServer(boost::asio::io_context& io_context, short port):_io_context(io_context),
_port(port),_acceptor(io_context, tcp::endpoint(tcp::v4(), port)),_timer(io_context,std::chrono::seconds(60))
{
	std::cout << "Server start success, listen on port: " << _port << std::endl;
	StartAccept();
}

CServer::~CServer()
{	
	std::cout << "Server destruct listen on port: " << _port << std::endl;
	
}

void CServer::ClearSession(std::string session_id)
{
	std::lock_guard<std::mutex> lock(_mutex);
	if (_sessions.find(session_id) != _sessions.end()) {
		UserMgr::GetInstance()->RmvUserSession(_sessions[session_id]->GetUserId(),session_id);
	}
	_sessions.erase(session_id);
	
}

//根据用户获取session
//std::shared_ptr<CSession> CServer::GetSession(std::string uuid) {
//	std::lock_guard<std::mutex> lock(_mutex);
//	auto it = _sessions.find(uuid);
//	if (it != _sessions.end()) {
//		return it->second;
//	}
//	return nullptr;
//}

bool CServer::CheckValid(std::string session_id)
{
	std::lock_guard<std::mutex> lock(_mutex);
	auto it = _sessions.find(session_id);
	if (it != _sessions.end()) {
		return true;
	}
	return false;
}

void CServer::HandleAccept(std::shared_ptr<CSession> new_session, const boost::system::error_code& error)
{
	if (!error) {
		new_session->Start();//该函数的执行都是主线程执行，Start() 内部“发起的异步读写”会在哪个线程执行，取决于它用哪个 executor/io_context 来发起。
		std::lock_guard<std::mutex> lock(_mutex);
		_sessions.insert(make_pair(new_session->GetSessionId(), new_session));
	}
	else {
		std::cout << "session accept failed,error is" << error.what() << std::endl;
	}
	StartAccept();
}

void CServer::StartAccept()
{
	auto& io_context = AsioIOServicePool::GetInstance()->GetIOService();
	std::shared_ptr<CSession> new_session = std::make_shared<CSession>(io_context, this);
	//acceptor 负责“新连接到来时，把连接绑定到 new_session->GetSocket() 这个 socket 上”
	//然后调用回调 HandleAccept(...)
	_acceptor.async_accept(new_session->GetSocket(), std::bind(&CServer::HandleAccept, this, new_session, std::placeholders::_1));
}


void CServer::on_timer(const boost::system::error_code& ec) {
	if (ec) {
		std::cout << "timer error: " << ec.message() << std::endl;
		return;
	}
	std::vector<std::shared_ptr<CSession>> _expired_sessions;
	int session_count = 0;
	//此处加锁遍历session
	//为什么要在这里设置session_copy  允许心跳没有较强的时效性
	std::map<std::string, std::shared_ptr<CSession>> sessions_copy;
	{
		std::lock_guard<std::mutex> lock(_mutex);
		sessions_copy = _sessions;
	}

	time_t now = std::time(nullptr);
	for (auto iter = sessions_copy.begin(); iter != sessions_copy.end(); iter++) {
		auto b_expired = iter->second->IsHeartbeatExpired(now);
		if (b_expired) {
			//关闭socket, 其实这里也会触发async_read的错误处理
			iter->second->Close();
			//收集过期信息
			_expired_sessions.push_back(iter->second);
			continue;
		}
		session_count++;
	}

	//设置session数量
	auto& cfg = ConfigMgr::Inst();
	auto self_name = cfg["SelfServer"]["Name"];
	auto count_str = std::to_string(session_count);
	RedisMgr::GetInstance()->HSet(LOGIN_COUNT, self_name, count_str);

	//处理过期session, 单独提出，防止死锁
	for (auto& session : _expired_sessions) {
		session->DealExceptionSession();
	}

	//再次设置，下一个60s检测
	_timer.expires_after(std::chrono::seconds(60));
	_timer.async_wait([this](boost::system::error_code ec) {
		on_timer(ec);
		});
}

void CServer::StartTimer()
{
	//启动定时器
	auto self(shared_from_this());
	_timer.async_wait([self](boost::system::error_code ec) {
		self->on_timer(ec);
		});
}

void CServer::StopTimer()
{
	_timer.cancel();
}
