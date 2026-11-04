# scripts

本目录提供可重复的实验操作。

- `up.sh`：构建并重建两个服务。
- `client.sh`：运行一次客户端调用。
- `verify.sh`：验证两笔固定订单及价格。
- `fault-test.sh`：验证服务停止后的故障边界。
- `collect-result.sh`：生成在线提交包及机器可读结果。
- `grade.sh`：自动评分的第一轮检查。
- `dht-local-test.sh`：在一台机器启动三个 DHT 进程并验证逐跳 Put/Get。
- `start-node-b.sh`：启动乙负责的 node-b CVM 节点。
- `upload-node-b.ps1`：从本地 Windows 上传 node-b 正式配置和启动脚本。
- `build-base-image.sh`、`export-base-image.sh`：仅供教师准备镜像。

从仓库根目录用 `bash scripts/<名称>.sh` 运行。云主机账号需已加入 Docker 用户组并重新登录。

