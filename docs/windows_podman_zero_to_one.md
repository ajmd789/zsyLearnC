# Windows 从 0 到 1 接入 Podman 全流程（zsyLearnC 实战版）

本文目标：让你在 Windows 上从零完成 Podman 开发环境接入，并能稳定构建/运行本项目；同时覆盖你实际遇到过的常见错误与处理方案。

适用对象：
- Windows 10/11 用户
- 使用 VS Code + Dev Containers
- 项目路径：`C:\Users\Archimedes\Desktop\codes\zsyLearnC`

---

## 0. 术语和架构先说明（避免后面混淆）

这套环境里有 3 层：

1. Windows 主机  
2. WSL 发行版（如 `Ubuntu-22.04`）  
3. 容器运行时（Podman / Docker CLI）与容器本身

你这个项目当前推荐的稳定方案：
- 手工构建运行可用 `podman.exe`（Windows 侧）
- VS Code Dev Containers 走 WSL 里的 `docker` 命令（你已验证可用）

注意：这不是冲突，而是“开发工具链分工”。

---

## 1. 前置检查（必须先过）

在 Windows PowerShell 执行：

```powershell
systeminfo | Select-String "OS Name","OS Version"
wsl --status
wsl -l -v
```

判定标准：
- `wsl --status` 能正常输出
- `wsl -l -v` 至少有一个发行版，建议 `Ubuntu-22.04`
- 发行版版本是 `2`

如果没有 Ubuntu 22.04：

```powershell
wsl --install -d Ubuntu-22.04
```

安装后重启电脑，再次执行 `wsl -l -v` 确认。

---

## 2. 安装 Podman（Windows）

1. 安装 Podman Desktop（或 Podman CLI，Desktop 更省事）。  
2. 打开 PowerShell 验证：

```powershell
$PODMAN = "C:\Users\Archimedes\AppData\Local\Programs\Podman\podman.exe"
& $PODMAN --version
```

若提示找不到路径，先在资源管理器确认 `podman.exe` 实际安装目录，再改变量。

---

## 3. 初始化并启动 Podman Machine

首次执行一次：

```powershell
& $PODMAN machine init --cpus 4 --memory 8192 --disk-size 60
```

每次开发前执行：

```powershell
& $PODMAN machine start
```

验证：

```powershell
& $PODMAN machine list
& $PODMAN info
```

---

## 4. 准备项目目录

```powershell
cd C:\Users\Archimedes\Desktop\codes\zsyLearnC
Get-ChildItem
```

你应能看到：
- `Containerfile`
- `CMakeLists.txt`
- `proto\`
- `src\`
- `.devcontainer\`

---

## 5. 拉取 Debian 13 基础镜像（重点：多源兜底）

你已经遇到过 `dockerproxy.net` 偶发 `500 Internal Server Error`，所以按“主源 + 备用源 + 本地重标记”执行。

### 5.1 先试主源

```powershell
& $PODMAN pull dockerproxy.net/library/debian:13
```

### 5.2 主源失败时，试备用源（推荐顺序）

```powershell
& $PODMAN pull docker.1ms.run/debian:13
```

### 5.3 统一本地标签（非常关键）

```powershell
& $PODMAN tag docker.1ms.run/debian:13 debian:13
& $PODMAN tag debian:13 dockerproxy.net/library/debian:13
& $PODMAN images
```

目的：
- 即使上游镜像源不稳定，`Containerfile` 仍可命中本地缓存标签，避免重复拉取失败。

---

## 6. 构建项目镜像

```powershell
& $PODMAN build --pull=never -t zsy-learn-grpc:debian13 -f Containerfile .
```

说明：
- `--pull=never` 防止构建时再次回源拉取基础镜像。
- 成功后应看到 `zsy-learn-grpc:debian13`。

验证：

```powershell
& $PODMAN images
```

---

## 7. 进入容器并运行程序（手工方式）

```powershell
& $PODMAN run --rm -it --cpus=4 --memory=8g -v "${PWD}:/workspace" -w /workspace --hostname rk3588 zsy-learn-grpc:debian13 bash
```

容器内执行：

```bash
cmake -S /workspace -B /workspace/build-podman -DCMAKE_BUILD_TYPE=Release
cmake --build /workspace/build-podman -j"$(nproc)"
/workspace/build-podman/pingpong_demo
```

---

## 8. VS Code Dev Container 接入（你现在最常用）

### 8.1 必要设置（你机器上已踩过坑）

你之前的问题是：
- Dev Containers 被设置成 `podman`
- 但 WSL 内没有 `podman`，于是报 `spawn podman ENOENT`

请确保用户设置为：

```json
"dev.containers.dockerPath": "docker",
"dev.containers.executeInWSL": true
```

文件路径：
- `C:\Users\Archimedes\AppData\Roaming\Code\User\settings.json`

### 8.2 仓库 DevContainer 配置关键点

`.devcontainer/devcontainer.json` 应包含：

```json
"workspaceFolder": "/workspace",
"workspaceMount": "source=${localWorkspaceFolder},target=/workspace,type=bind"
```

否则会出现“容器进去了，但 `/workspace` 为空”的问题（你已遇到并修复）。

### 8.3 标准进入流程

1. 关闭 VS Code 全窗口  
2. PowerShell 执行：

```powershell
wsl --shutdown
```

3. 重新打开项目  
4. 执行：`Dev Containers: Rebuild and Reopen in Container`

---

## 9. 环境自检清单（每次异常先跑）

Windows PowerShell：

```powershell
wsl -d Ubuntu-22.04 -e sh -lc "id -nG"
wsl -d Ubuntu-22.04 -e sh -lc "docker info --format '{{.ServerVersion}} {{.OperatingSystem}}'"
wsl -d Ubuntu-22.04 -e sh -lc "docker ps"
```

容器内（Dev Container 终端）：

```bash
pwd
ls -la /workspace
cmake --version
protoc --version
grpc_cpp_plugin --version || which grpc_cpp_plugin
```

---

## 10. 常见错误全集与处理

## 10.1 `spawn podman ENOENT`

现象：
- Dev Containers 日志反复出现 `spawn podman ENOENT`

原因：
- VS Code 配成了 `dev.containers.dockerPath=podman`
- 但 WSL 内没有 `podman` 可执行文件

处理：
- 改成：
  - `"dev.containers.dockerPath": "docker"`
  - `"dev.containers.executeInWSL": true`

---

## 10.2 `usermod -aG docker ubuntu` 后看起来卡住

现象：
- 日志停在这行附近

原因：
- 扩展在做 WSL docker 组授权，通常要重启 WSL 会话

处理：

```powershell
wsl --shutdown
wsl -d Ubuntu-22.04 -e sh -lc "id -nG"
wsl -d Ubuntu-22.04 -e sh -lc "docker info --format '{{.ServerVersion}} {{.OperatingSystem}}'"
```

若第一条含 `docker` 且第二条返回版本，继续 Reopen/Rebuild 即可。

---

## 10.3 `dockerproxy.net/...: 500 Internal Server Error`

现象：
- 拉取 `dockerproxy.net/library/debian:13` 返回 500

原因：
- 镜像代理站上游/节点临时异常

处理：
1. 改用备用源：

```powershell
& $PODMAN pull docker.1ms.run/debian:13
```

2. 打兼容标签：

```powershell
& $PODMAN tag docker.1ms.run/debian:13 debian:13
& $PODMAN tag debian:13 dockerproxy.net/library/debian:13
```

3. 构建时加 `--pull=never`

---

## 10.4 `No such image: dockerproxy.net/library/debian:13`

现象：
- 构建或 inspect 报镜像不存在

原因：
- 本地没有这个标签

处理：
- 执行上面的本地重标记流程，确保该标签存在。

---

## 10.5 进入容器后 `/workspace` 是空目录

现象：
- `ls /workspace` 空

原因：
- 代码实际挂载到了 `/workspaces/<repo>`，与 `workspaceFolder` 不一致

处理：
- 在 `devcontainer.json` 补：

```json
"workspaceMount": "source=${localWorkspaceFolder},target=/workspace,type=bind"
```

然后 `Rebuild Container`。

---

## 10.6 `Current working directory cannot be established`

原因：
- 当前目录被删或挂载状态变化

处理：

```bash
cd /
cd /workspace
pwd
```

之后重新执行 cmake。

---

## 10.7 `CMakeCache.txt directory is different`

原因：
- 旧缓存路径和当前路径不一致（主机/容器切换常见）

处理：

```bash
rm -rf /workspace/build-podman
cmake -S /workspace -B /workspace/build-podman -DCMAKE_BUILD_TYPE=Release
```

---

## 10.8 `grpc_cpp_plugin program not found`

原因：
- 容器里缺插件，或 CMake 未正确定位

处理：
1. 容器安装 `protobuf-compiler-grpc`
2. 确认 CMake 使用：
   - `--plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN_EXECUTABLE}`

---

## 10.9 `permission denied while trying to connect to docker daemon`

原因：
- 当前用户不在 docker 组，或会话未刷新

处理：

```powershell
wsl -d Ubuntu-22.04 -u root -e sh -lc "usermod -aG docker ubuntu"
wsl --shutdown
```

重新进入 WSL 再验证 `docker ps`。

---

## 11. 推荐“最稳开发日常流程”

1. `wsl --shutdown`（开工前可做一次）  
2. 启动 VS Code，打开项目  
3. `Dev Containers: Rebuild and Reopen in Container`（配置改动后）  
4. 容器内编译：

```bash
cmake -S /workspace -B /workspace/build-podman -DCMAKE_BUILD_TYPE=Release
cmake --build /workspace/build-podman -j"$(nproc)"
/workspace/build-podman/pingpong_demo
```

---

## 12. 你后续可以继续让我做的事

1. 把这套教程改成“图文版”（附命令执行期望输出截图位）  
2. 增加“一键诊断脚本”（PowerShell）自动检查 20+ 项环境问题  
3. 把镜像源改成“自动探活 + 自动回退”脚本，避免手动切换  

