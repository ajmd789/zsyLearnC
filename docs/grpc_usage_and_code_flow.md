# zsyLearnC gRPC 使用与关键代码流转详解

本文基于当前仓库代码讲解，不讲空泛概念，重点是你这个项目里 gRPC 的真实调用链。

## 1. 先看项目在做什么

这个程序在同一进程内启动两个 gRPC server（`node1`、`node2`），再启动两个发送线程，按轮次做 10 轮 ping-pong：

1. `node1` 线程通过 gRPC 调 `node2` 的 `SendMessage`
2. `node2` 服务端返回 ACK
3. 等 5 秒
4. `node2` 线程通过 gRPC 调 `node1` 的 `SendMessage`
5. `node1` 服务端返回 ACK
6. 重复以上过程

关键文件：
- `proto/pingpong.proto`
- `CMakeLists.txt`
- `src/app/pingpong_app.cpp`
- `src/common/grpc_message_client.cpp`
- `src/common/messenger_service.cpp`
- `src/common/turn_coordinator.cpp`
- `src/node1/node1_sender.cpp`
- `src/node2/node2_sender.cpp`

## 2. gRPC 在这个项目里的“最小闭环”

### 2.1 协议定义（proto）

`proto/pingpong.proto` 定义了：

- 请求消息 `PingRequest`
- 响应消息 `PingReply`
- 服务 `Messenger`
- RPC 方法 `SendMessage(PingRequest) returns (PingReply)`

这一步的意义是“把通信契约写死”，客户端和服务端都以这份契约生成代码。

### 2.2 代码生成（CMake + protoc）

`CMakeLists.txt` 里通过 `add_custom_command` 调用了：

- `--cpp_out` 生成 protobuf 消息代码（`pingpong.pb.*`）
- `--grpc_out` 生成 gRPC service/stub 代码（`pingpong.grpc.pb.*`）
- `--plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN_EXECUTABLE}`

生成目录是 `${build_dir}/generated`（变量 `GENERATED_DIR`）。

然后：
- `add_library(pingpong_proto ...)` 把生成代码编进库
- `pingpong_core` 和 `pingpong_demo` 链接这个库

所以：**proto 是源头，generated 是桥梁，core/demo 才是业务实现。**

## 3. 运行期总流程（从 main 到每次 RPC）

### 3.1 程序入口

`src/main.cpp` 只做一件事：

- 创建 `app::PingPongApp`
- 调 `Run()`

### 3.2 PingPongApp::Run 的编排职责

在 `src/app/pingpong_app.cpp`：

1. 创建共享日志器 `ThreadSafeLogger`
2. 创建两个服务实例：
   - `MessengerService("node1", logger)`
   - `MessengerService("node2", logger)`
3. 启动两个 gRPC server：
   - `0.0.0.0:50051`（node1）
   - `0.0.0.0:50052`（node2）
4. 各自 `Wait()` 放到后台线程，避免阻塞主线程
5. 等 500ms，让 server 启动稳定
6. 创建两个客户端：
   - node1 客户端目标 `127.0.0.1:50052`（发给 node2）
   - node2 客户端目标 `127.0.0.1:50051`（发给 node1）
7. 创建轮次协调器 `TurnCoordinator`
8. 创建并启动两个发送线程 `Node1Sender`/`Node2Sender`
9. 等发送线程结束后，关闭 server，回收线程
10. 打印 `completed 10 ping-pong rounds`

这里最核心的是：**app 层只做编排，不写 gRPC 细节。**

## 4. 服务端路径：请求如何被接收并回 ACK

### 4.1 服务注册

`common::BuildServer`（`src/common/server_utils.cpp`）做三件事：

1. `grpc::ServerBuilder builder`
2. `builder.AddListeningPort(...)`
3. `builder.RegisterService(service)`

返回 `BuildAndStart()` 的 `Server` 对象。

### 4.2 业务处理

`src/common/messenger_service.cpp` 里 `MessengerService::SendMessage(...)` 是真正的 RPC 处理函数：

1. 读取 `request` 中 `from/sequence/body`
2. 记录接收日志（谁收到了第几次请求）
3. 填充 `reply`：
   - `received = true`
   - `ack_from = self_name_`
   - `ack_at = 当前时间`
   - `sequence = request->sequence()`
4. 返回 `grpc::Status::OK`

也就是说，这个 demo 的服务端逻辑是“确认收到 + 回 ACK 元数据”。

## 5. 客户端路径：请求如何构造、发送、判定成功

`src/common/grpc_message_client.cpp`

### 5.1 构造阶段

构造函数里：

- `CreateChannel(target, InsecureChannelCredentials())`
- `pingpong::Messenger::NewStub(channel)`

得到 `stub_`，后续每次调用都通过它发 RPC。

### 5.2 Send 调用阶段

`GrpcMessageClient::Send(from, to, sequence)`：

1. 构造 `PingRequest`
   - `from`
   - `to`
   - `body = "hello from " + from`
   - `sent_at = CurrentTimeString()`
   - `sequence`
2. 创建 `ClientContext`
3. 同步调用：`stub_->SendMessage(&context, request, &reply)`
4. 如果 `status.ok()==false`，记录失败日志并返回 `false`
5. 成功则记录 ACK 日志并返回 `true`

这是同步 unary RPC（一次请求一次响应）的标准 C++ gRPC 用法。

## 6. 并发与轮次控制：两个 sender 为什么不会乱发

### 6.1 TurnCoordinator 机制

`src/common/turn_coordinator.cpp` 提供：

- `WaitFor(expected_turn)`：阻塞等待当前轮次到来
- `SetTurn(next_turn)`：切换轮次并唤醒等待线程

内部是 `mutex + condition_variable`，并支持 `Done` 终止态。

### 6.2 Node1Sender 行为

`src/node1/node1_sender.cpp`

每轮流程：

1. 等待轮到 `Node1ToNode2`
2. 调用 `client_.Send("node1","node2", round)`
3. 睡眠 5 秒
4. 切换到 `Node2ToNode1`

### 6.3 Node2Sender 行为

`src/node2/node2_sender.cpp`

每轮流程：

1. 等待轮到 `Node2ToNode1`
2. 调用 `client_.Send("node2","node1", round)`
3. 睡眠 5 秒
4. 若已最后一轮 -> `SetTurn(Done)`，否则切回 `Node1ToNode2`

因此不会出现两个线程同时抢着发同一轮请求。

## 7. 一次完整调用的时序（第 1 轮示例）

1. `Node1Sender` 被 `TurnCoordinator` 放行
2. `GrpcMessageClient(node1->node2)` 组包并调用 `stub_->SendMessage`
3. `node2` 的 `MessengerService::SendMessage` 执行，写接收日志
4. `node2` 返回 `PingReply`
5. `node1` 客户端收到 ACK，写发送结果日志
6. `Node1Sender` 睡眠 5 秒后切轮到 `node2`
7. `Node2Sender` 重复对称过程

## 8. 日志如何对照代码

你通常会看到两类日志：

1. 接收日志（服务端）  
   形态：`node2 received sequence=1 from=node1 ...`  
   来源：`MessengerService::SendMessage`

2. ACK 日志（客户端）  
   形态：`node1 -> node2 sequence=1 ack=true ack_from=node2 ...`  
   来源：`GrpcMessageClient::Send`

最终收尾日志：
- `completed 10 ping-pong rounds`  
来源：`PingPongApp::Run`

## 9. 这个项目里 gRPC 的关键知识点（你应该掌握）

1. `proto3` 是契约；代码由 `protoc` + `grpc_cpp_plugin` 生成。
2. C++ 客户端核心对象是 `Stub`，服务端核心对象是 `Service`。
3. `ServerBuilder` 负责监听端口和注册服务。
4. 这是同步 unary RPC：调用线程会阻塞直到返回。
5. `ClientContext` / `ServerContext` 是每次 RPC 的上下文容器。
6. `grpc::Status` 用于网络/协议层成功失败判定。
7. 业务层“成功”应放在 reply 字段里（这里用 `received`）。
8. 多线程输出要做互斥（`ThreadSafeLogger`），否则日志会串行破坏。
9. 通信顺序由业务协调器控制（`TurnCoordinator`），不是 gRPC 自动保证。

## 10. 当前实现的边界与可改进点

1. 使用了 `InsecureChannelCredentials`，仅适合本地/内网 demo。
2. `Send` 失败仅记录日志，未做重试/退避。
3. 没设置 deadline（超时），理论上可能长期阻塞。
4. 未做优雅停止握手（先停 sender，再停 server 已可用，但还可更精细）。
5. 同步模型简单但吞吐有限；高并发场景可考虑 async API。

## 11. 你可以继续问我的方向（复制就能问）

如果你要继续深挖，直接发下面任一问题：

1. “把这个项目改成 **异步 gRPC（CompletionQueue）**，给我完整改造方案和代码。”
2. “给 `GrpcMessageClient::Send` 增加 **deadline + 重试 + 指数退避**，并解释每个参数。”
3. “把 `InsecureChannelCredentials` 改成 **TLS**，列出证书、服务端和客户端改动。”
4. “解释 `generated/pingpong.grpc.pb.h` 里 `Stub` 和 `Service` 的关键接口。”
5. “给我画出 **第 N 轮** 从 `Node1Sender` 到 `MessengerService` 的逐行调用链。”

如果你愿意，我下一步可以再给你一份“按文件逐行精讲版”（更细，接近源码阅读手册）。
