# yacgc: Yet Another C Garbage Collector

[English](README.md) | [中文](README-CN.md)

一个用 C 语言从零实现的 GC 库（不依赖外部第三方库），目标是在“没有虚拟机”的前提下，尽可能提供类似 Go GC 的使用体验：业务侧主要使用 `gc_malloc` 进行分配，由 GC 负责回收。

本项目同时支持两种根集合获取方式：
- 保守式（Conservative）：通过扫描各线程的寄存器快照与栈内存，推断可能的指针并做标记。
- 精确式（Precise）：通过 shadow stack（精确根集合）保证“不会误保活”，适合常驻服务对内存稳定性要求较高的场景。

并且提供多线程 stop-the-world（STW）支持：在 GC 期间暂停其它线程，扫描它们的栈/寄存器/精确根，再恢复线程运行。

## 目录结构

- include/
  - gc.h：公共 API（分配/回收/统计/模式切换）
  - gc_conservative.h：保守模式相关 API
  - gc_precise.h：精确模式（shadow stack）相关 API
- src/
  - gc_core.c：分配 + mark/sweep 核心流程（调度不同模式的 root 扫描）
  - gc_conservative.c：保守模式 root 扫描
  - gc_precise.c：精确模式 shadow stack 与精确 root 扫描
  - gc_stack.c：跨平台获取线程栈边界
  - gc_threads.c：线程注册、全局锁、STW（POSIX/Windows）实现
- test/
  - example_conservative.c：保守模式示例（仅 `gc_malloc`）
  - example_precise.c：精确模式示例（shadow stack）
  - example_service.c：常驻服务风格示例（精确模式 + 周期性 GC）
  - example_multithread.c：多线程并发分配示例

## 构建与运行

- 编译静态库 + 动态库 + 示例可执行文件：

```bash
make all
```

- 运行示例（作为测试用例）：

```bash
make test
```

产物位于 `build/`：
- 静态库：`libyacgc.a`
- 动态库：按平台生成
  - macOS：`libyacgc.dylib`
  - Linux：`libyacgc.so`
  - Windows：`libyacgc.dll`

## 使用方式

### 1）默认（保守式）——业务只用 gc_malloc

保守模式默认启用，线程第一次调用 `gc_malloc` 会自动完成初始化与线程注册。典型业务代码只需要使用 `gc_malloc` 分配即可：

```c
#include "gc.h"

typedef struct Node { struct Node *next; int v; } Node;

static Node *build(int n) {
    Node *h = 0;
    for (int i = 0; i < n; i++) {
        Node *x = (Node *)gc_malloc(sizeof(Node));
        x->next = h;
        x->v = i;
        h = x;
    }
    return h;
}

int main(void) {
    for (int i = 0; i < 10000; i++) {
        Node *h = build((i % 64) + 1);
        if (h && h->next) h->v ^= h->next->v;
    }
    gc_shutdown();
    return 0;
}
```

注意：保守式扫描可能产生“误保活”（false retention），长期运行可能出现“像泄漏一样的缓慢增长”。如果需要可证明的内存稳定性，使用精确模式。

### 2）精确模式（shadow stack）——避免误保活

```c
#include "gc.h"
#include "gc_precise.h"

int main(void) {
    gc_set_mode(GC_MODE_PRECISE);
    gc_set_auto_collect(0);

    {
        gc_shadow_frame fr;
        gc_shadow_enter(&fr);

        void *p = gc_malloc(1024);
        GC_SHADOW_ROOT(&fr, p);

        gc_collect();
        gc_shadow_leave(&fr);
    }

    gc_collect();
    gc_shutdown();
    return 0;
}
```

精确模式的关键点是：GC 只扫描你显式登记的根（shadow stack），因此不会因为栈上残留的“假指针”而误保活对象。

## 技术设计方案

### 1）内存模型：非移动（non-moving）

本 GC 采用 non-moving 策略：
- 对象地址稳定（便于和普通 C 指针协作）
- 不做压缩/整理，简单可靠

代价是：可能产生碎片（依赖底层 `malloc` 的分配策略），也不具备对象搬迁带来的局部性优化。

### 2）分配：gc_malloc/gc_free

每次分配会创建一个带头部的块：
- 头部保存 `size/marked/next`
- 返回给用户的是头部后面的 payload 指针

所有对象通过单链表 `g_blocks` 维护，回收阶段 sweep 时将不可达对象摘链并 `free`。

### 3）标记-清扫（Mark & Sweep）

GC 一次周期：
1. 清空所有对象的 `marked`
2. 扫描 roots，将可达对象入栈（mark stack）
3. 反复弹出对象并扫描对象内存，把其中“看起来像指针”的值当作潜在引用继续标记
4. sweep：释放未标记对象

这是保守式的对象扫描：对象 payload 会按机器字宽逐字扫描，将每个机器字当作候选指针。

### 4）两种 root 获取模式

#### A. 保守模式（Conservative）

根集合来源：
- 各线程寄存器快照（通过 `setjmp` 把寄存器值落栈或复制到可扫描内存）
- 各线程栈范围（扫描 SP 到 stack top / 或 stack bottom 到 SP，取决于栈增长方向）

跨平台栈边界：
- macOS：`pthread_get_stackaddr_np/pthread_get_stacksize_np`
- Linux：`pthread_getattr_np + pthread_attr_getstack`
- Windows：优先 `GetCurrentThreadStackLimits`，否则用 `VirtualQuery` 估算区域

#### B. 精确模式（Precise）

根集合来源：
- shadow stack（每个线程一个 TLS 的 frame 链）

业务在需要跨 GC 保活的局部指针上调用 `GC_SHADOW_ROOT(&fr, p)` 登记。GC 扫描所有线程登记的 shadow roots。

### 5）多线程与 stop-the-world（STW）

本项目采用 STW 来保证并发安全的可达性分析：
- 在进入 `gc_collect()` 时请求其它线程暂停
- 扫描其它线程的寄存器快照与栈（保守模式）/shadow roots（精确模式）
- sweep 完成后恢复其它线程继续执行

实现方式：
- POSIX：通过 `pthread_kill(SIGUSR1)` 触发信号处理器，将线程“停住”并记录 SP/寄存器快照；GC 线程等待所有目标线程进入 parked 状态后继续。
- Windows：通过 `SuspendThread/GetThreadContext` 获取目标线程上下文并暂停，结束后 `ResumeThread`。

### 6）可移植性边界与限制

GC 在纯 C 环境下做“像 Go 一样”的体验，必然存在边界：
- 保守模式存在误保活风险（概率问题），适合“易用优先”的场景
- 精确模式需要业务显式登记根，换取可证明的回收正确性与稳定性
- STW 在 POSIX 依赖信号，对实时性敏感场景需谨慎（但实现上是标准做法）
