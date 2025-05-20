Building Docker Container:
================================================================================
docker build --no-cache -f hyper.dockerfile -t kourier-bench:rust-hyper .

Running Docker Container:
================================================================================
docker run --rm -d --network host kourier-bench:rust-hyper
