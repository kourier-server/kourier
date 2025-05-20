FROM golang:latest

ADD ./ /net-http
WORKDIR /net-http

RUN GOAMD64=v3 go build -ldflags="-s -w" -o app /net-http/src/net-http.go

ENTRYPOINT ["/net-http/app"]
