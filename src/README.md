# src

本目录包含实验一 A 的 C++ 应用代码。

- `pricing_service.*`：计价业务实现。
- `trip_service.*`：订单业务、内存仓储及计价服务代理。
- `pricing_server.cc`、`trip_server.cc`：服务注册与进程入口。
- `ride_client.cc`：创建和查询订单的命令行客户端。
- `dht_common.*`：FNV-1a 32 位 Hash 与静态环 owner 判断。
- `dht_service.*`、`dht_server.cc`：DHT 本地存储、successor 转发与进程入口。

学生主要修改 `TODO(Lab1A-1)` 和 `TODO(Lab1A-2)` 所在区域，不手工编辑 Proto 生成代码。

