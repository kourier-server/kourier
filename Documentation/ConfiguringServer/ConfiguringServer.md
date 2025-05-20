# Configuring Server {#ConfiguringServer}

[HttpServer](@ref Kourier::HttpServer) provides the [setServerOption](@ref Kourier::HttpServer::setServerOption) and
[serverOption](@ref Kourier::HttpServer::serverOption) methods that you can use to configure and check the server's configuration.

The [ServerOption](@ref Kourier::HttpServer::ServerOption) enum describes the server options that you can configure. Default, minimum, and maximum values for these options are shown in the table below, and you can call [errorMessage](@ref Kourier::HttpServer::errorMessage) to get a description of the error if [setServerOption](@ref Kourier::HttpServer::setServerOption) returns false.

[HttpServer](@ref Kourier::HttpServer) applies the configuration you set when it starts. If you change any configuration for an already running server, [HttpServer](@ref Kourier::HttpServer) will only use it the next time it starts.

You can call [HttpServer::setTlsConfiguration](@ref Kourier::HttpServer::setTlsConfiguration) to configure TLS encryption for [HttpServer](@ref Kourier::HttpServer) according to a given [TlsConfiguration](@ref Kourier::TlsConfiguration).

| Option | Default Value | Min Value | Max Value |
| --- | --- | --- | --- |
| WorkerCount | QThread::idealThreadCount() | 1 (0 sets max value) | QThread::idealThreadCount() |
| TcpServerBacklogSize | 4096 | 1 | std::numeric_limits<int>::max() |
| IdleTimeoutInSecs | 0 | 0 | std::numeric_limits<int>::max() |
| RequestTimeoutInSecs | 0 | 0 | std::numeric_limits<int>::max() |
| MaxHeaderNameSize | 1024 | 1 (0 sets max value) | std::numeric_limits<uint16_t>::max() |
| MaxTrailerNameSize | 1024 | 1 (0 sets max value) | std::numeric_limits<uint16_t>::max() |
| MaxHeaderValueSize | 8192 | 1 (0 sets max value) | std::numeric_limits<uint16_t>::max() |
| MaxTrailerValueSize | 8192 | 1 (0 sets max value) | std::numeric_limits<uint16_t>::max() |
| MaxHeaderLineCount | 64 | 1 (0 sets max value) | 128 |
| MaxTrailerLineCount | 64 | 1 (0 sets max value) | 128 |
| MaxUrlSize | 8192 | 1 (0 sets max value) | std::numeric_limits<int64_t>::max() |
| MaxChunkMetadataSize | 1024 | 1 (0 sets max value) | std::numeric_limits<int64_t>::max() |
| MaxRequestSize | 32MB (2<sup>25</sup>) | 1 (0 sets max value) | std::numeric_limits<int64_t>::max() |
| MaxBodySize | 32MB (2<sup>25</sup>) | 1 (0 sets max value) | std::numeric_limits<int64_t>::max() |
| MaxConnectionCount | std::numeric_limits<int64_t>::max() | 1 (0 sets max value) | std::numeric_limits<int64_t>::max() |



