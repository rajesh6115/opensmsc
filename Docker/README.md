# SMC Development Docker Environment

This Docker setup provides a complete C++ development environment with CMake and dbus-c++ support.

## Build Instructions

### Build the Docker image:

```bash
docker-compose build smsc-dev
```

### Run the container:

```bash
docker-compose up -d smsc-dev
```

### Enter the container shell:

```bash
docker-compose exec smsc-dev bash
```

## Development Workflow

Once inside the container, you can use these built-in aliases:

- `build` - Create build directory and compile with CMake
- `rebuild` - Clean build and recompile
- `cmake-clean` - Remove build artifacts

### Example:

```bash
# Inside container
cd /workspace
build
```

## Building Individual Projects

Each project has its own CMakeLists.txt. Build them from workspace:

```bash
cd /workspace/SMPPServer
mkdir -p build && cd build
cmake ..
make
```

## D-Bus Integration

The container includes dbus-c++ development libraries. D-Bus system bus can be accessed at `/run/dbus/system_bus_socket` if needed.

## Persistent Volumes

Development uses persistent volumes to speed up builds and preserve state:

- `build-cache` - CMake build directory and artifacts
- `ccache` - Compiler cache for faster incremental builds
- `cmake-cache` - CMake module and package caches
- `pkg-cache` - Package manager caches and local packages
- `source-dev` - Persistent development source directory

Volumes are automatically created and managed by Docker. To clear cache:

```bash
docker volume rm docker_build-cache docker_ccache docker_cmake-cache docker_pkg-cache
```

## Upgrading the Environment

To add new dependencies or tools:

1. Edit the Dockerfile in the `builder` stage to add build tools
2. Edit the `runtime` stage to add runtime dependencies
3. Rebuild: `docker-compose build --no-cache smsc-dev`
4. Recreate container: `docker-compose up -d --force-recreate smsc-dev`

### Example - Adding a library:

```dockerfile
RUN apt-get update && apt-get install -y --no-install-recommends \
    libsome-dev \
    && rm -rf /var/lib/apt/lists/*
```

## Removing the Environment

```bash
# Stop and remove containers
docker-compose down

# Remove image
docker rmi smsc-dev:latest

# Remove volumes (optional)
docker volume rm docker_build-cache docker_ccache
```

## Notes

- Base image: Ubuntu 24.04 LTS (latest stable)
- CMake version: Latest from Ubuntu repositories
- The workspace is mounted at `/workspace` inside the container
- All source files remain on your host machine
