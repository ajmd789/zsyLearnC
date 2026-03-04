# zsyLearnC - C++17 + gRPC 双线程通信示例（Windows + Podman + Debian 13）

本项目已验证可运行，能力如下：
- 使用 C++17
- 兼容 CMake 3.10
- 两个线程之间通过 gRPC 往返通信
- 时间格式：`YYYYMMDD HH:MM:SS`（例如 `20260303 23:42:01`）
- 共 10 轮：`node1 -> node2`，间隔 5 秒后 `node2 -> node1`

## 1. 进入项目目录

```powershell
cd C:\Users\Archimedes\Desktop\codes\zsyLearnC
$PODMAN = "C:\Users\Archimedes\AppData\Local\Programs\Podman\podman.exe"
```

## 2. 初始化并启动 Podman Machine（4 核 + 8GB）

首次只执行一次：

```powershell
& $PODMAN machine init --cpus 4 --memory 8192 --disk-size 60
```

每次使用前执行：

```powershell
& $PODMAN machine start
```

如果提示 `already running` 可忽略。

## 3. 拉取 Debian 13 基础镜像（国内源）

优先：

```powershell
& $PODMAN pull dockerproxy.net/library/debian:13
```

备用：

```powershell
& $PODMAN pull docker.1ms.run/library/debian:13
```

打本地标签（避免构建时回源 Docker Hub）：

```powershell
& $PODMAN tag dockerproxy.net/library/debian:13 debian:13
```

查看本地镜像：

```powershell
& $PODMAN images
```

## 4. 构建项目镜像

```powershell
& $PODMAN build --pull=never -t zsy-learn-grpc:debian13 -f Containerfile .
```

## 5. 如何进入容器（临时进入 / 长时间进入）

### 5.1 临时进入（退出即销毁）

适合临时调试，退出后容器自动删除。

```powershell
& $PODMAN run --rm -it --cpus=4 --memory=8g -v "${PWD}:/workspace" -w /workspace --hostname rk3588 zsy-learn-grpc:debian13 bash
```

说明：
- 进入后提示符类似 `root@rk3588:/workspace#`。
- 你已是 `root`，不需要 `sudo`。
- 退出命令：`exit`。

### 5.2 长时间进入（后台常驻容器）

先启动一个常驻容器，再反复进入。

1) 启动后台容器：

```powershell
& $PODMAN run -d --name zsy-dev --cpus=4 --memory=8g -v "${PWD}:/workspace" -w /workspace --hostname rk3588 zsy-learn-grpc:debian13 bash -lc "while true; do sleep 3600; done"
```

2) 进入容器：

```powershell
& $PODMAN exec -it zsy-dev bash
```

3) 查看容器状态：

```powershell
& $PODMAN ps
```

4) 退出当前 shell（容器继续运行）：

```bash
exit
```

5) 停止并删除常驻容器（不再需要时）：

```powershell
& $PODMAN stop zsy-dev
& $PODMAN rm zsy-dev
```

## 6. 一条命令：容器内编译并运行（推荐）

使用绝对路径，不依赖当前目录：

```powershell
& $PODMAN run --rm -i --cpus=4 --memory=8g -v "${PWD}:/workspace" -w /workspace zsy-learn-grpc:debian13 bash -lc 'set -e; rm -rf /workspace/build-podman; cmake -S /workspace -B /workspace/build-podman -DCMAKE_BUILD_TYPE=Release; cmake --build /workspace/build-podman -j"$(nproc)"; /workspace/build-podman/pingpong_demo'
```

## 7. 容器内一步一步：编译、打包、运行

先进入容器（推荐用 5.1 或 5.2 的方式进入），然后在容器内执行下面命令。

1) 修复当前目录并回到工程根目录（关键）：

```bash
cd / || exit 1
cd /workspace || exit 1
pwd
```

2) 清理旧构建目录（使用绝对路径）：

```bash
rm -rf /workspace/build-podman
```

3) 配置工程：

```bash
cmake -S /workspace -B /workspace/build-podman -DCMAKE_BUILD_TYPE=Release
```

4) 编译：

```bash
cmake --build /workspace/build-podman -j"$(nproc)"
```

5) 运行程序：

```bash
/workspace/build-podman/pingpong_demo
```

6) 打包可执行文件（生成 `.tar.gz`）：

```bash
mkdir -p /workspace/dist
cp /workspace/build-podman/pingpong_demo /workspace/dist/
tar -czf /workspace/dist/pingpong_demo-linux-amd64.tar.gz -C /workspace/dist pingpong_demo
sha256sum /workspace/dist/pingpong_demo-linux-amd64.tar.gz
```

打包后产物路径：
- 容器内：`/workspace/dist/pingpong_demo-linux-amd64.tar.gz`
- Windows 主机：`C:\Users\Archimedes\Desktop\codes\zsyLearnC\dist\pingpong_demo-linux-amd64.tar.gz`

## 8. 预期输出

运行后会看到类似日志：
- `[20260303 16:28:30] node2 received sequence=1 from=node1 ...`
- `[20260303 16:28:30] node1 -> node2 sequence=1 ack=true ...`
- 最终输出：`completed 10 ping-pong rounds`

## 9. 构建目录是否可删除

`build` 和 `build-podman` 都是构建产物目录，可以直接删除。

PowerShell 删除命令：

```powershell
Remove-Item -Recurse -Force .\build, .\build-podman
```

如果这些目录之前已经被 `git add` 过，还需要从索引中移除：

```powershell
git rm -r --cached build build-podman
```

## 10. 常见问题

### `Current working directory cannot be established`
原因：当前 shell 所在目录失效（常见于你在 `build-podman` 目录里时，目录被删除，或挂载目录状态变化）。

处理：

```bash
cd /
cd /workspace
pwd
cmake -S /workspace -B /workspace/build-podman -DCMAKE_BUILD_TYPE=Release
cmake --build /workspace/build-podman -j"$(nproc)"
```

### `image not known`
原因：本地没有 `debian:13`。

处理：先执行第 3 步拉取并打标签。

### `CMakeCache.txt directory is different`
原因：本机构建缓存与容器路径冲突。

处理：使用 `build-podman`，并采用第 6、7 步的绝对路径命令。

### `grpc_cpp_plugin program not found`
项目已修复为：

`--plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN_EXECUTABLE}`

## 11. 可选：停止 Podman Machine

```powershell
& $PODMAN machine stop
```

## 12. 重构后目录规划（线程拆分 + 公共层）

为解决 `src/main.cpp` 逻辑过重，当前代码已按“入口层 / 编排层 / 线程层 / 公共层”拆分。

目录结构：

```text
zsyLearnC/
├─ include/
│  ├─ app/
│  │  └─ pingpong_app.h
│  ├─ common/
│  │  ├─ grpc_message_client.h
│  │  ├─ messenger_service.h
│  │  ├─ server_utils.h
│  │  ├─ thread_safe_logger.h
│  │  ├─ time_utils.h
│  │  └─ turn_coordinator.h
│  ├─ node1/
│  │  └─ node1_sender.h
│  └─ node2/
│     └─ node2_sender.h
├─ src/
│  ├─ app/
│  │  └─ pingpong_app.cpp
│  ├─ common/
│  │  ├─ grpc_message_client.cpp
│  │  ├─ messenger_service.cpp
│  │  ├─ server_utils.cpp
│  │  ├─ thread_safe_logger.cpp
│  │  ├─ time_utils.cpp
│  │  └─ turn_coordinator.cpp
│  ├─ node1/
│  │  └─ node1_sender.cpp
│  ├─ node2/
│  │  └─ node2_sender.cpp
│  └─ main.cpp
├─ proto/
│  └─ pingpong.proto
└─ CMakeLists.txt
```

职责划分：
- `src/main.cpp`：只做程序入口（创建并运行 `PingPongApp`）。
- `src/app`：编排流程（启动 server、创建 client、拉起线程、收尾退出）。
- `src/node1`：`node1` 线程自身发送循环逻辑。
- `src/node2`：`node2` 线程自身发送循环逻辑。
- `src/common`：公共基础能力（时间、日志、gRPC 服务与客户端封装、轮次协调、server 构建）。

后续扩展建议：
- 新增线程角色时，按 `src/nodeX` + `include/nodeX` 新建目录，不改 `common` 的业务语义。
- `common` 仅放可复用能力，不放节点特有规则。
- `app` 层只做组合编排，不写底层 gRPC 细节。