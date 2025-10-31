FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    libprotobuf-dev \
    protobuf-compiler \
    protobuf-compiler-grpc \
    libgrpc-dev \
    libgrpc++-dev && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN cmake -S . -B build && \
    cmake --build build --target messaging_service -- -j"$(nproc || echo 2)"

EXPOSE 7002

CMD ["./build/messaging_service"]
