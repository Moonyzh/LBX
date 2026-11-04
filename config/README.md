# config

本目录保存 tRPC YAML 配置。

- `pricing-service.yaml`：计价服务监听容器内 8000 端口。
- `trip-service.yaml`：订单服务监听 8000，并把计价代理指向 `pricing:8000`。
- `ride-client.yaml`：把订单代理指向 `trip:8000`。
- `dht-node.yaml`：DHT 节点示例配置；node-b/node-c 需修改服务名、监听标识和后继地址。
- `dht-node-b.example.yaml`：乙负责的 node-b CVM 配置模板，只需填写 node-c 的可达地址。
- `dht-node-*.local.yaml`：一台机器启动三个 DHT 进程时使用的本地环配置。
- `dht-smoke-client.local.yaml`：DHT 冒烟测试客户端访问三个本地入口。
- `dht-smoke-client.cvm.example.yaml`：三台 CVM 联调时使用的测试客户端模板。

容器各自拥有网络命名空间，因此两个服务都能监听 8000。服务间地址使用 Compose 服务名，不改成 `127.0.0.1`。

