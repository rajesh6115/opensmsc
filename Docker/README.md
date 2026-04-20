# SMC Development Docker Environment

This Docker setup provides a complete C++ development environment with CMake and dbus-c++ support for building SMPP Server components.

## Quick Start

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

## Building in Docker

### Full Build (SMPPServer + SmppClientHandler)

```bash
cd /workspace
build-all
```

This will compile both:
- `simple_smpp_server` - Main SMPP server
- `smpp_client_handler` - Per-client handler

### Build Individual Components

```bash
# Build only SMPPServer
build-server

# Build only SmppClientHandler
build-handler

# Full rebuild (clean + compile)
rebuild
```

## Development Workflow

Once inside the container, you can use these built-in aliases:

- `build` - Create build directory and compile all projects with CMake
- `build-all` - Full build of all targets (SMPPServer + SmppClientHandler)
- `build-server` - Build only SMPPServer
- `build-handler` - Build only SmppClientHandler
- `rebuild` - Clean rebuild of all projects
- `cmake-clean` - Remove build artifacts

### Example Workflow:

```bash
# Inside container
cd /workspace

# First time build
build-all

# Later, just rebuild
rebuild

# Work on specific component
build-handler

# Clean everything
cmake-clean
build-all
```

## Binary Locations

After building, binaries are located at:

```bash
# SMPP Server binary
/workspace/build/SMPPServer/simple_smpp_server

# SMPP Client Handler binary
/workspace/build/SmppClientHandler/smpp_client_handler

# List all built binaries
ls -la /workspace/build/SMPPServer/ /workspace/build/SmppClientHandler/
```

## Testing Built Binaries

```bash
# Check if binaries were created
file /workspace/build/SMPPServer/simple_smpp_server
file /workspace/build/SmppClientHandler/smpp_client_handler

# Run with help/version flags
/workspace/build/SMPPServer/simple_smpp_server --help
/workspace/build/SmppClientHandler/smpp_client_handler --help
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
