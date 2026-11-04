# 实验 1B · 节点 A 与部署集成（甲）

本文是甲的交付说明：在乙的 DHT 核心（`src/dht_common.*`、`src/dht_service.*`、`src/dht_server.cc`）之上，
把 trip-service 接入 DHT、并把三台机器的启动、验证、取证串起来。

- DHT 核心由乙提供，本文件不改动它。
- 订单 RPC 契约（`proto/trip.proto`）不变，`ride_client` 与 pricing-service 无感。

---

## 1. 环设计（与乙的 `dht_common` 实现一致）

哈希：`HashKey` = **32 位 FNV-1a**；归属区间为 `(predecessor_id, node_id]`，跨 0 时回绕。

| 节点 | node_id | predecessor_id | 服务名 | 监听 |
|---|---|---|---|---|
| node-a | `0x10000000` | `0xB0000000`（= node-c） | `trpc.dht.node-a.DhtService` | 19001 |
| node-b | `0x60000000` | `0x10000000`（= node-a） | `trpc.dht.node-b.DhtService` | 19001 |
| node-c | `0xB0000000` | `0x60000000`（= node-b） | `trpc.dht.node-c.DhtService` | 19001 |

前驱/后继环：

```
        ┌─────────────── 后继 ───────────────┐
        ▼                                    │
   node-a (0x10) ──▶ node-b (0x60) ──▶ node-c (0xB0)
        ▲                                    │
        └─────────────── 后继 ───────────────┘
   每台只需知道自己的 predecessor_id 和 successor 的客户端服务名
   （--node_id / --predecessor_id / --successor_service）
```

- 每个节点**只有一条出边**（后继），收到不属于自己的 key 就转发给后继，因此路由是 O(环长)。
- trip-service **只配置 node-a 一个 DHT 地址**，由 node-a 负责沿环转发。

### 1.1 固定订单号的落点（按乙的 FNV-1a 实测）

| key | HashKey | owner | 从 node-a 入口 |
|---|---|---|---|
| `trip:trip-000001` | `0x3FD60345` | node-b | node-a → node-b，1 跳 |
| `trip:trip-000002` | `0x3CD5FE8C` | node-b | 1 跳 |
| `trip:trip-000003` | `0x3DD6001F` | node-b | 1 跳 |

> 注意：连续订单号（`trip-000001`～`trip-000100`）在 FNV-1a 下只落在 **node-a / node-b** 两段区间
> （实测 51 / 49 分布），**node-c 不会成为 trip key 的 owner**。想演示"三个节点都存了数据"，用乙的
> `scripts/dht-local-test.sh` 里的 `key-0408 / key-0020 / key-0103`，那三个 key 正好覆盖 a/b/c。

## 2. 每台机器准备（三台相同）

1. 腾讯云 CVM ×3，Ubuntu 24.04，2 核 4 GiB。
2. 安全组放行 TCP 22，以及**三台之间的 TCP 19001**（入站来源填另外两台地址）。只开 22 会表现为 Put 转发失败。
3. 安装 Docker 与 Compose v2：

   ```bash
   sudo apt update && sudo apt install -y docker.io docker-compose-v2
   sudo systemctl enable --now docker && sudo usermod -aG docker "$USER"
   exit   # 重新登录生效
   ```

4. 拉课程基础镜像并打标签：

   ```bash
   docker pull ccr.ccs.tencentyun.com/ccnu-cloudcomputing/trpc-cpp-dev:2026.09
   docker tag ccr.ccs.tencentyun.com/ccnu-cloudcomputing/trpc-cpp-dev:2026.09 course/trpc-cpp-dev:2026.09
   ```

5. 把仓库同步到 `~/lab1-mini-dht`：

   ```bash
   git clone <团队仓库地址> ~/lab1-mini-dht     # 有共享仓库时
   # 或者：在本地 bash scripts/pack-repo.sh 后 scp 上传并解压
   ```

6. 构建镜像（**三台都要做**：B、C 只跑 dht_server，也需要本地镜像）：

   ```bash
   cd ~/lab1-mini-dht/docker && docker compose build pricing && cd ..
   ```

## 3. 配置生成（任务 3）

节点配置不进 git（`.gitignore` 里有 `config/*.cvm.yaml`），由脚本从模板渲染，避免把真实 IP 提交上去。

```bash
cd ~/lab1-mini-dht
export NODE_A_IP=<MACHINE-A-IP> NODE_B_IP=<MACHINE-B-IP> NODE_C_IP=<MACHINE-C-IP>
bash scripts/dht-gen-config.sh
```

产出 `config/dht-node-{a,b,c}.cvm.yaml`，其中：

| 文件 | admin_port | 后继服务 | 后继地址 |
|---|---|---|---|
| `dht-node-a.cvm.yaml` | 18081 | `trpc.dht.node-b.DhtService` | `<NODE_B_IP>:19001` |
| `dht-node-b.cvm.yaml` | 18082 | `trpc.dht.node-c.DhtService` | `<NODE_C_IP>:19001` |
| `dht-node-c.cvm.yaml` | 18083 | `trpc.dht.node-a.DhtService` | `<NODE_A_IP>:19001` |

地址也可以写在 `node-inventory.env` 里（从 `node-inventory.example.env` 复制），脚本会读
`NODE_A_HOST/NODE_B_HOST/NODE_C_HOST`。

## 4. trip-service 的 DHT 接入（任务 4、5、6）

改动集中在三处：

| 文件 | 改动 |
|---|---|
| `config/trip-service.yaml` | 新增客户端服务 `trpc.dht.node-a.DhtService` → `dht-node-a:19001`（固定入口） |
| `src/trip_service.h` | 去掉内存 map，增加 `dht_proxy_`、`GetDhtProxy()`、`kDhtEntryService` |
| `src/trip_service.cc` | `CreateRide` 做 `SerializeAsString → Put`；`GetTrip` 做 `Get → ParseFromString` |
| `CMakeLists.txt` | `trip_server` 链接 `${DHT_PB_SRCS}` 与 `${DHT_TRPC_PB_SRCS}` |

关键代码（`src/trip_service.cc`）：

```cpp
// CreateRide：定价成功后
const std::string dht_key = DhtKeyOf(trip_id);          // "trip:" + trip_id
const std::string dht_value = trip.SerializeAsString();
::trpc::dht::PutRequest put_request;
put_request.set_key(dht_key);
put_request.set_value(dht_value);
put_request.set_hops(0);
dht_context->SetFuncName("/trpc.dht.DhtService/Put");
...  // ok=false 时不返回 trip_id，避免"订单编号已发但环上没存"

// GetTrip
dht_context->SetFuncName("/trpc.dht.DhtService/Get");
...  // found=false → reply->set_found(false)
trip.ParseFromString(get_reply.value());
```

调用路径写的是 **proto 里的服务名** `/trpc.dht.DhtService/Put`，不是配置里的客户端别名
`trpc.dht.node-a.DhtService`——配置名只是本地寻址用的，这点和 pricing 的写法一致。

## 5. 三台启动（任务 7）

`docker/docker-compose.yml` 里三个节点都映射 19001，所以**每台只能起自己的那个**：

```bash
# 机器 A：node-a + pricing + trip
bash scripts/dht-up-node.sh a
# 机器 B：只起 node-b
bash scripts/dht-up-node.sh b
# 机器 C：只起 node-c
bash scripts/dht-up-node.sh c
```

脚本会：渲染配置 → 缺镜像时自动构建 → `docker compose up -d` 对应服务 → 打印本机的后继 target。

核对（脚本末尾也会打印）：A 指向 B 的 IP、B 指向 C 的 IP、C 指向 A 的 IP。

## 6. 验证（任务 7 收尾）

在机器 A 上：

```bash
bash scripts/dht-verify.sh
```

预期 11 项 PASS：三笔订单 `create_ok=1`、编号 `trip-000001..3`、票价仍是 `41.04 / 17.20`，
回读 `found=1`，不存在的订单 `found=0`，而且 trip-service 日志里每笔都有
`dht_key=trip:trip-00000X dht_owner=node-b dht_hops=1`。

手工看证据：

```bash
docker compose exec -T trip grep "dht_key" /tmp/trip-service.log
docker compose logs --tail=50 dht-node-a
```

## 7. 取证（任务 8）

```bash
export NODE_A_SSH=ubuntu@<MACHINE-A-IP> NODE_B_SSH=ubuntu@<MACHINE-B-IP> \
       NODE_C_SSH=ubuntu@<MACHINE-C-IP> REMOTE_DIR=~/lab1-mini-dht
bash scripts/dht-collect-evidence.sh
```

产物在 `data/evidence/lab1b/node-{a,b,c}/`：主机名、`docker compose ps`、脱敏后的节点配置、节点日志；
A 另含 trip / pricing 日志。这就是甲方任务 8 要的四类材料。

## 8. 从空白 CVM 到三节点运行（任务 9）

```bash
# ===== 安全组：三台互相放行 TCP 19001，另放行 22 =====

# ===== 每台 CVM（0. 环境）=====
ssh ubuntu@<MACHINE-X-IP>
sudo apt update && sudo apt install -y docker.io docker-compose-v2
sudo systemctl enable --now docker && sudo usermod -aG docker "$USER"
exit && ssh ubuntu@<MACHINE-X-IP>
docker pull ccr.ccs.tencentyun.com/ccnu-cloudcomputing/trpc-cpp-dev:2026.09
docker tag ccr.ccs.tencentyun.com/ccnu-cloudcomputing/trpc-cpp-dev:2026.09 course/trpc-cpp-dev:2026.09
git clone <团队仓库地址> ~/lab1-mini-dht
cd ~/lab1-mini-dht/docker && docker compose build pricing && cd ..

# ===== 每台 CVM（1. 生成配置 + 启动自己的节点）=====
cd ~/lab1-mini-dht
export NODE_A_IP=<MACHINE-A-IP> NODE_B_IP=<MACHINE-B-IP> NODE_C_IP=<MACHINE-C-IP>
bash scripts/dht-up-node.sh a     # 机器 B 用 b，机器 C 用 c

# ===== 机器 A（2. 验证与取证）=====
bash scripts/dht-verify.sh
export NODE_A_SSH=ubuntu@<MACHINE-A-IP> NODE_B_SSH=ubuntu@<MACHINE-B-IP> \
       NODE_C_SSH=ubuntu@<MACHINE-C-IP> REMOTE_DIR=~/lab1-mini-dht
bash scripts/dht-collect-evidence.sh
```

## 9. 常见故障

| 现象 | 原因 |
|---|---|
| `put ... forward` 失败 / `hops exceeded` | 安全组没放行 19001，或后继 IP 填错（比对 `config/dht-node-*.cvm.yaml`） |
| `trip` 起来但 `create_ok=0`，message 含 `dht put rpc failed` | trip 比 node-a 先就绪；等几秒重跑，或 `docker compose restart trip` |
| `dht entry node-a proxy is unavailable` | `config/trip-service.yaml` 里的客户端服务名被改坏 |
| B/C 启动报 `pull access denied` | 漏了 `docker compose build pricing` |
| 验证时 `found=0` 但订单刚创建过 | 容器重启过：DHT 存储是进程内 map，重启即清空 |
