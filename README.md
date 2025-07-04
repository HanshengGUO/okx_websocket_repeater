# okx_websocket_repeater

## Results
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
Total matching seqId pairs: 265
Repeater was faster: 118 times.
OKX direct feed was faster: 147 times.
Average latency (repeater_time - okx_time): -0.8169 ms
(A negative value means the repeater is faster on average)
```

在这类更高频率的数据上，我们的repeater没能在更多时候更快，但是仍然在40%的情况下占优，并且获得了0.8ms的平均领先。