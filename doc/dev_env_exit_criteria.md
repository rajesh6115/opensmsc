# Development Environment Setup - Exit Criteria Checklist

**Phase**: Pre-Development Setup  
**Goal**: Establish persistent Docker development environment with SSH + VS Code  
**Created**: 2026-04-18

---

## Phase Breakdown

### Phase A: Docker Setup
- [ ] `Docker/smsc-dev/Dockerfile.dev` created with:
  - [ ] Base image: Ubuntu 24.04
  - [ ] GCC compiler (C++17 support)
  - [ ] CMake 3.20+
  - [ ] OpenSSH server
  - [ ] Build tools (git, make, etc.)
  - [ ] Google Test (gtest) library
  - [ ] pkg-config, libssl-dev
  
- [ ] `docker-compose.dev.yml` created with:
  - [ ] Container service definition
  - [ ] Port mapping (2222→22 for SSH)
  - [ ] Volume mounts for project and build artifacts
  - [ ] Startup command for SSH

- [ ] Docker image builds successfully:
  ```bash
  docker-compose -f docker-compose.dev.yml build
  ```

### Phase B: Container & SSH Access
- [ ] Container starts without errors:
  ```bash
  docker-compose -f docker-compose.dev.yml up -d
  ```

- [ ] SSH connection works:
  ```bash
  ssh developer@localhost -p 2222
  # Should prompt for password or connect with key
  ```

- [ ] Can execute commands in container:
  ```bash
  ssh developer@localhost -p 2222 'cmake --version'
  # Should show CMake version
  ```

- [ ] Project files visible in container:
  ```bash
  ssh developer@localhost -p 2222 'ls -la /workspace'
  # Should list project files
  ```

### Phase C: Host Machine Configuration
- [ ] VS Code Remote - SSH extension installed
- [ ] SSH config file created: `~/.ssh/config`
  ```
  Host smsc-dev
      HostName localhost
      Port 2222
      User developer
      StrictHostKeyChecking no
  ```

- [ ] SSH key-based auth configured (optional but recommended):
  ```bash
  ssh-keygen -t rsa -N "" -f ~/.ssh/id_rsa
  ssh-copy-id -i ~/.ssh/id_rsa.pub -p 2222 developer@localhost
  ```

### Phase D: Build Environment
- [ ] Root `CMakeLists.txt` updated with:
  - [ ] Project name and version
  - [ ] C++17 standard
  - [ ] smppcxx subdirectory
  - [ ] Server executable target
  - [ ] Testing enabled
  - [ ] No syntax errors

- [ ] `SMPPServer/CMakeLists.txt` created (or updated):
  - [ ] Links against smppcxx
  - [ ] Includes correct header paths

- [ ] `tests/CMakeLists.txt` created with:
  - [ ] Google Test configuration
  - [ ] Test executable definition
  - [ ] Proper linking

- [ ] CMake configuration succeeds:
  ```bash
  ssh developer@localhost -p 2222 'cd /workspace && mkdir -p build && cd build && cmake ..'
  # Should complete without errors
  ```

### Phase E: Compilation
- [ ] Build succeeds without warnings:
  ```bash
  ssh developer@localhost -p 2222 'cd /workspace/build && cmake --build .'
  # Should compile all targets
  ```

- [ ] Executable created:
  ```bash
  ssh developer@localhost -p 2222 'ls -la /workspace/build/smpp_server'
  # Should exist
  ```

### Phase F: Testing Framework
- [ ] Google Test installed in container:
  ```bash
  ssh developer@localhost -p 2222 'apt list --installed | grep gtest'
  # Should show gtest package
  ```

- [ ] Sample test file created: `tests/test_sample.cpp`
  ```cpp
  #include <gtest/gtest.h>
  TEST(EnvSetup, VerifyCompiler) {
    EXPECT_EQ(1 + 1, 2);
  }
  ```

- [ ] Tests compile:
  ```bash
  ssh developer@localhost -p 2222 'cd /workspace/build && cmake --build . --target run_tests'
  ```

- [ ] Tests run:
  ```bash
  ssh developer@localhost -p 2222 'cd /workspace/build && ctest -V'
  # Should show at least one test passing
  ```

### Phase G: VS Code Remote SSH
- [ ] C/C++ extension installed in VS Code
- [ ] CMake Tools extension installed in VS Code
- [ ] Can connect to remote host in VS Code:
  - [ ] Command Palette → "Remote-SSH: Connect to Host"
  - [ ] Select "smsc-dev"
  - [ ] VS Code opens remote window

- [ ] Project folder opens in remote VS Code:
  - [ ] Files visible in Explorer
  - [ ] Can browse SMPPServer/, tests/, external/ folders
  - [ ] Can open and edit `.cpp` and `.h` files

- [ ] IntelliSense works:
  - [ ] Open a .cpp file
  - [ ] Hover over a standard library function
  - [ ] Should show documentation/definition

### Phase H: Documentation & Verification
- [ ] `doc/development_environment.md` complete
- [ ] This exit criteria document completed
- [ ] Steps to run, build, and test are documented
- [ ] Troubleshooting section filled with known issues

---

## Verification Test Script

Run this to verify everything is working:

```bash
#!/bin/bash
echo "=== Development Environment Verification ==="

echo "1. Docker container status..."
docker ps | grep smsc-dev

echo "2. SSH connectivity..."
ssh developer@localhost -p 2222 'echo "SSH OK"'

echo "3. GCC version (C++17 support)..."
ssh developer@localhost -p 2222 'g++ --version'

echo "4. CMake version..."
ssh developer@localhost -p 2222 'cmake --version'

echo "5. Google Test installed..."
ssh developer@localhost -p 2222 'dpkg -l | grep gtest'

echo "6. Build test..."
ssh developer@localhost -p 2222 'cd /workspace/build && cmake --build . && echo "Build OK"'

echo "7. Test execution..."
ssh developer@localhost -p 2222 'cd /workspace/build && ctest -V && echo "Tests OK"'

echo "=== All Checks Complete ==="
```

---

## Success Criteria (All Must Pass)

✅ **Docker & SSH**
- Container runs reliably
- SSH access works with one command
- Files are persistent (no rebuild needed)

✅ **Compiler Toolchain**
- GCC supports C++17
- CMake configuration succeeds
- Build produces no warnings

✅ **Project Structure**
- All required directories exist
- CMakeLists.txt files are valid
- smppcxx submodule is accessible

✅ **Testing**
- Google Test is installed
- Test compilation works
- At least one test passes

✅ **Development Experience**
- VS Code connects via Remote SSH
- Can edit files and see changes in container
- IntelliSense works for C++

---

## What's NOT Included Yet

These are out of scope for dev environment setup:
- Building the actual SMPP server code (Phase 1.1)
- Integration with real SMPP clients for testing
- Docker production image (this is dev-focused)
- Performance optimizations

---

## Timeline

| Step | Time | Notes |
|------|------|-------|
| Create Dockerfile.dev | 30 min | Install packages, configure SSH |
| Create docker-compose.yml | 15 min | Container orchestration |
| Build Docker image | 15-30 min | Depends on internet/packages |
| Configure VS Code SSH | 15 min | Extensions + config |
| Verify build pipeline | 15 min | CMake + compilation test |
| Document setup | 15 min | This checklist + troubleshooting |
| **Total** | **1.5-2 hours** | One-time setup |

---

## Status

- [ ] Phase A: Docker Setup
- [ ] Phase B: Container & SSH
- [ ] Phase C: Host Machine Config
- [ ] Phase D: Build Environment
- [ ] Phase E: Compilation
- [ ] Phase F: Testing Framework
- [ ] Phase G: VS Code Remote SSH
- [ ] Phase H: Documentation
- [ ] **[DEVELOPMENT ENVIRONMENT READY]**

Next: Start Phase 1.1 (smppcxx integration into CMake)
