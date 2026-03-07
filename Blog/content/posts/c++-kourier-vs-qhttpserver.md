+++
author = "Glauco Pacheco"
title = "Kourier vs QHttpServer"
date = "2025-05-01"
authors = ["Glauco Pacheco"]
description = "Kourier leaves QHttpServer in the dust."
tags = [
"kourier",
    "qhttpserver",
    "c++",
    "http",
    "server"
]
+++


<!--more-->

In this post, I compare Kourier with QHttpServer regarding performance and compliance with HTTP syntax.

To make things more interesting, I use TLS-encrypted connections for testing performance. As QHttpServer does not use the concept of workers, I limit HTTP request processing to a single thread for both servers.

Both servers use TLS 1.2 with the ECDHE-ECDSA-AES128-GCM-SHA256 cipher.

All Docker images used in this benchmark can be easily built using the build [script](https://github.com/kourier-server/kourier/blob/main/Src/Tests/Resources/Benchmarks/build_all.sh) contained in Kourier's repository as follows:

```bash
git clone https://github.com/kourier-server/kourier.git Kourier
sudo ./Kourier/Src/Tests/Resources/Benchmarks/build_all.sh
```

The servers are started with the following commands:

```bash
# Kourier. The server listens on port 3275
# and uses a single thread to process incoming requests.
sudo docker run --rm -d --network host kourier-bench:kourier -a 127.0.0.1 -p 3275 --worker-count=1 --enable-tls --request-timeout=20 --idle-timeout=60
# QHttpServer. The server listens on port 5275
# and uses a single thread to process incoming requests.
sudo docker run --rm --network host -d kourier-bench:qhttpserver -a 127.0.0.1 -p 5275 --enable-tls
```

The command shown below is used to test the performance:

```bash
# PORT is 3275 for Kourier, or 5275 for QHttpServer.
sudo docker run --rm --network host -it kourier-bench:wrk -c 512 -d 15 -t 6 --latency https://localhost:PORT/hello -s /wrk/pipeline.lua -- 256
```

I use the following command to test compliance with HTTP syntax (for this test to work, the servers must be started without the *\-\-enable-tls* command-line option):

```bash
# PORT is 3275 for Kourier, or 5275 for QHttpServer.
# Servers should be started without the --enable-tls command line option.
sudo docker run --rm --network host -it kourier-bench:http-compliance -a 127.0.0.1 -p PORT
```

The test certificates and private keys used for TLS encryption are located in *Src/Tests/Resources/Benchmarks/certs* within the Kourier repository and are copied to the Docker images.<br><br>

# Results

| Server      |   RPS   | HTTP Compliance Score |
| -------     | ------: | :------: |
| Kourier     | 2238158 | 100.0% |
| QHttpServer |   69704 | 1.2% |

```bash
# Testing Kourier server at 127.0.0.1:3275
glauco@ldh:~$ sudo docker run --rm --network host -it kourier-bench:wrk -c 512 -d 15 -t 6 --latency https://localhost:3275/hello -s /wrk/pipeline.lua -- 256
Running 15s test @ https://localhost:3275/hello
  6 threads and 512 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    26.61ms   17.53ms 148.54ms   38.63%
    Req/Sec   383.20k    82.12k  647.68k    67.58%
  Latency Distribution
     50%   54.71ms
     75%    0.00us
     90%    0.00us
     99%    0.00us
  33782628 requests in 15.09s, 3.30GB read
Requests/sec: 2238158.06
Transfer/sec:    224.12MB
```

```bash
# Testing QHttpServer server at 127.0.0.1:5275
glauco@ldh:~$ sudo docker run --rm --network host -it kourier-bench:wrk -c 512 -d 15 -t 6 --latency https://localhost:5275/hello -s /wrk/pipeline.lua -- 256
Running 15s test @ https://localhost:5275/hello
  6 threads and 512 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   856.07ms  502.41ms   1.93s    56.34%
    Req/Sec    11.92k     3.98k   50.26k    89.16%
  Latency Distribution
     50%  872.91ms
     75%    1.35s
     90%    1.66s
     99%    0.00us
  1052160 requests in 15.09s, 51.17MB read
Requests/sec:  69704.77
Transfer/sec:      3.39MB
```

```bash
# Testing Kourier server compliance at 127.0.0.1:3275
glauco@ldh:~$ sudo docker run --rm --network host -it kourier-bench:http-compliance -a 127.0.0.1 -p 3275
Testing HTTP compliance...
Finished testing HTTP compliance.
HTTP compliance score (% of passed tests): 100.0%
       175 requests with invalid URL absolute path detected.
         0 requests with invalid URL absolute path undetected.
       175 requests with invalid URL query detected.
         0 requests with invalid URL query undetected.
       178 requests with invalid header name detected.
         0 requests with invalid header name undetected.
        30 requests with invalid header value detected.
         0 requests with invalid header value undetected.
     61692 requests with invalid pct-encoded hex digits in URL absolute path detected.
         0 requests with invalid pct-encoded hex digits in URL absolute path undetected.
     61692 requests with invalid pct-encoded hex digits in URL query detected.
         0 requests with invalid pct-encoded hex digits in URL query undetected.
```

```bash
# Testing QHttpServer server compliance at 127.0.0.1:5275
glauco@ldh:~$ sudo docker run --rm --network host -it kourier-bench:http-compliance -a 127.0.0.1 -p 5275
Testing HTTP compliance...
Finished testing HTTP compliance.
HTTP compliance score (% of passed tests):   1.2%
         1 requests with invalid URL absolute path detected.
       174 requests with invalid URL absolute path undetected.
         1 requests with invalid URL query detected.
       174 requests with invalid URL query undetected.
         0 requests with invalid header name detected.
       178 requests with invalid header name undetected.
         0 requests with invalid header value detected.
        30 requests with invalid header value undetected.
       765 requests with invalid pct-encoded hex digits in URL absolute path detected.
     60927 requests with invalid pct-encoded hex digits in URL absolute path undetected.
       765 requests with invalid pct-encoded hex digits in URL query detected.
     60927 requests with invalid pct-encoded hex digits in URL query undetected.
```

<br>

# Conclusion

Kourier is the next level of network-based communication. It outperforms all other tested servers in terms of performance, compliance, and memory consumption, leaving them far behind.

Creating the fastest server ever requires much more than a stellar HTTP parser. Kourier exports many of the C++ classes it uses internally for TCP-based communication, TLS encryption, signal-slot connections, and lightweight timers.

I developed Kourier with strict and demanding requirements, where all possible behaviors are comprehensively verified in specifications written in the Gherkin style. To this end, I created Spectator, a test framework that I also open-sourced with Kourier. You can check all files ending in spec.cpp in the Kourier repository to see how meticulously tested Kourier is. There is a stark difference in testing rigor between Kourier and other frameworks.

Any business that depends on high-performance HTTP-based communication can use Kourier to achieve the next level of performance and compliance.

You can [contact me](mailto:glaucopacheco@gmail.com) if your Business is not compatible with the requirements of the AGPL and you want to license Kourier under alternative terms.
