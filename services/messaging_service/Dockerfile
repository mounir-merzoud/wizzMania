FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# Base toolchain and utilities
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    curl \
    zip \
    unzip \
    pkg-config \
    ca-certificates && \
    rm -rf /var/lib/apt/lists/*

# Install vcpkg for CMake package resolution (grpc, protobuf, libpqxx)
WORKDIR /opt
RUN git clone https://github.com/microsoft/vcpkg.git && \
    /opt/vcpkg/bootstrap-vcpkg.sh -disableMetrics

# Pre-install required libraries
RUN /opt/vcpkg/vcpkg install grpc protobuf libpqxx

WORKDIR /app

COPY . .

# Configure and build using vcpkg toolchain
RUN cmake -S . -B build \
      -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake \
      -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --target messaging_service -- -j"$(nproc || echo 2)"

EXPOSE 7002

CMD ["./build/bin/messaging_service"]
