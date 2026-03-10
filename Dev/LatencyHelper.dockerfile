FROM alpine:3.20

RUN apk add --no-cache bash docker-cli iproute2 util-linux kmod

CMD ["sh", "-c", "trap : TERM INT; sleep infinity & wait"]
