# Multi-stage C++ development environment with CMake and dbus-c++
# Image name: smsc-dev

FROM ubuntu:24.04 AS builder

# Set non-interactive frontend
ENV DEBIAN_FRONTEND=noninteractive

# Install base development tools
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    pkg-config \
    wget \
    curl \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Install dbus and dbus-c++ dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    libdbus-1-dev \
    libdbus-c++-dev \
    dbus \
    && rm -rf /var/lib/apt/lists/*

# Development runtime image
FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

# Install runtime and development dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    pkg-config \
    gdb \
    valgrind \
    libdbus-1-3 \
    libdbus-c++ \
    libdbus-1-dev \
    libdbus-c++-dev \
    dbus \
    && rm -rf /var/lib/apt/lists/*

# Create workspace directory
WORKDIR /workspace

# Copy source code (optional - can be mounted as volume)
# COPY . /workspace/

# Set default command to bash
CMD ["/bin/bash"]

# Final development image
FROM runtime AS smsc-dev

LABEL maintainer="Telecom Team"
LABEL description="C++ Development Environment with CMake and dbus-c++"
LABEL version="1.0"

# Add useful build aliases
RUN echo 'alias build="mkdir -p build && cd build && cmake .. && make"' >> /root/.bashrc && \
    echo 'alias rebuild="cd build && make clean && make"' >> /root/.bashrc && \
    echo 'alias cmake-clean="rm -rf build"' >> /root/.bashrc

# Expose common ports if needed
# EXPOSE 5000

WORKDIR /workspace
CMD ["/bin/bash"]
