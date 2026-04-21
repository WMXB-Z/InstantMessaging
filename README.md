# 分布式即时通讯 IM 系统（Qt 客户端 + C++ 后端微服务）

## 项目概述

==**基于 Qt 的类微信 IM 客户端 + C++ Asio 高并发聊天后端，采用微服务（gRPC）与连接池/线程池支撑高并发长连接与稳定消息转发。**==

独立完成了一套全栈即时通讯系统，客户端使用 **Qt** 实现类微信聊天界面（好友列表、会话列表、气泡消息、聊天记录加载、搜索/添加好友等），通过 **QListWidget + 自定义 ItemWidget** 管理会话与好友列表，使用 **QPainter/QSS** 封装气泡消息组件并优化 UI 体验；网络侧基于 **Qt Network** 封装 HTTP/TCP 调用，完成登录、拉取列表、消息收发与断线重连。

后端采用 **C++ 分布式微服务架构**，拆分为 **网关服务 GateServer、聊天服务 ChatServer、状态服务 StatusServer、验证服务 VerifyServer**：

- **GateServer（HTTP）** 负责注册登录、鉴权与路由；登录时向 StatusServer 查询在线 ChatServer，实现**负载均衡与服务发现**；
- **ChatServer（TCP 长连接）** 基于 **Asio** 实现高并发可靠长连接，使用 **async_read/async_write** 的异步 IO（Proactor 思路）完成消息收发与转发；连接由 Session 管理，结合 **智能指针/RAII** 保证回调生命周期安全；
- 服务间使用 **gRPC** 通信，封装 **gRPC 连接池** 支持并发访问与断线重连；
- 数据层使用 **MySQL** 持久化用户、好友关系、聊天记录等，封装 **MySQL 连接池**（生产者-消费者 + 心跳保活），同时引入 **Redis 缓存** 并封装 Redis 连接池，减少热点数据访问延迟。

在压测中，单机节点可稳定支撑**约 8000+ 长连接**（多节点部署可进一步扩展），消息收发延迟在高并发场景下保持稳定，并通过心跳机制及时清理僵尸连接，提升系统可用性与稳定性。

**技术关键词：**   **Qt（信号槽/QSS/QPainter/自定义控件）、C++11、Asio、TCP 长连接、Proactor/异步 IO、TLV/粘包处理、gRPC、Redis/MySQL、连接池、线程池、负载均衡、断线重连、RAII/智能指针、生产者-消费者、MVC 分层**。



## 开发环境

- 平台：Windows
- IDE：Visual Studio 2019
- C++ 标准：C++14（兼容旧版 gRPC）
- 构建系统：MSBuild（未来考虑 CMake）

## 依赖库

- gRPC v1.40.0
- Protobuf v3.17.3
- Boost.Asio（网络 IO）

------
