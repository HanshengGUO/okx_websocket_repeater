# okx_websocket_repeater
本项目是一个WebSocket低延迟重复器，它通过同时从多个源（模拟多IP连接）订阅OKX交易所的同一WebSocket数据流，利用网络路由的微小差异以及OKX交易所的广播逻辑，捕获最先到达的消息。系统内置了高效的去重逻辑，确保只有最快、唯一的市场数据被转发到下游的交易策略客户端。

本项目同时提供了benchmarking脚本，该脚本可以根据项目的配置文件自动连接到repeater和OKX Direct Feed，比较该repeater在更真实场景下的性能表现。项目拥有较好的扩展性，理论上支持OKX WebSocket的全部Channel，并且已经在`books5`和`bbo-tbt`两个订单簿channel上完成了测试和benchmarking.

```
+--------------------------+
|   OKX Exchange Servers   |
+--------------------------+
      |               |
      |               |
(路径 A: 直接连接)   (路径 B: 多个并发连接)
      |               |
      |      +--------V--------+
      |      |                 |
      |      |   OKX Repeater  |
      |      | (去重并转发最快消息) |
      |      |                 |
      |      +--------V--------+
      |               |
      |               | (路径 C: 从Repeater输出的优化数据流)
      |               |
+-----V---------------+-----V---+
|                               |
|      Benchmark Tool           |
|  对比 路径 A 和 路径 C 的      |
|      消息到达时间              |
|                               |
+-----------------------------+
```

## 项目结构
```
(base) ubuntu@10-7-120-93:~/work/okx_websocket_repeater$ tree -I "build"
.
├── CMakeLists.txt              # 项目总CMakeLists
├── README.md                   
├── apps                        # 存放最终的可执行程序
│   ├── CMakeLists.txt          # 'apps' 目录的CMakeLists，用于生成可执行文件
│   ├── benchmark_main.cpp      # 基准测试程序，用于集成测试和量化性能
│   └── repeater_main.cpp       # Repeater主程序，启动服务
├── config                      # 存放配置文件
│   └── repeater_config.json    # 程序的配置文件
├── include                     # 存放公共头文件
│   └── repeater                # 库的命名空间目录，防止名称冲突
│       ├── message_processor.hpp      # 声明业务逻辑核心：消息去重与处理
│       ├── plain_websocket_client.hpp # 声明非加密(ws://)的WebSocket客户端
│       ├── repeater_core.hpp          # 声明应用协调器，组合所有模块
│       ├── websocket_client.hpp       # 声明加密(wss://)的WebSocket客户端 (连接OKX)
│       └── websocket_server.hpp       # 声明WebSocket服务器 (向下游广播)
└── src                         # 存放库的源代码实现 (.cpp文件)
    ├── CMakeLists.txt          # 'src' 目录的构建脚本，用于生成静态库(repeater_lib)
    ├── message_processor.cpp      # 实现消息去重逻辑
    ├── plain_websocket_client.cpp # 实现非加密WebSocket客户端
    ├── repeater_core.cpp          # 实现应用协调器
    ├── websocket_client.cpp       # 实现加密WebSocket客户端
    └── websocket_server.cpp       # 实现WebSocket服务器

5 directories, 17 files
```

## Quick Start

### 1. 环境
以 Ubuntu/Debian 为例：
```
sudo apt update
sudo apt install -y build-essential cmake libssl-dev libboost-all-dev nlohmann-json3-dev
```
### 2. Clone并进入build目录
```
git clone git@github.com:HanshengGUO/okx_websocket_repeater.git
cd okx_websocket_repeater/build
```
### 3. 编译项目
```
cmake ..
make -j8
```
### 4. 运行项目
#### repeater_main主项目
```
./apps/repeater_main
```
repeater_main是repeater的程序入口，它会根据配置文件的内容订阅OKX的某一数据源，并且转发最快的消息到一个新的WS Channel.
```
[RepeaterCore] Starting Repeater...
[WebSocketServer] Server starting on port 9002...
[WebSocketClient 1] Connected to OKX.
[WebSocketClient 2] Connected to OKX.
...
```
#### benchmark_main基准测试工具
此工具将同时连接到 OKX 和您本地运行的 Repeater 服务。连接成功后，它会开始接收消息并打印延迟对比结果，量化 Repeater 带来的性能提升。
```
./apps/benchmark_main
```
此工具会连续运行15秒，并且打印出15秒内的延迟统计结果（见下文Results Section）
### 配置文件 repeater_config.json
您可以按需要修改这个配置文件，来订阅订阅的Channel或调优性能。
```
{
  "debug": true,                       // 调试模式，true会输出日志

  "threads": 4,                        // 程序使用的线程池大小，用于处理网络I/O

  "repeater_server": {                 // 本地Repeater服务器的配置
    "host": "0.0.0.0",                 // 监听的IP地址。"0.0.0.0"表示监听所有网络接口
    "port": 9002                       // 监听的端口号，下游客户端（如benchmark）将连接此端口
  },

  "okx_connections": [                 // 要并发连接到OKX的WebSocket地址列表
    "wss://ws.okx.com:8443/ws/v5/public", // 每个地址代表一条独立的连接
    "wss://ws.okx.com:8443/ws/v5/public", // 多个连接可以增加接收到最快消息的概率
    "wss://ws.okx.com:8443/ws/v5/public",
    "wss://ws.okx.com:8443/ws/v5/public"
  ],

  "subscription_message": {            // 连接到OKX后要发送的订阅消息内容
    "op": "subscribe",
    "args": [                          // 订阅参数列表
      {
        "channel": "bbo-tbt",          // 订阅的频道
        "#channel": "books5",          // (注释) 备选频道
        "instId": "BTC-USDT"           // 订阅的产品ID：BTC-USDT交易对
      }
    ]
  }
}
```

## 项目实现简述
* 全异步I/O模型
  * 整个网络层基于 Boost.Asio 构建，所有网络操作（连接、读、写）均为非阻塞
* 线程池的使用：创建一个与CPU核心数匹配的线程池，所有线程共享并执行同一个 `io_context`
* 去重算法：采用“仅记录最大seqId”的策略。这是一个 O(1) 的整数比较操作，几乎无开销。
* 最小化锁竞争
  * 使用 std::mutex 保护共享的去重状态。
  * 锁的粒度被严格控制在最小范围：仅在读写 max_seq_id 或 unordered_set 的一个小范围内持有锁。
* 零拷贝
  * 传递 std::string_view，避免了重量级的字符串拷贝
  * 广播端: 使用`std::shared_ptr<const std::string>`传递数据。所有下游客户端会话共享同一份内存，而不是为每个连接复制一份数据。

## ⚠️注意
### 关于`MessageProcessor`的去重逻辑
当前的 MessageProcessor 实现采用了**仅处理最新消息**的策略。它只维护一个 max_seq_id（已处理过的最大序列号）。

当收到新消息时，它会进行如下判断：

* 如果新消息的 seqId 大于 当前的 max_seq_id，则认为该消息是全新的、有效的，会将其转发，并更新 max_seq_id。
* 如果新消息的 seqId 小于或等于 当前的 max_seq_id，则该消息被视为过时的乱序消息或重复消息，并被直接丢弃。

这种设计内存和计算占用低，避免了内存泄露，非常适用于那些我们只关心最新数据快照的场景。例如：

* 全量订单簿 (Full Order Book, e.g., books, books5): 每一条消息都包含完整的深度信息，直接覆盖旧的状态。
* 逐笔最优报价 (Best Bid/Offer, e.g., bbo-tbt): 我们只关心当前最新的最优买卖价。

但是对于增量频道，客户端需要在本地维护一个订单簿状态，并按顺序应用每一个 seqId 的更新。如果因为网络延迟，一个较旧的更新（例如 seqId=100）比一个较新的更新（seqId=101）更晚到达，本项目的 MessageProcessor 会丢弃 seqId=100 这条消息。这将导致客户端本地的订单簿状态出现数据缺口。

如果您需要支持增量数据频道，必须重新实现新的`MessageProcessor`，采用能够处理短时乱序的机制（例如基于 seqId 的滑动窗口去重），以确保所有数据更新都能被处理，保证数据流的完整性。

## Results
### 实验设定
以下的实验运行于base HK的四核8G内存的ubuntu22服务器，并且使用以下配置文件：
```
{
  "debug": true,
  "threads": 4,
  "repeater_server": {
    "host": "0.0.0.0",
    "port": 9002
  },
  "okx_connections": [
    "wss://ws.okx.com:8443/ws/v5/public",
    "wss://ws.okx.com:8443/ws/v5/public",
    "wss://ws.okx.com:8443/ws/v5/public",
    "wss://ws.okx.com:8443/ws/v5/public"
  ],
  "subscription_message": {
    "op": "subscribe",
    "args": [
      {
        "channel": "bbo-tbt",
        "#channel": "books5",
        "instId": "BTC-USDT"
      }
    ]
  }
}
```
### Books5（100ms）
`books5` 是一个提供深度快照的频道。它提供订单簿中**最佳的5个买单（bids）和卖单（asks）** 的深度数据。当订单簿发生变化时，数据会每100毫秒推送一次。如果在100毫秒的间隔内订单簿没有变化，则不会发送新的快照。
#### 运行结果
```
--- Benchmark Results ---
Total matching seqId pairs: 100
Repeater was faster: 71 times.
OKX direct feed was faster: 29 times.
Average latency (repeater_time - okx_time): -1.7558 ms
(A negative value means the repeater is faster on average)
```
我们的repeater在默认的实验设置下，70%数据上更快，并且获得了1.76ms的平均领先。

### BBO-TBT（10ms）
`bbo-tbt` 代表 “Best Bid Offer - Tick by Tick”，即逐笔最佳买卖盘。
它只提供订单簿中最顶层的数据，即一个最佳买单（best bid）和一个最佳卖单（best ask）。

虽然名字里有“TBT”（tick-by-tick），但其数据形式是快照，每次推送当前最优的买卖价。 当订单簿的顶层（即最佳买卖价）发生变化时，数据会每10毫秒推送一次。

```
--- Benchmark Results ---
Total matching seqId pairs: 229
Repeater was faster: 164 times.
OKX direct feed was faster: 65 times.
Average latency (repeater_time - okx_time): -0.6644 ms
(A negative value means the repeater is faster on average)
```

在这类更高频率的数据上，我们的repeater的平均领先降低至0.66ms，但仍然在约75%的情况下更快得到数据，这体现了这个repeater在不同频率大小的数据上具有一定的鲁棒性。