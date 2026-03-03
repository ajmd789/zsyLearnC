ARG BASE_IMAGE=dockerproxy.net/library/debian:13
FROM ${BASE_IMAGE}

ENV DEBIAN_FRONTEND=noninteractive

# Switch APT source to LZU mirror for faster package install in CN.
RUN if [ -f /etc/apt/sources.list ]; then \
      sed -i 's|http://deb.debian.org/debian|http://mirror.lzu.edu.cn/debian|g; s|http://security.debian.org/debian-security|http://mirror.lzu.edu.cn/debian-security|g' /etc/apt/sources.list; \
    fi && \
    if [ -f /etc/apt/sources.list.d/debian.sources ]; then \
      sed -i 's|http://deb.debian.org/debian|http://mirror.lzu.edu.cn/debian|g; s|http://security.debian.org/debian-security|http://mirror.lzu.edu.cn/debian-security|g' /etc/apt/sources.list.d/debian.sources; \
    fi && \
    apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    ca-certificates \
    pkg-config \
    libgrpc++-dev \
    libprotobuf-dev \
    protobuf-compiler \
    protobuf-compiler-grpc \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace