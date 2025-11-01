### 分支管理方案：分模块开发 + 合并策略

建议按 “`main` 主分支 → 功能分支 → 合并验证” 的流程进行，具体步骤：

#### 拆分三个功能分支，分别开发对应模块

从 `main` 分支创建三个子分支，专注于各自模块的开发：

```bash
# 创建 server 分支（开发服务器端完整功能）
git checkout -b server main

# 创建 client 分支（开发客户端完整功能）
git checkout -b client main

# 创建 utils 分支（完善通用工具函数）
git checkout -b utils main
```

**各分支开发重点**：

- `server` 分支：在原型基础上扩展服务器功能（完善 `ftp_cmds.c` 中的命令处理、`data_transfer.c` 的文件传输等）；
- `client` 分支：扩展客户端功能（实现 `transfer_setup_pasv`/`transfer_recv_file` 等数据传输逻辑）；
- `utils` 分支：完善路径处理（`utils_join_path`/`utils_check_path`）、错误处理等通用工具。

#### 定期合并，避免分支偏离

由于三个模块存在依赖（如 `server` 和 `client` 都依赖 `utils`），需定期将已完成的功能合并回 `main`，再同步到其他分支：

```bash
# 例：当 utils 分支完成路径工具后，合并到 main
git checkout main
git merge utils
git commit -m "merge utils: complete path handling functions"

# 其他分支（server/client）同步 main 的更新
git checkout server
git merge main  # 同步 utils 的新功能到 server 分支
```

**合并频率**：建议每个模块完成一个独立功能点（如服务器端的 `RETR` 命令、客户端的 `PORT` 模式）后就合并到 `main`，避免分支长期隔离导致大量冲突。

#### 最终合并与测试

当三个分支的核心功能完成后，统一合并到 `main` 进行集成测试：

```bash
git checkout main
git merge server
git merge client
# 解决可能的冲突（如 utils 函数的调用方式差异）
git commit -m "merge all modules: complete ftp server/client"
```

### 编译命令

#### 编译服务器

```bash
gcc -o ftp_server \
    server/main.c \
    server/config/config.c \
    server/net/socket_utils.c \
    server/conn/client_conn.c \
    server/cmd/ftp_cmds.c \
    utils/utils.c \
    -Iserver/config -Iserver/net -Iserver/conn -Iutils
```

#### 编译客户端

```bash
gcc -o ftp_client \
    client/main.c \
    client/net/client_socket.c \
    client/cmd/client_cmds.c \
    client/ui/ui_utils.c \
    utils/utils.c \
    -Iclient/net -Iclient/cmd -Iclient/ui -Iutils
```

###  运行测试

#### 启动服务器

```bash
./ftp_server -p 2121  # 使用2121端口
```

#### 启动客户端（新终端）

```bash
./ftp_client 127.0.0.1 2121  # 连接本地服务器
```