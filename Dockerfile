FROM gcc:latest as builder
COPY . /usr/src/myapp
WORKDIR /usr/src/myapp
RUN gcc -o greenis -static main.c

FROM alpine:latest as runtime
COPY --from=builder /usr/src/myapp/greenis /greenis
ENTRYPOINT ["./greenis"]