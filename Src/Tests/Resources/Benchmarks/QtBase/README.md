Building Docker Container:
================================================================================
sudo docker build --no-cache --build-arg THREAD_COUNT=12 --build-arg OPENSSL_VERSION=3.5.0 -t kourier-bench:qt-base -f qt-base.dockerfile .
