# docker

本目录负责镜像和多容器编排。

- `Dockerfile.base`：教师课前构建 tRPC-Cpp 基础镜像；学生不运行。
- `Dockerfile`：基于课程镜像编译三个应用程序。
- `docker-compose.yml`：启动 `pricing`、`trip`，并按需运行 `client`。

从仓库根目录优先运行 `bash scripts/up.sh`。主机映射端口是 18001 和 18002；服务间调用使用 Compose 服务名及容器端口。

