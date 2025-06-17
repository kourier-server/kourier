> “The most amazing achievement of the computer software industry is its continuing cancellation of the steady and staggering gains made by the computer hardware industry.”
>
> — <cite>Henry Petroski</cite> 

<br />

# Meet Kourier, The Performance King

Kourier is open-source, written in modern C++, and is 100% HTTP syntax-compliant. Kourier outperforms significantly less-compliant servers in terms of performance and memory consumption.

Below, you can see the results of reproducible, container-based benchmarks shown in detail at https://blog.kourier.io and created using open-source Docker images.

With Kourier, you can implement backend services at a fraction of your current infrastructure costs. Besides its ultra-fast and fully compliant HTTP server and parser, Kourier also exports many C++ classes it uses for TCP communication, TLS encryption, lightweight timers, and signal-slot communications that you can use as building blocks to implement high-performance networked systems. At https://docs.kourier.io, you can find detailed documentation about the Kourier server and all C++ classes it exports.

If high-performance network communication is part of your core business, Kourier can take it to the next level.

<br />

## Benchmarks

It is common to see high RPS for servers on benchmarks, but how many HTTP-compliance benchmarks have you seen on the Internet? Below, you can see how Kourier compares against well-known alternatives (you can click on the links below to go to the respective post detailing the benchmark from which the compliance score was obtained):<br /><br />

| Server      | Lan     | HTTP Compliance Score |
| -------     | :-----: | :------: |
| [Kourier](https://blog.kourier.io/posts/kourier-vs-lithium-the-fully-compliant-server-is-also-the-fastest)     | C++     | 100.0% |
| [Hyper](https://blog.kourier.io/posts/c-kourier-vs-rust-hyper-vs-go-http-http-compliance-benchmark)       | Rust    | 89.7% |
| [net/http](https://blog.kourier.io/posts/c-kourier-vs-rust-hyper-vs-go-http-http-compliance-benchmark)    | Go      | 63.1% |
| [QHttpServer](https://blog.kourier.io/posts/kourier-vs-qhttpserver) | C++     | 1.2% |
| [Lithium](https://blog.kourier.io/posts/kourier-vs-lithium-the-fully-compliant-server-is-also-the-fastest)     | C++     | 0.0% |

<br />

Many servers/frameworks deliberately take a lenient approach regarding the HTTP syntax rules to make it easier to implement the HTTP parser. Although it would be expected for a fully-compliant server to be slower than the less-compliant ones, Kourier is truly a wonder of software engineering and outperforms the less-compliant servers, most by a large margin, as can be seen below (you can click on the links to go to the respective benchmark from which the result was obtained):<br /><br />

| Framework   | CPU       | Lan     | RPS       |
| :---------- | :-------: | :-----: | :-------: |
| [Kourier](https://blog.kourier.io/posts/kourier-vs-lithium-the-fully-compliant-server-is-also-the-fastest)     | AMD EPYC  |   C++   | 33.0M     |
| [Lithium](https://blog.kourier.io/posts/kourier-vs-lithium-the-fully-compliant-server-is-also-the-fastest)     | AMD EPYC  |   C++   | 32.5M     |
| [Kourier](https://blog.kourier.io/posts/c-kourier-vs-rust-hyper-vs-go-http-performance-benchmark)     | AMD Ryzen |   C++   | 12.1M     |
| [Hyper](https://blog.kourier.io/posts/c-kourier-vs-rust-hyper-vs-go-http-performance-benchmark)       | AMD Ryzen |   Rust  | 4.3M      |
| [net/http](https://blog.kourier.io/posts/c-kourier-vs-rust-hyper-vs-go-http-performance-benchmark)    | AMD Ryzen |   Go    | 231K      |  

<br />

An ultra-fast server enables a significant reduction of the computing resources required to implement backend services, but this advantage gets lost if the server requires too much memory. Below, you can see how Kourier compares against popular alternatives regarding memory consumption (you can click on the links to go to the posts detailing the benchmarks from which the results were taken):<br /><br />

| Framework     | Lan | Max Used Memory (kB) |
| ------------- | :-----: | :--------: |
| [Kourier](https://blog.kourier.io/posts/c-kourier-vs-rust-hyper-vs-go-http-memory-consumption-benchmark)  | C++     | 1.2 GB   |
| [Hyper](https://blog.kourier.io/posts/c-kourier-vs-rust-hyper-vs-go-http-memory-consumption-benchmark)  | Rust | 5.9 GB   |
| [net/http](https://blog.kourier.io/posts/c-kourier-vs-rust-hyper-vs-go-http-memory-consumption-benchmark) | Go |  9.8 GB   |

<br />

## How Can A Fully-Compliant Server Be So Fast?
Developing a server that delivers unprecedented performance is a complex and challenging task. Many different aspects must be addressed to achieve stellar performance. Let's explore some key features that help Kourier to provide its unbeatable performance:

<br />

## State Of The Art HTTP Parser
The HTTP syntax rules are simple to enforce when the parser is a state machine that works byte-by-byte. To write a faster parser, we have to use SIMD instructions. However, enforcing HTTP syntax rules becomes considerably more complex with SIMD instructions. That's why many parsers deliberately loosen HTTP syntax rules to employ them.

Kourier uses SIMD instructions extensively on its parser while maintaining strict adherence to HTTP syntax rules.

Kourier's parser is a performance powerhouse, capable of processing 12.1 million HTTP requests per second on an AMD Ryzen 5 1600, an 8-year-old mid-range processor, using only half of its cores (wrk uses the other half). It sets a new standard for HTTP parsing, leaving the top performers far behind. On a modern server-grade CPU, Kourier beats Lithium, the fastest C++ server on TechEmpower benchmarks and whose parser only scans for CRLFs without enforcing any HTTP syntax rule.

<br />

## Unbeatable TLS Performance
Kourier uses OpenSSL for TLS encryption. Although OpenSSL is battle-tested, integrating it properly can be a challenging task. Almost all users of OpenSSL employ file descriptor-based BIOs, which notoriously consume vast amounts of memory beyond being slow.

Kourier's implementation excels at TLS performance because it provides custom memory BIOs to OpenSSL, restricting it to TLS computations while keeping all other responsibilities under Kourier's control.

<br />

## Cutting-Edge Signal-Slot Implementation
Signals and slots promote loosely coupled designs that propagate events through signals. Signals and slots unify frontend and backend programming into a single paradigm, as events can represent either user interaction or asynchronous I/O.

Kourier implements a modern signals and slots mechanism built upon C++'s powerful template metaprogramming capabilities. It is an order of magnitude faster than Qt's signals and slots and consumes four times less memory.

<br />

## Lightweight Timers
Writing a reliable server requires timers. Malicious users intentionally send data slowly on multiple connections to attack a server. Timers help to prevent that abuse from happening. However, userspace timer implementations are generally not designed with the requirements of high-performance servers as a use case.

For example, every time a server starts to process a request, it resets a request timer, and whenever the server responds to a request, it starts an idle timer. Typical timer implementations are not optimized for this use case, which involves multiple resets without timeouts.

Although deadlines prevent slow senders, they are useless against rogue senders that keep the connection open indefinitely after sending only part of a request.

Kourier provides a timer implementation that allows timers to be reset millions of times per second without incurring system calls or memory allocations. Kourier's implementation provides timeout-based timers and can be viewed as an ultra-lightweight version of Qt's coarse timers.

<br />

## Correct Use Of Epoll
Kourier also provides one of the best implementations for using epoll, a high-performance Linux IO event notification interface, to monitor file descriptors.

On the Internet, the debate over level-triggered vs. edge-triggered is often limited to the difference in how the state is reset. However, the implications are more far-reaching than many developers think. As always, the truth lies in the [source code](https://github.com/torvalds/linux/blob/90b83efa6701656e02c86e7df2cb1765ea602d07/fs/eventpoll.c#L1923). With level-triggering, epoll keeps all file descriptors that have become ready on its ready list even after their state is reset.

Implementing a server with unprecedented performance requires sharp attention to detail. How the system interacts with the low-level IO readiness model provided by the Kernel is crucial for its performance and reliability.

Kourier integrates epoll into Qt's event system and implements socket classes that utilize the signals and slots mechanism to abstract epoll-based I/O operations while providing all the benefits of having a Qt event loop running on worker threads.

Kourier exports TcpSocket and TlsSocket classes, which you can use instead of Qt's socket classes. Both are significantly faster and lighter than their Qt-based counterparts.

<br />

## Behaves By The Book
Kourier is developed with strict and demanding requirements, where all possible behaviors are comprehensively verified in specifications written in the Gherkin style. Test files use Spectator, a test framework that is part of Kourier and is [included](Src/Tests/Spectator/README.md) in Kourier's repository. You can check all files ending in *.spec.cpp* in the Kourier repository to see how meticulously tested Kourier is. There is a stark difference in testing rigor between Kourier and other publicly available servers.

<br />

## AGPL Only? My Business Is Not Compatible With It!

Kourier is open-source. Thus, anyone can study, build, validate, and benchmark Kourier.

You can contact me at glauco@kourier.io if your Business is incompatible with the AGPL and you want to license Kourier under alternative terms.

<br />

## About Me
I have zero social media usage. If your Business wants to license Kourier, [here](https://kourier.io/about.html) 
you can learn more about me.

<br />

## Documentation
Kourier's documentation is available at https://docs.kourier.io.

<br />

## Contributing

I do not accept contributions of any kind. Please do not create pull requests for this repository.
