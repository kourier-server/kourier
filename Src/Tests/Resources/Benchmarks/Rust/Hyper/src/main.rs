#![deny(warnings)]

use std::net::SocketAddr;
use bytes::Bytes;
use http_body_util::{combinators::BoxBody, BodyExt, Full};
use hyper::server::conn::http1;
use hyper::{Request, Response};
use tokio::net::TcpListener;
use hyper_util::rt::tokio::TokioIo;
use tokio::runtime;
use std::time::Duration;
use hyper_util::service::TowerToHyperService;
use tower::ServiceBuilder;


async fn hello(
    _req: Request<hyper::body::Incoming>,
) -> Result<Response<BoxBody<Bytes, hyper::Error>>, hyper::Error> {
            let body = "Hello World!";
            let response = Response::builder()
                .header("Server", "hyper")
                .header("Content-Length", body.len())
                .body(full(body))
                .unwrap();
            Ok(response)
     
}


fn full<T: Into<Bytes>>(chunk: T) -> BoxBody<Bytes, hyper::Error> {
    Full::new(chunk.into())
        .map_err(|never| match never {})
        .boxed()
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    let addr = SocketAddr::from(([127, 0, 0, 1], 8080));

    let listener = TcpListener::bind(addr).await?;
    let rt = runtime::Builder::new_multi_thread()
        .enable_all()
        .worker_threads(6)
        .build()
        .unwrap();
    println!("Listening on http://{}", addr);
    loop {
        let (stream, _) = listener.accept().await?;
        let io = TokioIo::new(stream);

        rt.spawn(async move {
            let tower_service = ServiceBuilder::new()
                .timeout(Duration::from_secs(60))
                .service_fn(hello);
            let hyper_service = TowerToHyperService::new(tower_service);
            if let Err(err) = http1::Builder::new()
                .pipeline_flush(true)
                .serve_connection(io, hyper_service)
                .await
            {
                println!("Error serving connection: {:?}", err);
            }
        });
    }
}
