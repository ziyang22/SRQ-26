# Scripts

只放可复用的本地/远程执行脚本，不放 C/C++ 源码。

| 脚本 | 用途 |
|---|---|
| `remote-probe.sh` | 只读探测远程节点的 CPU、NUMA、内存、编译器和 OpenMP 情况 |
| `remote-run.sh` | 从公开 GitHub 同步后，在远程节点 32 逻辑 CPU 内运行 |
| `check-layout.sh` | 检查源码是否放在规定目录 |

## remote-run.sh 行为

```sh
scripts/remote-run.sh test
scripts/remote-run.sh benchmark
```

- 拒绝在本地工作区不干净、未提交或未推送时运行；
- 远程只做 `fetch` + `merge --ff-only`，HEAD 不一致就退出；
- 远程工作区不干净时拒绝覆盖；
- 以 `taskset -c 0-31` 和 `OMP_NUM_THREADS=32` 运行。

可用环境变量覆盖连接与资源参数：

```text
SRQ_REMOTE_HOST   默认 global.prd.ga.launchpad.nvidia.com
SRQ_REMOTE_PORT   默认 17255
SRQ_REMOTE_USER   默认 nvidia
SRQ_REMOTE_DIR    默认 /home/nvidia/SRQ-26
SRQ_REMOTE_CPUS   默认 32，允许范围 1-32
```

认证由本机 SSH 配置或 SSH agent 提供；脚本不读取也不写入任何凭据。
