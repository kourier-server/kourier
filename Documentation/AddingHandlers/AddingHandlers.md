# Adding Handlers {#AddingHandlers}

[HttpServer](@ref Kourier::HttpServer) provides the [addRoute](@ref Kourier::HttpServer::addRoute) method that you can use to map handlers to HTTP methods and paths.

Whenever [HttpServer](@ref Kourier::HttpServer) parses a request with a mapped method, it calls the handler mapped to the most specific path to handle it. For example, if you map \a /food and \a /food/pasta paths to [GET](@ref Kourier::HttpRequest::Method::GET) requests, a [GET](@ref Kourier::HttpRequest::Method::GET) request targeting \a /food/fish/salmon will be handled by the handler mapped to \a /food, and a [GET](@ref Kourier::HttpRequest::Method::GET) request targeting \a /food/pasta/spaghetti will be handled by the handler mapped to \a /food/pasta.

[HttpServer](@ref Kourier::HttpServer) accepts as handlers pointers to functions having the following signature:

```cpp
void handlerFcn(const Kourier::HttpRequest &request, Kourier::HttpBroker &broker)
```

If you add a route to an already mapped path and method, [HttpServer](@ref Kourier::HttpServer) replaces the previous handler with the new one. The routes you add to [HttpServer](@ref Kourier::HttpServer) are used after you start the server. If you add a route to an already running server, it will only be available the next time you start it.

If [HttpServer](@ref Kourier::HttpServer) fails to find a mapped handler for a given request, it responds with a <em>404 Not Found</em> status code and closes the connection. If the mapped handler throws an unhandled exception, [HttpServer](@ref Kourier::HttpServer) responds with a <em>500 Internal Server Error</em> status code and closes the connection.

[HttpServer](@ref Kourier::HttpServer) provides a flexible request-handling mechanism, but you must know how it works. First, [HttpServer](@ref Kourier::HttpServer) calls the mapped handler as soon as when it parses the request header block. If the request has a body that is not chunked, [HttpServer](@ref Kourier::HttpServer) processes all body data available when it parses the header block before calling the mapped handler. You can call [isComplete](@ref Kourier::HttpRequest::isComplete) on the [HttpRequest](@ref Kourier::HttpRequest) instance that [HttpServer](@ref Kourier::HttpServer) passes to the handler to know if the request is complete. Thus, [HttpServer](@ref Kourier::HttpServer) must allow you to handle requests outside the mapped handler. To this end, [HttpBroker](@ref Kourier::HttpBroker) provides the [setQObject](@ref Kourier::HttpBroker::setQObject) method that you can use to process the request after the handler function returns.

It is uncommon, but you can respond to the request before receiving it. In this case, [HttpServer](@ref Kourier::HttpServer) reads the entire request message body after sending the response, per section 9.3 of RFC 9112. [HttpServer](@ref Kourier::HttpServer) responds automatically with the <em>100 Continue</em> status code when the request contains an \a Expect header field with a <em>100-continue</em> expectation.

[HttpServer](@ref Kourier::HttpServer) closes the connection if the mapped handler neither writes a complete response nor calls [setQObject](@ref Kourier::HttpBroker::setQObject) to set an object to the broker.
