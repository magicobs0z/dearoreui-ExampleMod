# 课程 05 - Frame Data（时钟条 + 跨日刷新）

给日历加上**每帧驱动的时钟条**，并让"今天"跨午夜自动刷新 —— 周期性数据推送的正确姿势。

## 学什么

- `subscribeFrame(FrameSubscriptionOptions{owner}, cb)`：客户端每帧回调（`ClientInstance::update`，**主菜单也在跑**）。
- 回调运行在游戏主线程 → 可直接调 `publishEvent`（本客户端 publish 必须游戏线程）。
- 订阅/退订成对：Ready 订阅 → Destroyed 退订；本课按秒去重推送（每 30 帧取一次时间，同秒跳过）。

## 代码位置

- `src/mod/examples/ex05_frame_data.h`：`onReady` 订阅帧 → `onFrame` 推 `calendar.clock`。

## 成功标识

- 底部时钟条 `14:32:05` 每秒跳动（等宽数字）；
- 跨午夜后"今天"描边自动移到新日期（帧驱动比对 `ymd !== today` 触发重绘）。

## 切换方法

`config.json` → `"example": "05"`，重启游戏。

## 已知坑（周期源 —— 真机踩坑最重的一课）

- **客户端唯一可靠周期源 = 帧服务**。以下实测不可用（主菜单/启用期）：
  - LL `ClientLevelTickEvent` / `ServerLevelTickEvent`：发射器未就绪，且主菜单无 Level；
  - LL 协程 executor（`coro::execute` / `executeAfter`）：未触发；
  - 自建线程定时发布：publish 要求游戏主线程，跨线程直接调有风险。
- **频率** = 客户端帧率（谨慎推流；本课 30 帧一跳 + 同秒去重）。
- Destroyed 后继续推会喂给已死上下文 —— 一律成对退订。