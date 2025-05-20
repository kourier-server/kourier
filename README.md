> “The most amazing achievement of the computer software industry is its continuing cancellation of the steady and staggering gains made by the computer hardware industry.”
>
> — <cite>Henry Petroski</cite>

# Meet Kourier, The Fastest Server Ever

[Kourier](https://kourier.io) is the fastest, lightest, and most HTTP syntax-compliant server.

Kourier is truly a wonder of software engineering. It is open source, written in modern C++, 
and leaves all the alternatives far behind. Below, I summarize the benchmark results that 
I show on my [blog](https://blog.kourier.io). All benchmarks are container-based, allowing 
anyone to validate the results shown here.



## Benchmarks

On my [blog](https://blog.kourier.io), you can see posts where I compare Kourier with other popular 
frameworks and show that Kourier beats all of them by a large margin regarding performance, memory 
consumption, and compliance with HTTP syntax. All benchmarks are container-based and can be
[found](Src/Tests/Resources/Benchmarks/README.md) in this repository.

I summarize the benchmark results shown in my posts below.

### Performance (512 connections - pipelined requests)
This benchmark is similar to the plaintext test used on TechEmpower benchmarks. Tests were run 
on an AMD Ryzen 5 1600 processor with 32GB of memory, and all frameworks used six threads to 
process HTTP requests.

| Framework     | RPS       |
| ------------- | --------: |
| Kourier       | 12138068  |
| Rust (Hyper)  | 4382914   |
| Go (net/http) | 231080    |


### Memory Consumption (500K connections)
Memory consumption benchmark tests were run on an AMD Ryzen 5 1600 processor with 32GB of memory, 
and all frameworks used six threads to process HTTP requests.

| Framework     | Max Used Memory (kB) |
| ------------- | --------: |
| Kourier       | 1261344   |
| Rust (Hyper)  | 5974808   |
| Go (net/http) | 9801832   |



### HTTP Compliance
HTTP compliance test results were obtained using a Docker image 
[belonging](Src/Tests/Resources/Benchmarks/HttpCompliance/README.md) to this repository.

| Kourier  | Results |
| -------------: | :-------- |
| 175 | requests with invalid URL absolute path detected.
| 0 | requests with invalid URL absolute path undetected.
| 175 | requests with invalid URL query detected.
| 0 | requests with invalid URL query undetected.
| 178 | requests with invalid header name detected.
| 0 | requests with invalid header name undetected.
| 30 | requests with invalid header value detected.
| 0 | requests with invalid header value undetected.
| 61692 | requests with invalid pct-encoded hex digits in URL absolute path detected.
| 0 | requests with invalid pct-encoded hex digits URL absolute path undetected.
| 61692 | requests with invalid pct-encoded hex digits URL query detected.
| 0 | requests with invalid pct-encoded hex digits URL query undetected.


| Rust (Hyper)     | Results |
| -------------: | :-------- |
| 165 | requests with invalid URL absolute path detected.
| 10 | requests with invalid URL absolute path undetected.
| 165 | requests with invalid URL query detected.
| 10 | requests with invalid URL query undetected.
| 178 | requests with invalid header name detected.
| 0 | requests with invalid header name undetected.
| 30 | requests with invalid header value detected.
|  0 | requests with invalid header value undetected.
| 55332 | requests with invalid pct-encoded hex digits in URL absolute path detected.
| 6360 | requests with invalid pct-encoded hex digits URL absolute path undetected.
| 55332 | requests with invalid pct-encoded hex digits URL query detected.
| 6360 | requests with invalid pct-encoded hex digits URL query undetected.


| Go (net/http)     | Results |
| -------------: | :-------- |
| 35 | requests with invalid URL absolute path detected.
| 140 | requests with invalid URL absolute path undetected.
| 34 | requests with invalid URL query detected.
| 141 | requests with invalid URL query undetected.
| 177 | requests with invalid header name detected.
|  1 | requests with invalid header name undetected.
| 30 | requests with invalid header value detected.
|  0 | requests with invalid header value undetected.
| 61692 | requests with invalid pct-encoded hex digits in URL absolute path detected.
|  0 | requests with invalid pct-encoded hex digits URL absolute path undetected.
| 16252 | requests with invalid pct-encoded hex digits URL query detected.
| 45440 | requests with invalid pct-encoded hex digits URL query undetected.


## Why No One Did This Before?
Developing a server that delivers unprecedented performance is a complex and challenging task. Many different aspects must be addressed to achieve stellar performance. Let's explore some key features that help Kourier to provide its never-before-seen performance.



## State Of The Art HTTP Parser
The HTTP syntax rules are simple to enforce when the parser is a state machine that works byte-by-byte. To write a faster parser, we have to use SIMD instructions. However, enforcing HTTP syntax rules becomes considerably more complex with SIMD instructions. That's why many parsers deliberately loosen HTTP syntax rules to employ them, as I show on my [blog](https://blog.kourier.io).

Kourier uses SIMD instructions extensively on its parser while maintaining strict adherence to HTTP syntax rules.

Kourier's parser is a performance powerhouse, capable of processing 12.1 million unencrypted HTTP requests per second on an AMD Ryzen 5 1600, an 8-year-old mid-range processor, using only half of its cores (wrk uses the other half). It sets a new standard for HTTP parsing and leaves the highest-performing enterprise network appliances in the dust.

Kourier is not just the fastest server on Earth; it's also open source, ensuring that all claims about its performance are verifiable.



## Unbeatable TLS Performance
Kourier uses OpenSSL for TLS encryption. Although OpenSSL is battle-tested, it is challenging to integrate it appropriately. Almost all users of OpenSSL do the naive approach of employing file descriptor-based BIOs, which notoriously consume vast amounts of memory, besides being slow.

Kourier's implementation excels at TLS performance because it provides custom memory BIOs to OpenSSL to restrict it to TLS computations while keeping all other responsibilities under Kourier's control.



## Cutting-Edge Signal-Slot Implementation
The awesome Qt Framework popularized signals and slots, one of Qt's main features. Signals and slots promote loosely coupled designs that propagate events through signals. Signals and slots unify frontend and backend programming into a single paradigm, as events can represent either user interaction or incoming/outgoing network data.

Kourier implements a modern signals and slots mechanism built upon C++'s powerful template metaprogramming capabilities. It is an order of magnitude faster than Qt and consumes 4x less memory.



## Lightweight Timers
Writing a reliable server requires timers. Malicious users intentionally send data slowly on multiple connections to attack a server. Timers help to prevent that abuse from happening. However, userspace timer implementations are generally not designed with the requirements of high-performance servers as a use case.

For example, every time a server starts to process a request, it resets a request timer, and whenever the server responds to a request, it starts an idle timer. Typical timer implementations are not optimized for this use case of multiple resets without timeouts. That's why some frameworks use deadlines instead of real timeout-based timers to achieve better benchmark results.

However, deadlines can only prevent slow senders. They are useless against rogue senders that keep the connection open indefinitely after sending only part of a request.

Kourier provides a timer implementation that allows timers to be reset millions of times per second without incurring system calls or memory allocations. Kourier's implementation provides timeout-based timers and can be viewed as an ultra-lightweight version of Qt's coarse timers.



## Correct Use Of Epoll
Kourier also provides one of the best implementations for using epoll, a high-performance Linux IO event notification interface, to monitor file descriptors.

If you look at epoll's source code, you will learn that it only keeps O(1) computational complexity if used in edge-triggered mode. In level-triggered mode, file descriptors added to epoll's ready list never leave it.

Implementing a server with unprecedented performance requires sharp attention to detail. How the system interacts with the low-level IO readiness model provided by the Kernel is crucial for its performance and reliability.

Kourier integrates epoll into Qt's event system and implements socket classes that use the signals and slots mechanism to abstract epoll-based IO operations while providing all the niceties of having a Qt event loop running on worker threads.

Kourier exports TcpSocket and TlsSocket classes, which you can use instead of Qt's socket classes. Both are much faster and more lightweight than their Qt-based counterparts.



## Behaves By The Book
I developed Kourier with strict and demanding requirements, where all possible behaviours are 
comprehensively verified in specifications written in the Gherkin style. To this end, I created 
Spectator, a test framework that I also open-sourced with Kourier and is [included](Src/Tests/Spectator/README.md) in Kourier's
repository. You can check all files 
ending in spec.cpp in the Kourier repository to see how meticulously tested Kourier is. It is 
funny to compare the abysmal difference in testing rigor between Kourier and other frameworks.



## AGPL Only? My Business Is Not Compatible With It!
You can [contact me](mailto:glauco@kourier.io) if your Business wants to use Kourier under an alternative license.

It is not a problem if your network appliances run on a BSD-derived OS. The IO readiness models 
provided by epoll and kqueue are similar, and it is not too much effort to make Kourier work with 
both.


## About Me
I have zero social media usage. If your Business wants to license Kourier, [here](https://kourier.io/about.html) 
you can learn more about me.


## Documentation
Kourier's documentation is available at https://docs.kourier.io.

## Contributing

I do not accept contributions of any kind. Please do not create pull requests or issues for this repository.
