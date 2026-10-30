# 实验检查点 2：TCP 接收器

**截止时间：**10 月 18 日（星期日）晚上 11:59

**协作政策：**与检查点 0 相同。请不要查看其他同学的代码，也不要查看往年版本作业的聊天机器人代码或解答。请在报告中完整披露所有合作者以及任何可能存在疑问的情况——如实披露是最稳妥的做法。

## 0 概述

*建议：在开始实现之前，先通读整份实验文档。*

在检查点 0 中，你实现了“流量控制字节流”（flow-controlled byte stream，ByteStream）的抽象。在检查点 1 中，你创建了一个 Reassembler，它接收一系列子字符串；这些子字符串都截取自同一个字节流，并将它们重新组装回原始字节流。

这些模块对于实现 TCP 非常有用，但它们本身都没有涉及传输控制协议（Transmission Control Protocol）的具体细节。现在情况有所不同。在检查点 2 中，你将实现 TCPReceiver，即 TCP 实现中负责处理传入字节流的部分。

TCPReceiver 通过 `receive()` 方法接收来自对端发送器的消息，并将这些消息转换为对 Reassembler 的调用；Reassembler 最终会把数据写入输入 ByteStream。应用程序从这个 ByteStream 中读取数据，就像你在实验 0 中通过 TCPSocket 读取数据一样。

与此同时，TCPReceiver 还会通过 `send()` 方法生成返回给对端发送器的消息。这些“接收器消息”负责告诉发送器：

1. “第一个尚未组装的”字节的索引，这个索引称为“确认号”（acknowledgment number，或 `ackno`）。它表示接收器还需要从发送器获得的第一个字节。
2. 输出 ByteStream 的可用容量，这称为“窗口大小”（window size）。

`ackno` 和窗口大小共同描述了接收器窗口：TCP 发送器允许发送的索引范围。接收器利用这个窗口控制传入数据的流量，使发送器在接收器准备好接收更多数据之前限制发送量。我们有时会把 `ackno` 称为窗口的“左边界”（即 TCPReceiver 感兴趣的最小索引），把 `ackno + window size` 称为“右边界”（即 TCPReceiver 感兴趣的最大索引之后的位置）。

在你编写 Reassembler 和 ByteStream 时，实际上已经完成了实现 TCPReceiver 所需的大部分算法工作；本实验主要是把这些通用类与 TCP 的具体细节连接起来。最困难的部分将涉及理解 TCP 如何表示流中每个字节的位置——这种表示称为“序列号”（sequence number）。

## 1 开始操作

TCPReceiver 的实现使用与检查点 0 和 1 相同的 Imp 库，同时增加了一些类和测试。开始前请完成以下步骤：

1. 确认你已经提交了检查点 1 的全部解答。请不要修改顶层 `src` 目录之外的任何文件，也不要修改 `webget.cc`。否则，合并检查点 2 的起始代码时可能会遇到问题。

2. 在实验作业仓库目录中运行 `git fetch --all`，以获取最新版本的实验作业。

3. 运行以下命令下载检查点 2 的起始代码：

   `git merge origin/check2-startercode`

   如果你把名为 `origin` 的远程仓库重命名成了其他名称，这里可能需要使用不同的名称，例如：

   `git merge upstream/check2-startercode`

4. 确认构建系统已经正确设置：

   `cmake -S . -B build`

   **针对 arm64（UTM）Mac 用户的说明：**过去，g++ 的“sanitizer”（错误检查器）在 arm64 上运行得非常慢。Imp 使用这些工具运行测试。如果你使用的是 arm64 Mac 并遇到问题，可以配置 CMake 使用其他编译器：

   ```bash
   cmake -S . -B build -DCMAKE_CXX_COMPILER=clang++
   ```

5. 编译源代码：

   `cmake --build build`

6. 打开并开始编辑 `writeups/check2.md` 文件。该文件是实验报告模板，提交时也会包含在你的作业中。

## 2 检查点 2：TCP 接收器

TCP 是一种协议，它通过不可靠的数据报网络，可靠地传输一对受流量控制的字节流——连接的每个方向各有一个字节流。两个参与者（或称“对端”）建立 TCP 连接，并且**每个对端**同时既是自己输出字节流的“发送器”（sender），也是输入字节流的“接收器”（receiver）。

本周你将实现 TCP 的“接收器”部分。它负责接收来自发送器的消息，重新组装字节流（包括在结束时处理字节流终止），并确定应该发送回发送器、用于确认和流量控制的消息。

*⋆我为什么要做这个？* 这些信号对于 TCP 在不可靠的数据报网络上提供受流量控制且可靠的字节流服务至关重要。在 TCP 中，确认的含义是：“接收器为了继续组装 ByteStream，接下来需要哪个字节的索引？”这会告诉发送器哪些字节需要发送或重新发送。流量控制的含义是：“接收器对哪些索引范围感兴趣并且愿意接收？”（这取决于接收器的可用容量。）这会告诉发送器它最多被允许发送多少数据。

### 2.1 在 64 位索引和 32 位 seqno 之间转换

作为热身，我们需要实现 TCP 表示索引的方式。上周你实现了一个 Reassembler，它重新组装子字符串；每个字节都有一个 64 位的流索引，并且流中的第一个字节总是索引 0。64 位索引足够大，因此我们可以把它视为永远不会溢出。¹但是，在 TCP 首部中，空间非常宝贵，所以流中每个字节的索引不用 64 位索引表示，而是用 32 位“序列号”（sequence number，或 `seqno`）表示。这带来了三个复杂点：

1. **实现必须考虑 32 位整数回绕。**

   TCP 中的流可以任意长——通过 TCP 发送的 ByteStream 没有长度上限。但 `2^32` 个字节只有 4 GiB，并不算特别大。当 32 位序列号递增到 `2^32 - 1` 后，流中的下一个字节的序列号就会变成 0。

   ```text
   2^32
   2^32 - 1
   ```

2. **TCP 序列号从一个随机值开始：**

   为了提高健壮性，并避免把属于同一端点之间早期连接的旧报文段误认为当前连接的一部分，TCP 会尽量确保序列号难以猜测，并且不太可能重复。因此，某个流的序列号不会从 0 开始。流中的第一个序列号是一个**随机的 32 位数**，称为初始序列号（Initial Sequence Number，ISN）。这个序列号表示流的“零点”，即 SYN（流的开始）。此后的序列号按正常方式递增：第一个数据字节的序列号为 `ISN + 1 (mod 2^32)`，第二个数据字节的序列号为 `ISN + 2 (mod 2^32)`，以此类推。

3. **逻辑上的开始和结束各占用一个序列号：**

   除了确保所有数据字节都被可靠接收之外，TCP 还要确保流的开始和结束被可靠接收。因此，在 TCP 中，SYN（流开始）和 FIN（流结束）控制标志也被分配了序列号。它们各占用**一个**序列号。（SYN 标志占用的序列号就是 ISN。）流中的每个数据字节也占用一个序列号。请注意，SYN 和 FIN 不属于流本身，也不是“字节”——它们表示字节流的开始和结束。

这些序列号（seqno）被写入每个 TCP 报文段的首部。（此外，TCP 有两个方向的流；每个方向都有独立的序列号以及不同的随机 ISN。）有时，讨论“绝对序列号”（absolute sequence number）也很有帮助：它从 0 开始并且不会回绕；还可以讨论“流索引”（stream index），也就是你在 Reassembler 中一直使用的索引：从 0 开始，为流中的每个数据字节编排索引。

为了具体说明这些区别，考虑只包含三个字母 `cat` 的字节流。如果 SYN 恰好有序列号 `2^32 - 2`，那么每个字节的序列号、绝对序列号和流索引如下：

**Fall 2026（2026 年秋季）**

| 元素 | SYN | c | a | t | FIN |
|---|---:|---:|---:|---:|---:|
| seqno | `2^32 - 2` | `2^32 - 1` | `0` | `1` | `2` |
| absolute seqno | `0` | `1` | `2` | `3` | `4` |
| stream index | — | `0` | `1` | `2` | — |

上图展示了 TCP 中涉及的三种不同索引：

### 序列号（Sequence Numbers）

- 从 ISN 开始；
- 32 位，会回绕；
- 包含 SYN/FIN；
- 也称为 `seqno`。

### 绝对序列号（Absolute Sequence Numbers）

- 从 0 开始；
- 64 位，不会回绕；
- 包含 SYN/FIN；
- 也称为“absolute seqno”。

### 流索引（Stream Indices）

- 从 0 开始；
- 64 位，不会回绕；
- 不包含 SYN/FIN；
- 也称为“stream index”。

在绝对序列号和流索引之间转换很简单：只需加一或减一。不幸的是，在序列号和绝对序列号之间转换要困难一些，而且混淆二者会导致难以发现的错误。为了系统地避免这些错误，我们用自定义类型 `Wrap32` 表示序列号，并实现它与绝对序列号（用 `uint64_t` 表示）之间的转换。`Wrap32` 是一种*包装类型*（wrapper type）：它包含一个内部类型（这里是 `uint32_t`），但为该类型提供了另一组函数和运算符。

我们已经为你定义了这个类型并提供了一些辅助函数，但你需要在 `wrapping_integers.cc` 中实现以下转换：

1. `static Wrap32 Wrap32::wrap(uint64_t n, Wrap32 zero_point)`

   将绝对序列号转换为序列号：`absolute seqno → seqno`。给定一个绝对序列号 `n` 和一个初始序列号（`zero_point`），生成 `n` 对应的（相对）序列号。

2. `uint64_t unwrap(Wrap32 zero_point, uint64_t checkpoint) const`

   将序列号转换为绝对序列号：`seqno → absolute seqno`。给定一个序列号（即这个 `Wrap32` 值）、初始序列号和一个绝对序列号 `checkpoint`，找到与该 checkpoint 最接近的对应绝对序列号。

**注意：**需要 checkpoint，是因为任意一个 seqno 都对应着**许多**绝对序列号。例如，当 ISN 为 0 时，seqno `17` 对应的绝对序列号可以是 `17`，也可以是 `2^32 + 17`、`2^33 + 17`、`2^33 + 2^32 + 17`、`2^34 + 17`、`2^34 + 2^32 + 17`，等等。

checkpoint 用于消除这种歧义：它是一个绝对序列号，调用这个类的代码知道正确答案应该大致位于它附近。在 TCP 实现中，你将使用第一个尚未组装的索引作为 checkpoint。

**提示：**使用 `wrapping_integers.hh` 中提供的辅助函数，会得到最简洁、最容易实现的代码。`wrap`/`unwrap` 操作应当保持偏移量：相差 17 的两个 seqno，应当对应相差 17 的两个绝对序列号。

**提示 2：**我们预计 `wrap` 只需要一行代码，而 `unwrap` 少于 10 行代码。如果你发现自己写了多得多的代码，最好停下来重新思考，尝试采用不同的策略。

你可以运行以下测试来检查实现：

`cmake --build build --target check2`

（提醒：使用 arm64 Mac 的用户可能需要配置 CMake 使用 `clang++` 编译器，具体见上文。）

### 2.2 实现 TCP 接收器

恭喜你正确实现了回绕和解回绕逻辑！如果是在实验课上，我们会和你握手（或者在疫情之后，和你碰肘）庆祝这次胜利。在本实验的剩余部分，你将实现 TCPReceiver。它将完成以下工作：

1. 接收来自对端发送器的消息，并使用 Reassembler 重新组装 ByteStream；
2. 向对端发送器返回包含确认号（ackno）和窗口大小的消息。

我们预计这一部分总共需要大约 15 行代码。

首先回顾 TCP“发送器消息”的格式。该消息包含有关 ByteStream 的信息，并由某个 TCPSender 发送给它的对端 TCPReceiver：

```cpp
/*
 * The TCPSenderMessage structure contains five fields (imp/util/tcp_sender_message.hh):
 *
 * 1) The sequence number (seqno) of the beginning of the segment. If the SYN flag is set,
 *    this is the sequence number of the SYN flag. Otherwise, it's the sequence number of
 *    the beginning of the payload.
 *
 * 2) The SYN flag. If set, this segment is the beginning of the byte stream, and the seqno field
 *    contains the Initial Sequence Number (ISN) -- the zero point.
 *
 * 3) The payload: a substring (possibly empty) of the byte stream.
 *
 * 4) The FIN flag. If set, the payload represents the ending of the byte stream.
 *
 * 5) The RST (reset) flag. If set, the stream has suffered an error and the connection
 *    should be aborted.
 */

struct TCPSenderMessage
{
  Wrap32 seqno { 0 };

  bool SYN {};
  std::string payload {};
  bool FIN {};

  bool RST {};

  // How many sequence numbers does this segment use?
  size_t sequence_length() const { return SYN + payload.size() + FIN; }
};
```

**Fall 2026（2026 年秋季）**

TCPReceiver 会向对端 TCPSender 生成自己的消息：

```cpp
/**
 * The TCPReceiverMessage structure contains three fields (imp/util/tcp_receiver_message.hh):
 *
 * 1) The acknowledgment number (ackno): the next sequence number needed by the TCP Receiver.
 *    This is an optional field that is empty if the TCPReceiver hasn't yet received the
 *    Initial Sequence Number.
 *
 * 2) The window size. This is the number of sequence numbers that the TCP receiver is interested
 *    to receive, starting from the ackno if present. The maximum value is 65,535 (UINT16_MAX from
 *    the <cstdint> header).
 *
 * 3) The RST (reset) flag. If set, the stream has suffered an error and the connection
 *    should be aborted.
 */
struct TCPReceiverMessage
{
  std::optional<Wrap32> ackno {};
  uint16_t window_size {};
  bool RST {};
};
```

你的 TCPReceiver 的工作，就是接收其中一种消息，并发送另一种消息：

```cpp
class TCPReceiver
{
public:
  // Construct with given Reassembler
  explicit TCPReceiver( Reassembler&& reassembler ) : reassembler_( std::move( reassembler ) )

  // The TCPReceiver receives TCPSenderMessages from the peer's TCPSender.
  void receive( TCPSenderMessage message );

  // The TCPReceiver sends TCPReceiverMessages to the peer's TCPSender.
  TCPReceiverMessage send() const;

  // Access the output (only Reader is accessible non-const)
  const Reassembler& reassembler() const { return reassembler_; }
  Reader& reader() { return reassembler_.reader(); }
  const Reader& reader() const { return reassembler_.reader(); }
  const Writer& writer() const { return reassembler_.writer(); }

private:
  Reassembler reassembler_;
};
```

### 2.2.1 `receive()`

每当从对端发送器收到一个新的报文段时，就会调用这个方法。该方法需要完成以下工作：

- **必要时设置初始序列号。**第一个到达且设置了 SYN 标志的报文段的序列号，就是初始序列号。你需要记录它，以便在 32 位回绕序列号/确认号与绝对序列号之间进行转换。（注意，SYN 只是首部中的一个标志。同一条消息也可能同时携带数据，或者设置 FIN 标志。）

- **把数据推送给 Reassembler。**如果 TCP 报文段首部设置了 FIN 标志，这意味着 payload 的最后一个字节就是整个流的最后一个字节。请记住，Reassembler 需要从 0 开始的流索引；因此你必须先解回绕 seqno，得到这些索引。

- **不要重复保存其他组件已经提供的状态或功能。**你已经构建了 ByteStream、Reassembler 和 Wrap32 模块，它们已经提供了许多有用的功能。TCPReceiver 不应该保存任何这些模块已经处理的状态（成员变量），也不应该重复实现它们已有的功能。

## 3 开发与调试建议

1. 在 `tcp_receiver.cc` 文件中实现 TCPReceiver 的公共接口（以及你想添加的任何私有方法或函数）。你可以向 `tcp_receiver.hh` 中的 TCPReceiver 类添加私有成员。

2. 可以使用以下命令测试代码：

   `cmake --build build --target check2`

3. 请重新阅读实验 0 文档中关于“使用 Git”的部分，并记住要把代码保留在实验发放时提供的 Git 仓库的 `main` 分支中。提交代码时应当进行小步提交，并使用能够说明“修改了什么以及为什么修改”的良好提交信息。

4. 请努力让代码具有良好的可读性。为变量使用合理、清晰的命名约定。使用注释解释复杂或微妙的代码部分。采用“防御式编程”：明确检查函数的前置条件或不变量，如果出现任何问题就抛出异常。使用模块化设计：识别共用的抽象和行为，并尽可能将它们提取出来。重复代码块和过于庞大的函数会使代码更难理解。

5. 还请遵守检查点 0 文档中介绍的“现代 C++”风格。`cppreference` 网站（https://en.cppreference.com）是很好的参考资源，不过你不需要使用任何复杂的 C++ 特性来完成这些实验。

## 4 提交

1. 提交时，请只修改 `src` 目录中的 `.hh` 和 `.cc` 文件。在这些文件中，你可以根据需要添加私有成员，但请不要修改任何类的公共接口。

2. 提交任何作业之前，请依次运行以下命令：

   **(a)** 确认你已经将所有修改提交到 Git 仓库。可以运行 `git status`，确认没有未提交的修改。记住，编写代码时应当进行小步提交。

   **(b)** `cmake --build build --target format`（规范化代码风格）

   **(c)** `cmake --build build --target check2`（确保自动化测试通过）

   **(d)** 可选：`cmake --build build --target tidy`（提出遵循良好 C++ 编程实践的改进建议）

3. 在 `writeups/check2.md` 中写一份报告。该文件应当是一个大约 20 至 50 行的文档，每行不超过 80 个字符，以便阅读。报告应包含以下部分：

   **(a) 程序结构与设计（Program Structure and Design）**

   描述代码体现的高层结构和设计选择。不需要详细讨论你从起始代码中继承的部分。可以借此机会突出重要的设计方面，并对这些方面进行更详细的说明，以便评分助教理解。强烈建议使用小标题和提纲，使报告尽可能易读。请不要只是把程序逐行翻译成一段英文。

   **(b) 你考虑过的其他设计选择（Alternative design choices）**

   描述你曾考虑过的其他设计选择，或者从理想角度评价它们的性能、编写难度（例如，需要多少小时才能写出没有 bug 的实现）、可读性难度（例如，代码行数以及其中微妙或不明显的正确性），以及其他你认为对读者或过去的自己有趣的维度。如果适用，请给出测量结果。

   **(c) 实现挑战（Implementation Challenges）**

   描述你认为最棘手的代码部分，并解释为什么它们困难。反思你是如何克服这些挑战的，以及是什么帮助你最终理解了之前感到困惑的概念。说明你尝试采用哪些方法来确保代码维持自己的假设、不变量和前置条件，以及哪些方面容易或困难。你是如何调试和测试代码的？

   **(d) 遗留错误（Remaining Bugs）**

   尽可能指出并解释代码中仍然存在的 bug（或尚未处理的边界情况）。

4. 在报告中还要填写这项作业花费的小时数以及其他评论。

5. 如果在实验课上遇到任何问题，请尽快告知课程工作人员，或者在 Ed 上发布问题。祝好运！

## 5 额外加分

如果改进测试套件，将获得额外加分。在 `tests` 目录中的某个文件里添加一个测试用例（例如 `imp/tests/recv_connect.cc`），该测试应当能够发现某个同学可能合理犯下、但现有测试套件尚未覆盖的真实 bug。请将测试作为 Pull Request 提交（公开提交也可以），这样我们可以查看它，并决定是否将其加入整体测试套件。

这个机会会持续开放。例如，如果你在第 7 周发现了一个针对 Reassembler 的优秀额外测试，也可以提交。

---

## 术语对照

- **ByteStream**：字节流
- **Reassembler**：重组器／重组模块
- **TCPReceiver**：TCP 接收器
- **TCPSender**：TCP 发送器
- **segment**：报文段
- **payload**：有效载荷
- **sequence number / seqno**：序列号
- **absolute sequence number**：绝对序列号
- **stream index**：流索引
- **acknowledgment number / ackno**：确认号
- **window size**：窗口大小
- **Initial Sequence Number / ISN**：初始序列号
- **SYN**：同步标志，表示流的开始
- **FIN**：结束标志，表示流的结束
- **RST**：复位标志，表示连接或流出现错误，需要中止
- **checkpoint**：检查点，用于在序列号回绕时确定最接近的绝对序列号

## 参考来源

- 原始作业说明：[CS144 Lab Checkpoint 2: the TCP receiver](https://cs144.github.io/assignments/check2.pdf)
- 本译文基于上述 PDF 的内容整理，代码、命令和 C++ 接口保留原文形式。