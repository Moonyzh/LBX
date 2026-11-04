# 实验一 A：基于 tRPC 的网约车订单微服务

这是正式实验一“mini-DHT 分布式存储”的第一阶段。我们沿用课程中的打车开发故事：先把订单与计价从单体应用拆成两个 tRPC 微服务，再在实验 1B 中把订单的内存仓储替换成三节点 DHT。

本阶段只完成单机或单台云主机上的 RPC、容器编排和故障观察；三台 CVM 组网、哈希环路由、节点故障和数据行为属于实验 1B。

## 1. 系统故事与调用链

乘客发起行程后，订单服务向计价服务取得报价，再创建状态为 `WAITING` 的订单：

```text
ride-client ──CreateRide/GetTrip──> trip-service ──EstimateFare──> pricing-service
                                     │
                                     └── 1A：内存订单表
                                         1B：DHT Put/Get
```

- `ride-client`：模拟乘客创建和查询订单。
- `trip-service`：拥有订单，调用计价服务，但不复制计价规则。
- `pricing-service`：唯一维护起步价、里程价和高峰倍率。

完成 1A 后，应能解释 Proto 契约、客户端 stub、服务端实现、服务间 RPC、YAML 配置、Compose 网络及下游故障的传播方式。

## 2. 课前环境准备（不计入课堂 80 分钟）

### 方案 A：腾讯云 Ubuntu CVM

1. 在腾讯云控制台新建 CVM，计费模式选**按量计费**，镜像选 **Ubuntu Server 24.04 LTS 64 位（x86_64）**，建议至少 2 核 CPU、4 GiB 内存。
2. 在订单确认页选择当前账号可用的**学生优惠券/代金券**。能否使用取决于券的地域、有效期和订单限制；创建按量实例时仍可能需要余额承担冻结金额。
3. 配置公网 IPv4。安全组入站只需开放 TCP 22，并尽量把来源限制为自己的公网 IP。拉取镜像使用出站 HTTPS，不要开放 Docker 2375 端口。
4. 待实例运行后登录：`ssh ubuntu@<CVM公网IP>`。
5. 安装 Docker 与 Compose：

   ```bash
   sudo apt update
   sudo apt install -y docker.io docker-compose-v2
   sudo systemctl enable --now docker
   sudo usermod -aG docker "$USER"
   exit
   ```

6. 重新 SSH 登录，再确认 `docker version` 和 `docker compose version` 可直接运行。

加入 `docker` 用户组后才能直接运行本实验脚本。该用户组具有接近 root 的主机权限，只应加入受信任的实验账号。

### 方案 B：本机 WSL2

启动 Docker Desktop，确认 WSL integration 已开启，然后在 WSL 终端运行 `docker version` 和 `docker compose version`。本实验不要求 Windows 防火墙允许 Docker Backend 接收入站公网连接。

### 拉取并验证课程基础镜像

TCR 仓库是公有仓库，学生不需要教师账号，也不需要 `docker login`：

```bash
docker pull ccr.ccs.tencentyun.com/ccnu-cloudcomputing/trpc-cpp-dev:2026.09
docker tag ccr.ccs.tencentyun.com/ccnu-cloudcomputing/trpc-cpp-dev:2026.09 course/trpc-cpp-dev:2026.09
docker run --rm course/trpc-cpp-dev:2026.09 \
  bash -lc 'test -s /usr/local/trpc-cpp/trpc-2024-07/lib/libtrpc.a && /usr/local/trpc-cpp/trpc-2024-07/bin/protoc --version'
```

预期输出为 `libprotoc 3.15.8`。这条路径已在腾讯云 Ubuntu 24.04 x86_64 CVM 上实测通过。部分 CVM 无法访问 Docker Hub，因此不要用 `hello-world` 判断课程镜像是否可用。

## 3. 工程目录

| 目录 | 用途 |
|---|---|
| `proto/` | 订单与计价 RPC 契约 |
| `src/` | 两个服务和命令行客户端 |
| `config/` | tRPC 服务端、客户端配置 |
| `docker/` | 基础镜像、应用镜像与 Compose |
| `scripts/` | 启动、调用、验证、故障和结果收集 |
| `data/` | 固定测试输入 |
| `dist/` | 教师离线分发产物 |

每个子目录均有 README，说明文件职责和修改边界。

## 4. 课堂操作（80 分钟）

所有命令都在 `lab1-mini-dht` 目录执行。

### 0–10 分钟：读接口与服务边界

阅读两个 Proto 和三个组件，回答为什么 `trip-service` 应调用计价服务，而不能复制计价公式。

### 10–30 分钟：完成计价服务

在学生版的 `TODO(Lab1A-1)` 中实现确定性的离线计价：

```text
base_fare = 10.0
distance_fare = distance_km × 2.4
multiplier = peak ? 1.35 : 1.0
total_fare = (base_fare + distance_fare) × multiplier
```

校验 `city`、`distance_km` 和 `time_slot`；禁止调用真实地图、打车或支付 API。

### 30–50 分钟：完成订单服务的跨服务调用

在学生版的 `TODO(Lab1A-2)` 中：

1. 构造 `EstimateFareRequest`。
2. 通过 `PricingServiceProxy` 调用 `EstimateFare`。
3. 处理 RPC 失败和业务失败。
4. 生成订单号，保存报价和 `WAITING` 状态。
5. 让 `GetTrip` 从内存订单表读取订单。

### 50–60 分钟：构建并启动

```bash
bash scripts/up.sh
```

首次构建应用镜像通常需要几分钟，但不会重新编译 tRPC-Cpp。

### 60–68 分钟：验证正常链路

```bash
bash scripts/verify.sh
```

8.5 km 高峰订单应为 `41.04`，3.0 km 平峰订单应为 `17.20`。查看日志：

```bash
cd docker
docker compose logs --no-color trip pricing
cd ..
```

### 68–74 分钟：观察故障边界

```bash
bash scripts/fault-test.sh
```

记录计价服务停止后旧订单能否查询、新订单在哪一步失败，以及订单服务停止后的影响。

### 74–80 分钟：生成并在线提交结果

填写 [report-template.md](report-template.md)，再运行：

```bash
bash scripts/collect-result.sh
```

上传最新的 `submission/lab1a-*.tar.gz` 和同名 `.result.json`。不要提交云主机密码、SSH 私钥或 Registry 密码。

## 5. 验收条件

- 两笔固定订单均可创建并查询，价格和 `WAITING` 状态正确。
- 两个服务的日志能证明跨服务调用发生过。
- 故障脚本结果与报告解释一致。
- 能说明 1B 如何保持客户端 API 不变，把内存表替换为 DHT `Put/Get`。
- 报告包含 AI 与人工分工说明。

## 6. 实验结束

确认作业已经上传后，释放按量 CVM；只关机不等同于释放，相关资源仍可能计费。教师发布流程见 [INSTRUCTOR-GUIDE.md](INSTRUCTOR-GUIDE.md)。

