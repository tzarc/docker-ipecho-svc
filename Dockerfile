FROM alpine:3.8 as builder
LABEL Maintainer="Nick Brassel <nick@tzarc.org>" \
      Description="Lightweight container for building statically-linked ipecho-svc under Alpine Linux."

RUN mkdir /src \
    && apk update \
    && apk --no-cache add build-base abuild binutils
RUN apk --no-cache add clang clang-analyzer

WORKDIR /
RUN mkdir -pv /src

COPY ./main.c /src/
RUN { scan-build clang -O3 -static -o /src/ipecho-svc /src/main.c || true ; } \
    && strip -s /src/ipecho-svc \
    && { scan-build clang -g -O0 -static -o /src/ipecho-svc.dbg /src/main.c || true ; } \
    && { tar -zcf /src/scan-build.tar.gz /tmp/scan-build-* 2>/dev/null || true ; }

FROM alpine:3.8 as tester
LABEL Maintainer="Nick Brassel <nick@tzarc.org>" \
      Description="Lightweight container for testing statically-linked ipecho-svc under Alpine Linux."

RUN apk update \
    && apk --no-cache add valgrind

COPY --from=builder /src/ipecho-svc.dbg /ipecho-svc.dbg

FROM scratch
LABEL Maintainer="Nick Brassel <nick@tzarc.org>" \
      Description="Lightweight container for echoing IP addresses of the requester."

COPY --from=builder /src/ipecho-svc /ipecho-svc

CMD ["/ipecho-svc"]
