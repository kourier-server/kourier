OpenSSL Commands
========================================================

The certificates/private keys contained here were created with the following commands:

openssl genpkey -algorithm EC -pkeyopt group:P-256 -out ca.key

openssl req -x509 -new -key ca.key -sha512 -days 3600 -out ca.crt -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=ecdsa-ca.root-certificate" -extensions v3_ca -addext "basicConstraints=critical,CA:TRUE,pathlen:10"

openssl genpkey -algorithm EC -pkeyopt group:P-256 -out cert.key

openssl req -new -key cert.key -out cert.csr -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=ecdsa-certificate"

openssl x509 -req -extfile <(printf "subjectAltName=DNS:*.localhost,DNS:localhost,IP:127.0.0.1") -in cert.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out cert.crt -days 3000 -sha512
