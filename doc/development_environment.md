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

# Libraries for SMPP development
- libssl-dev (if TLS needed later)
- pkg-config (for finding libraries)

# Testing
- google-test library (libgtest-dev + cmake)
```

**SSH Configuration**:
- Create non-root user (e.g., `developer`)
- Configure `/etc/ssh/sshd_config` for password-less SSH
- Generate host keys on startup
- Expose port 22 (or custom port like 2222)

**Project Setup**:
- Copy/mount project source to `/workspace` or `/home/developer/workspace`
- Pre-build smppcxx library dependencies if needed

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
      - "2222:22"        # SSH port mapping
    volumes:
      - ./:/workspace    # Mount project root
      - dev-build:/workspace/build  # Persist build artifacts
    environment:
      - DEVELOPER_USER=developer
      - DEVELOPER_PASSWORD=dev123  # Change to your preference
    command: /usr/sbin/sshd -D
    stdin_open: true
    tty: true

volumes:
  dev-build:
```

**Usage**:
```bash
docker-compose -f docker-compose.dev.yml up -d      # Start container
docker-compose -f docker-compose.dev.yml down        # Stop container
docker-compose -f docker-compose.dev.yml logs -f     # View logs
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

3. **Develop**:
   - Edit code in VS Code (all changes reflected in container)
   - Rebuild and test iteratively

4. **End work** (optional):
   ```bash
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

---

## 10. Troubleshooting

| Issue | Solution |
|-------|----------|
| SSH connection refused | Check Docker container is running: `docker ps` |
| CMake version too old | Ubuntu 24.04 has 3.28+; update Dockerfile if needed |
| smppcxx headers not found | Verify submodule is initialized: `git submodule update --init` |
| gtest not found | Install in Dockerfile: `apt-get install libgtest-dev` |
| VS Code can't find IntelliSense | Install C/C++ extension and restart VS Code |

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
