# proto

本目录定义网约车微服务的 RPC 契约。

- `pricing.proto`：计价服务的 `EstimateFare`。
- `trip.proto`：订单服务的 `CreateRide` 与 `GetTrip`。
- `dht.proto`：三节点 DHT 的通用 `Put` 与 `Get`。

修改字段时保留已有字段编号，不要复用已删除字段的编号。生成的 `.pb.*` 和 `.trpc.pb.*` 文件由 CMake 写入构建目录，不提交到这里。

