Building Docker Container:
================================================================================
docker build --no-cache -f net-http.dockerfile -t kourier-bench:go-net-http .

Running Docker Container:
================================================================================
docker run --rm -d --network host kourier-bench:go-net-http
