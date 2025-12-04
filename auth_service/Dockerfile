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

# Install vcpkg for CMake package resolution (grpc, protobuf, libpqxx, cryptopp, jwt-cpp, nlohmann-json)
WORKDIR /opt
RUN git clone https://github.com/microsoft/vcpkg.git && \
    /opt/vcpkg/bootstrap-vcpkg.sh -disableMetrics

# Pre-install required libraries to speed up build (optional, manifest-less)
RUN /opt/vcpkg/vcpkg install grpc protobuf libpqxx cryptopp jwt-cpp nlohmann-json

WORKDIR /app

# Copy source
COPY . .

# Configure and build using vcpkg toolchain
RUN cmake -S . -B build \
      -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake \
      -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --target auth_service -- -j"$(nproc || echo 2)"

EXPOSE 50051

# The binary output path is configured to build/bin in CMakeLists
CMD ["./build/bin/auth_service"]
