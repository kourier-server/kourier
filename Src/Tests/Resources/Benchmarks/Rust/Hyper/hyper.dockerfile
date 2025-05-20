FROM rust:latest

ADD ./ /hyper
WORKDIR /hyper

RUN cargo clean
RUN RUSTFLAGS="-C target-cpu=native -C target-feature=+avx2" cargo build --release

ENTRYPOINT ["/hyper/target/release/hyper-bench"]
