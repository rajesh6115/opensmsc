# Development Environment Setup Plan

**Version**: 1.0  
**Created**: 2026-04-18  
**Status**: Planning  
**Dev Tools**: VS Code + Remote SSH  
**Compiler**: GCC with C++17  
**Testing**: Google Test (gtest)

---

## 1. Overview

This document defines the development environment needed for the SMSC project. We will use:
- **Docker container** (Ubuntu 24.04) with SSH access for persistent development
- **VS Code Remote SSH** for coding and debugging
- **CMake** for building
- **Google Test** for unit testing

**Benefits**:
- Consistent environment across iterations
- No need to rebuild Docker container during development
- Full IDE experience with VS Code
- Easy to share/replicate environment

---

## 2. Architecture: Host → Docker with SSH

```
Your Host Machine (Windows 11)
    ↓ (ssh command / VS Code Remote SSH)
    ↓
Docker Container (Ubuntu 24.04)
    ├── GCC compiler
    ├── CMake 3.20+
    ├── SSH server (openssh-server)
    ├── Build tools (make, git, etc.)
    ├── Google Test library
    └── Project source code
```

**Workflow**:
1. Start Docker container once (persists)
2. SSH into container for all development
3. Build, test, and debug from within container
4. Container stays running throughout development cycle

---

## 3. Docker Setup

### 3.1 Dockerfile Requirements

Create `Docker/smsc-dev/Dockerfile.dev` (for development) with:

**Base Image**: `ubuntu:24.04`

**Must-Install Packages**:
```dockerfile
# Build tools
- cmake (3.28+, available in Ubuntu 24.04)
- build-essential (g++, gcc, make)
- git
- curl / wget

# Development tools
- openssh-server (for SSH access)
- openssh-client
- sudo
- vim / nano (optional, for in-container editing)

# D-Bus and IPC
- dbus (message bus system)
- dbus-user-session (user session D-Bus)
- libdbus-1-dev (D-Bus development headers)
- sdbus-c++ (or libsdbus-c++-dev if available)
- busctl (D-Bus introspection and testing tool)

# Libraries for SMPP development
- libssl-dev (if TLS needed later)
- pkg-config (for finding libraries)

# Testing
- google-test library (libgtest-dev + cmake)

# Debugging & Monitoring
- net-tools (netstat, ifconfig)
- curl (for HTTP testing, if needed)
- tmux (optional, for multiple sessions)
```

**SSH Configuration**:
- Create non-root user (e.g., `developer`)
- Configure `/etc/ssh/sshd_config` for password-less SSH
- Generate host keys on startup
- Expose port 22 (or custom port like 2222)

**D-Bus Configuration**:
- Install dbus and dbus-user-session
- Mount `/run/dbus` from host for IPC communication
- Enable D-Bus socket availability in container
- Install sdbus-c++ library for C++ D-Bus bindings
- Install busctl for debugging D-Bus services

**Network Port Exposure**:
- Expose SMPP server port 2775 (standard SMPP)
- Expose SMPP TLS port 2776 (future use)
- Map container ports to host for client connections

**Project Setup**:
- Copy/mount project source to `/workspace` or `/home/developer/workspace`
- Pre-build smppcxx library dependencies if needed
- Ensure build artifacts persist in Docker volume

### 3.2 Docker Compose Setup

Create `docker-compose.dev.yml` to simplify container management:

```yaml
Services:
  smsc-dev:
    build:
      context: .
      dockerfile: Docker/smsc-dev/Dockerfile.dev
    container_name: smsc-dev-container
    ports:
      - "2222:22"        # SSH port mapping (for remote access)
      - "2775:2775"      # SMPP server port (standard SMPP port)
      - "2776:2776"      # SMPP server port (TLS, future use)
    volumes:
      - ./:/workspace    # Mount project root
      - dev-build:/workspace/build  # Persist build artifacts
      - /run/dbus:/run/dbus  # D-Bus socket for IPC
    environment:
      - DEVELOPER_USER=developer
      - DEVELOPER_PASSWORD=dev123  # Change to your preference
    command: /usr/sbin/sshd -D
    stdin_open: true
    tty: true
    privileged: true  # Needed for D-Bus and system integration

volumes:
  dev-build:
```

**Usage**:
```bash
docker-compose -f docker-compose.dev.yml up -d      # Start container
docker-compose -f docker-compose.dev.yml down        # Stop container
docker-compose -f docker-compose.dev.yml logs -f     # View logs
```

### 3.3 D-Bus and sdbus-c++ Setup

D-Bus is an IPC (Inter-Process Communication) system that allows communication between processes. This is useful for:
- Testing SMPP server services
- Monitoring and debugging
- Future integration with other system components

**In Dockerfile.dev**:
```dockerfile
# D-Bus and sdbus-c++
RUN apt-get update && apt-get install -y \
    dbus \
    dbus-user-session \
    libdbus-1-dev \
    busctl \
    net-tools

# sdbus-c++ (C++ D-Bus bindings)
# Option 1: From Ubuntu repos (if available)
RUN apt-get install -y libsdbus-c++-dev

# Option 2: Build from source (if not in repos)
# RUN git clone https://github.com/Kistler-Group/sdbus-cpp.git /tmp/sdbus-cpp && \
#     cd /tmp/sdbus-cpp && \
#     mkdir build && cd build && \
#     cmake .. && make && make install && \
#     cd / && rm -rf /tmp/sdbus-cpp
```

**Port Exposure for SMPP Server**:
The docker-compose.yml already maps:
- `2775:2775` - SMPP port (client connections)
- `2776:2776` - SMPP TLS port (future)

This allows SMPP clients on your host to connect to:
```
localhost:2775   (from host machine)
container:2775   (from other containers)
```

### 3.4 Testing the Network Setup

Inside container, verify ports are open:
```bash
netstat -tulpn | grep LISTEN
# Should show port 2775 and 2776 when server is running
```

From host machine, test connection to SMPP server:
```bash
telnet localhost 2775
# Should connect (or show connection refused if server isn't running)

nc -zv localhost 2775
# Alternative port check
```

---

## 4. Host Machine Setup

### 4.1 Prerequisites

**On Your Host (Windows 11)**:
- Docker Desktop installed and running
- Docker Compose installed
- VS Code installed
- VS Code Remote - SSH extension installed
- SSH client (built-in on Windows 10+, or use Git Bash)

### 4.2 VS Code Remote SSH Configuration

After Docker container is running, configure VS Code:

**Step 1**: Install VS Code Extensions
- Remote - SSH
- C/C++ (Microsoft)
- CMake Tools
- CMake Language Support

**Step 2**: Create SSH Config
Create `~/.ssh/config` on your host:
```
Host smsc-dev
    HostName localhost
    Port 2222
    User developer
    StrictHostKeyChecking no
```

**Step 3**: Connect in VS Code
- Open Command Palette: `Ctrl+Shift+P`
- Type: "Remote-SSH: Connect to Host"
- Select: "smsc-dev"
- VS Code will connect and open workspace in container

---

## 5. Build Environment

### 5.1 CMake Configuration

Update root `CMakeLists.txt` to:

```cmake
cmake_minimum_required(VERSION 3.20)
project(SimpleSmppServer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")

# Include smppcxx library
add_subdirectory(external/smppcxx)

# Main server executable
add_executable(smpp_server
    SMPPServer/src/main.cpp
    SMPPServer/src/tcp_server.cpp
    SMPPServer/src/smpp_service_manager.cpp
)
target_include_directories(smpp_server PRIVATE SMPPServer/include)
target_link_libraries(smpp_server PRIVATE smppcxx)

# Enable testing
enable_testing()
add_subdirectory(tests)
```

### 5.2 Build Process

**From within container (via SSH)**:
```bash
cd /workspace
mkdir -p build
cd build
cmake ..
cmake --build .              # Build all
cmake --build . --target run_tests  # Run tests
ctest -V                      # Detailed test output
```

---

## 6. Testing Setup (Google Test)

### 6.1 Install gtest

In Dockerfile.dev:
```dockerfile
RUN apt-get update && apt-get install -y libgtest-dev
RUN cd /usr/src/gtest && cmake . && cmake --build . --target all
RUN cp lib/*.a /usr/lib/
```

### 6.2 Test Structure

Create `tests/CMakeLists.txt`:
```cmake
find_package(GTest REQUIRED)

add_executable(run_tests
    test_tcp_server.cpp
    test_smpp_service_manager.cpp
)

target_link_libraries(run_tests PRIVATE GTest::GTest GTest::Main smppcxx)
target_include_directories(run_tests PRIVATE ${PROJECT_SOURCE_DIR}/SMPPServer/include)

add_test(NAME smpp_tests COMMAND run_tests)
```

**Test Files** to create:
- `tests/test_tcp_server.cpp` - TCP server functionality
- `tests/test_smpp_service_manager.cpp` - SMPP operations
- `tests/test_helpers.cpp` (optional) - Utility functions

---

## 7. Project Structure (After Setup)

```
simple_smpp_server/
├── CMakeLists.txt                 # Root build config
├── docker-compose.dev.yml         # Dev container orchestration
├── Docker/
│   └── smsc-dev/
│       ├── Dockerfile.dev         # Development image
│       └── (existing files)
├── SMPPServer/
│   ├── include/
│   │   ├── tcp_server.hpp
│   │   ├── smpp_service_manager.hpp
│   │   └── ...
│   ├── src/
│   │   ├── main.cpp
│   │   ├── tcp_server.cpp
│   │   ├── smpp_service_manager.cpp
│   │   └── ...
│   └── CMakeLists.txt             # Server build config
├── external/
│   └── smppcxx/                   # SMPP library (submodule)
├── tests/
│   ├── CMakeLists.txt             # Test build config
│   ├── test_tcp_server.cpp
│   ├── test_smpp_service_manager.cpp
│   └── ...
├── build/                          # Build artifacts (created during build)
├── doc/
│   ├── projectplan.md
│   ├── development_environment.md  # This file
│   └── projectexecution/
└── README.md
```

---

## 8. Development Workflow

### Typical Day:

1. **Start work**:
   ```bash
   docker-compose -f docker-compose.dev.yml up -d
   # Wait ~10 seconds for SSH to be ready
   # Connect in VS Code via Remote-SSH
   ```

2. **Build and test**:
   ```bash
   cd /workspace/build
   cmake --build .
   ctest -V
   ```

3. **Run and test SMPP server**:
   ```bash
   # Build the server
   cd /workspace/build && cmake --build .
   
   # Start the server (inside container via SSH)
   ssh developer@localhost -p 2222 '/workspace/build/smpp_server'
   # Or run in VS Code terminal
   
   # In another terminal, verify server is listening
   netstat -tulpn | grep 2775
   ```

4. **Test connectivity and ports** (from host or another container):
   ```bash
   # Test SMPP port is accessible from host
   telnet localhost 2775
   # Or
   nc -zv localhost 2775
   
   # Query D-Bus services (if using D-Bus)
   ssh developer@localhost -p 2222 'busctl list'
   
   # Monitor SMPP server via D-Bus (future)
   ssh developer@localhost -p 2222 'busctl introspect com.example.smsc /com/example/smsc/Server'
   ```

5. **Develop**:
   - Edit code in VS Code (all changes reflected in container)
   - Rebuild and test iteratively
   - Server runs in separate SSH session or in background

6. **End work** (optional):
   ```bash
   # Kill SMPP server
   ssh developer@localhost -p 2222 'pkill -f smpp_server'
   
   # Stop container (or leave running)
   docker-compose -f docker-compose.dev.yml stop
   # Or leave running for next session
   ```

---

## 9. Exit Criteria - Environment Setup is "Done"

✅ **All of the following must be verified**:

### Docker & SSH
- [ ] Docker container builds without errors
- [ ] SSH connection works: `ssh developer@localhost -p 2222`
- [ ] Can execute commands remotely via SSH

### Compiler & Build
- [ ] GCC version supports C++17: `g++ --version` (inside container)
- [ ] CMake is installed: `cmake --version` (inside container)
- [ ] Root `CMakeLists.txt` exists and is valid
- [ ] `cmake ..` runs without errors
- [ ] `cmake --build .` compiles without warnings

### Project Structure
- [ ] `SMPPServer/` directory with header and source files
- [ ] `tests/` directory created
- [ ] `external/smppcxx/` submodule present and accessible
- [ ] All paths in CMakeLists.txt are correct

### Testing Framework
- [ ] Google Test is installed in container
- [ ] `tests/CMakeLists.txt` exists and is valid
- [ ] At least one dummy test file created
- [ ] `ctest -V` runs without errors (even if tests are minimal)

### VS Code Remote SSH
- [ ] VS Code Remote SSH extension installed
- [ ] SSH config file created on host (`~/.ssh/config`)
- [ ] Can connect to `smsc-dev` from VS Code
- [ ] Can browse project files in VS Code via remote SSH
- [ ] Can edit a file and see it reflected in container

### Verification Test
- [ ] Create a simple test file in `tests/` that verifies:
  ```cpp
  #include <gtest/gtest.h>
  TEST(EnvSetup, BasicTest) {
    EXPECT_EQ(1 + 1, 2);
  }
  ```
- [ ] Test compiles and runs successfully: `ctest -V`

### Documentation
- [ ] This development environment document is complete
- [ ] Steps to set up SSH and VS Code are documented
- [ ] Build and test commands are documented
- [ ] All setup decisions are recorded

### D-Bus and Network Testing
- [ ] D-Bus is installed in container: `apt list --installed | grep dbus`
- [ ] busctl is available: `which busctl`
- [ ] sdbus-c++ is installed: `apt list --installed | grep sdbus`
- [ ] Can list D-Bus services: `busctl list`
- [ ] SMPP port 2775 is exposed in docker-compose
- [ ] SMPP port 2775 is accessible from host: `telnet localhost 2775` or `nc -zv localhost 2775`
- [ ] Can verify listening ports from inside container: `netstat -tulpn` shows port 2775
- [ ] Server can be started and stopped cleanly

---

## 10. D-Bus Integration & Testing

### busctl Commands for Debugging

Once SMPP server is running (and if it exposes D-Bus services):

```bash
# List all D-Bus services
busctl list

# Introspect a service (example)
busctl introspect com.example.smsc /com/example/smsc/Server

# Monitor D-Bus traffic
busctl monitor com.example.smsc

# Call a D-Bus method (example)
busctl call com.example.smsc /com/example/smsc/Server \
    com.example.smsc.Server GetStatus
```

### Network Port Verification

```bash
# From inside container - verify ports are listening
netstat -tulpn | grep LISTEN

# From host - test SMPP server connectivity
telnet localhost 2775
nc -zv localhost 2775
ss -tulpn | grep 2775

# If using SMPP client library, can test:
# - Bind request/response
# - Unbind request/response
# - Keep-alive (ENQUIRE_LINK)
```

### Integration with Server Code

In your server implementation (future Phase 1.2):
- Use sdbus-c++ to expose D-Bus services for monitoring
- Listen on port 2775 for SMPP client connections
- Both can coexist - D-Bus for internal monitoring, SMPP for external clients

---

## 11. Troubleshooting

| Issue | Solution |
|-------|----------|
| SSH connection refused | Check Docker container is running: `docker ps` |
| CMake version too old | Ubuntu 24.04 has 3.28+; update Dockerfile if needed |
| smppcxx headers not found | Verify submodule is initialized: `git submodule update --init` |
| gtest not found | Install in Dockerfile: `apt-get install libgtest-dev` |
| VS Code can't find IntelliSense | Install C/C++ extension and restart VS Code |
| SMPP port 2775 not accessible | Check docker-compose.yml port mapping; verify server is running: `netstat -tulpn` |
| D-Bus services not found | Check dbus is installed: `apt list --installed \| grep dbus`; restart dbus-daemon |
| busctl command not found | Install in Dockerfile: `apt-get install dbus` (includes busctl) |
| sdbus-c++ headers not found | Install in Dockerfile: `apt-get install libsdbus-c++-dev` |
| /run/dbus not available | Ensure volume mount in docker-compose: `- /run/dbus:/run/dbus` |

---

## 11. Next Steps

1. Create `Docker/smsc-dev/Dockerfile.dev` with all required packages
2. Create `docker-compose.dev.yml` file
3. Update root `CMakeLists.txt` for the build setup
4. Create `tests/CMakeLists.txt` template
5. Build Docker image and start container
6. Verify all exit criteria
7. Document any additional setup needed

**Start Development Environment Setup**: When approved
