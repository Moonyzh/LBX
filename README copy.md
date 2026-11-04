# 实验一 B：三节点 mini-DHT 订单仓储

> 实验 1A 完成后再开始本阶段。1A 中的订单保存在 `trip-service` 进程内存；本阶段把它替换为运行在三台 CVM 上的 DHT，同时保持乘客侧的 `CreateRide` / `GetTrip` 接口不变。

## 1. 目标与边界

你们小组将用 C++ 与 tRPC-Cpp 实现一个可运行的、**静态三节点**分布式哈希表。订单服务用它保存和读取订单：

```text
ride-client --> trip-service --> DHT Put / Get --> node-a --> node-b --> node-c
                                  ^                  |                    |
                                  +------ 静态 successor 环 ---------------+
```

本实验训练的是“真实组网、真实路由、真实故障观察”，不是让 AI 生成完整 Chord。范围严格限定如下：

- 必做：`Put` / `Get`、三节点哈希环、successor 逐跳转发、远端 tRPC 调用、真实三台 CVM 部署。
- 必做：将 `trip:<trip_id>` 作为 key；订单以 `bytes` 值存入 DHT；`TripService` 对客户端的两个 RPC 不改名、不改字段。
- 不要求：finger table、动态加/离节点、自动迁移、复制、修复和一致性协议。
- 加分项：在保证基础功能后，为每个 key 保存一个副本，并说明故障后读请求如何选择副本。

## 2. 前置条件与分工

1. 每名成员使用**自己的**腾讯云账号和一台 Ubuntu 24.04 x86_64 CVM；不得共享账号、密码、私钥或优惠券。
2. 每台主机完成实验 1A 的 Docker 环境与课程基础镜像准备；三台机器之间只开放本实验使用的 tRPC 端口，并按最小权限配置安全组。
3. 小组共用一个代码仓库可以，但报告必须逐项写清每名成员的代码、部署、测试与故障分析贡献。
4. 将 [node-inventory.example.env](node-inventory.example.env) 复制为本地不提交的 `node-inventory.env`，记录节点名称、内网地址和端口；它不得包含密码或私钥。

建议角色是“节点 A 与部署”“节点 B 与路由”“节点 C 与验证/报告”。角色不是分割责任：任一成员都可能被要求解释完整调用链和路由判断。

## 3. 固定设计契约

为保证不同小组提交可以验收，基础部分采用以下契约。可以在此之上扩展，但不可修改其语义。

### 3.1 哈希环与归属

- 使用无符号 32 位哈希空间 `[0, 2^32-1]`；`Hash(text)` 的具体实现要写入代码和报告，并保证三台机器结果相同。
- 给三个节点配置彼此不同的 `node_id`，按无符号整数升序形成环；每个节点知道自己的 predecessor 与 successor。
- key 的责任节点是顺时针第一个 `node_id >= Hash(key)` 的节点；越过最大 `node_id` 时回绕到最小节点。
- 等价地，节点 `N` 对区间 `(predecessor_id, N.node_id]` 负责；回绕区间必须单独正确处理。
- 请求从任意一个入口节点进入。非责任节点只转发给 successor；每经过一个节点，`hops` 加一。
- 若 `hops >= 3`，必须停止并返回明确错误。这是静态环的配置保护，不是容错机制。

### 3.2 DHT RPC

在 `proto/dht.proto` 中定义下面等价的接口。字段编号可不同，但不得删除这些语义：

```proto
service DhtService {
  rpc Put(PutRequest) returns (PutReply);
  rpc Get(GetRequest) returns (GetReply);
}

message PutRequest {
  string key = 1;
  bytes value = 2;
  uint32 hops = 3;
}
message PutReply {
  bool ok = 1;
  string owner = 2;
  uint32 hops = 3;
  string message = 4;
}
message GetRequest {
  string key = 1;
  uint32 hops = 2;
}
message GetReply {
  bool found = 1;
  bytes value = 2;
  string owner = 3;
  uint32 hops = 4;
  string message = 5;
}
```

`DhtService` 是通用 KV 层，不应依赖 `Trip` 的字段。`TripService` 负责将 `Trip` 序列化为字节、以 `trip:<trip_id>` 为 key 调用 DHT，并在 `GetTrip` 时反序列化。

### 3.3 代码边界

保留 1A 的价格服务职责；不要把计价公式复制到 DHT 节点。需要替换的是 `TripServiceImpl` 内的内存 `unordered_map`，不是乘客端 API：

```text
CreateRide: pricing RPC --> Trip 序列化 --> DHT Put("trip:<id>", bytes)
GetTrip:    DHT Get("trip:<id>") --> Trip 反序列化 --> 原样返回
```

不要手工编辑 protobuf 或 tRPC 生成文件；每次修改 `.proto` 后重新生成、重新构建。三台主机应使用同一 commit 和同一份节点配置版本。

## 4. 推荐实施顺序

1. **画环并定配置**：写出三台节点的 `node_id`、predecessor、successor、内网 IP/端口；先由组员交叉核对区间覆盖整个环且不重叠。
2. **实现单节点 KV**：本地 `Put/Get` 先能保存并读回任意字节；日志打印 `node`、`key`、`hash`、`owner`、`hops`。
3. **实现静态转发**：先只实现 `Put`，再对称实现 `Get`；不要在一次调试中同时改哈希、区间判断和网络配置。
4. **接入订单服务**：删掉内存订单表的读写路径；运行 1A 的两笔订单，确认客户端 API 和报价不变。
5. **部署三台 CVM**：每台仅启动自己的 DHT 节点；入口可以固定为 node-a，也可以让 trip-service 配置一个入口代理。服务间地址使用组网可达地址，不将 Docker daemon 暴露到公网。
6. **做验收与故障实验**：按照下一节记录命令、日志与现象，再打包提交。

## 5. 验收任务

### A. 正常链路

1. 展示三台 CVM 和三台 DHT 节点均在运行的证据（主机名、私网地址、容器状态）。
2. 用至少 6 个不同 key 进行 `Put` 后 `Get`；选择 key 使三个责任节点都至少保存一条记录。
3. 每个 key 至少从一个**非责任节点**入口发起一次请求；日志必须展示逐跳转发与最终 owner。
4. 用 1A 的客户端创建并查询两笔订单；订单价格和 `WAITING` 状态保持正确，且日志证明订单经过 DHT Put/Get。

### B. 故障观察

1. 选定一个已知责任节点，先记录其负责的一个 key 与一条不由它负责的 key 的正常读取路径。
2. 停止该节点的 DHT 容器或进程；不得伪造超时、不得改客户端结果。
3. 分别重试上述两个读取，原样记录成功、失败、超时或路由中断现象，以及每一跳日志。
4. 解释为什么会出现该现象。基础版没有副本和修复，因此“数据不可读”或“到 successor 的 RPC 失败”都可能是正确观察；评分看证据和解释，不看你把它说成自动恢复。

## 6. 提交物与评分

提交一个不含私钥、密码、云密钥、镜像凭据或容器大文件的压缩包，至少包括：

- 完整可构建源码、`proto/dht.proto`、三节点配置模板、Docker/启动脚本和固定 commit hash；
- `README`：从空白 CVM 到三节点运行的命令，以及所需端口；
- 三节点运行、跨节点 Put/Get、1A 订单兼容、故障实验的原始日志或截图；
- 已完成的 [report-template.md](report-template.md)；
- AI/人工分工说明，以及每位成员的贡献表。

| 评分项 | 分值 | 满分标准 |
|---|---:|---|
| 哈希环与归属判断 | 20 | 区间、回绕、owner 与三节点配置正确 |
| Put/Get 与 successor 路由 | 25 | 非责任节点逐跳转发，hops 与日志可验证 |
| 订单服务集成 | 20 | 不改客户端 API，订单读写真实经过 DHT |
| 三台 CVM 部署 | 15 | 三个独立节点可复现运行，配置与端口说明完整 |
| 故障实验与解释 | 10 | 原始证据充分，解释符合无副本静态环的边界 |
| 报告、贡献与答辩 | 10 | 分工可追溯，AI/人工说明具体，成员能解释路由 |

基础部分共 100 分；实现副本、明确故障切换策略并实测，可按课程规则获得加分。教师可随机指定一个 key 和入口节点，要求任一成员现场说明 `Hash`、区间判断与完整路由链。

## 7. 常见误区

- **把三个节点各自的内存表当成 DHT**：没有 owner 判断和远端转发，不是 DHT。
- **只从 owner 本机测试**：这绕开了最核心的远端路由。
- **杀节点后宣称系统“自动恢复”**：基础版没有副本、成员管理或修复，必须如实报告。
- **把公网 IP 写进仓库**：使用示例配置和本地 `.env`；不提交私网拓扑、密码或私钥。
- **让 AI 代替实测**：AI 可以协助读日志和写脚手架，但不能替你证明三台真实机器、路由分布和故障现象。
