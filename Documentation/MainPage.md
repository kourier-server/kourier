# Getting Started

On this page, you can view the documentation for [Kourier](https://kourier.io), the fastest HTTP server ever.


With [Kourier](https://kourier.io), you can create extremely high-performance REST services quickly and easily. You only have to create an instance of [HttpServer](@ref Kourier::HttpServer), [configure](@ref ConfiguringServer) it, [add handlers](@ref AddingHandlers) to it, and you're ready to go.

Much more than a state-of-the-art HTTP parser is required to create the fastest HTTP server ever. [Kourier](https://kourier.io) relies on a cutting-edge implementation of signal-slot communication, TCP-based communication, and TLS encryption, provided through the [Object](@ref Kourier::Object), [TcpSocket](@ref Kourier::TcpSocket), and [TlsSocket](@ref Kourier::TlsSocket) classes. [TlsSocket](@ref Kourier::TlsSocket) configures TLS encryption according to the [TlsConfiguration](@ref Kourier::TlsConfiguration) you give in [TlsSocket](@ref Kourier::TlsSocket)'s constructor.

