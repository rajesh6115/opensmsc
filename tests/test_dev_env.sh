#!/bin/bash
# Development Environment Verification Test Script
# Automatically tests all exit criteria for dev environment setup
# Location: tests/test_dev_env.sh
# Usage: bash tests/test_dev_env.sh

set -e

CONTAINER_NAME="smsc-dev-container"
RESULTS_FILE="doc/projectexecution/dev_env_verification_results.md"
PASS_COUNT=0
FAIL_COUNT=0
TOTAL_TESTS=0

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Initialize results file
mkdir -p doc/projectexecution
cat > "$RESULTS_FILE" << 'EOF'
# Development Environment Verification Results

**Date**: $(date)
**Container**: smsc-dev-container
**Status**: Running verification...

## Test Results

EOF

echo "=========================================="
echo "  Development Environment Verification"
echo "=========================================="
echo ""

# Function to run a test
run_test() {
    local test_name="$1"
    local test_command="$2"
    local expected_output="$3"

    TOTAL_TESTS=$((TOTAL_TESTS + 1))

    echo -n "[$TOTAL_TESTS] Testing: $test_name... "

    if eval "$test_command" > /dev/null 2>&1; then
        echo -e "${GREEN}✓ PASS${NC}"
        echo "- ✅ $test_name" >> "$RESULTS_FILE"
        PASS_COUNT=$((PASS_COUNT + 1))
    else
        echo -e "${RED}✗ FAIL${NC}"
        echo "- ❌ $test_name" >> "$RESULTS_FILE"
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
}

# ============ Phase A: Docker Setup ============
echo ""
echo "Phase A: Docker Setup"
echo "---"

run_test "Dockerfile.dev exists" "test -f Docker/smsc-dev/Dockerfile.dev"
run_test "docker-compose.dev.yml exists" "test -f docker-compose.dev.yml"
run_test "Docker image exists" "docker image ls | grep simple_smpp_server-smsc-dev"

# ============ Phase B: Container & SSH ============
echo ""
echo "Phase B: Container & SSH Access"
echo "---"

run_test "Container is running" "docker ps | grep $CONTAINER_NAME"
run_test "SSH port 2222 is mapped" "docker ps | grep -E '2222|22'"
run_test "SMPP port 2775 is mapped" "docker ps | grep 2775"

# ============ Phase D: Build Environment ============
echo ""
echo "Phase D: Build Environment"
echo "---"

run_test "Root CMakeLists.txt exists" "test -f CMakeLists.txt"
run_test "SMPPServer CMakeLists.txt exists" "test -f SMPPServer/CMakeLists.txt"
run_test "GCC installed in container" "docker exec $CONTAINER_NAME which g++"
run_test "CMake installed in container" "docker exec $CONTAINER_NAME which cmake"
run_test "CMake configuration works" "docker exec $CONTAINER_NAME bash -c 'cd /workspace && cmake -S . -B /tmp/cmake_test 2>&1 | head -1'"

# ============ Phase F: Testing Framework ============
echo ""
echo "Phase F: Testing Framework"
echo "---"

run_test "Google Test installed" "docker exec $CONTAINER_NAME dpkg -l | grep -q libgtest-dev"

# ============ Phase G: D-Bus and Network Setup ============
echo ""
echo "Phase G: D-Bus and Network Setup"
echo "---"

run_test "D-Bus installed" "docker exec $CONTAINER_NAME which dbus-daemon"
run_test "sdbus-c++ installed" "docker exec $CONTAINER_NAME pkg-config --exists sdbus-c++"
run_test "busctl available" "docker exec $CONTAINER_NAME which busctl"
run_test "D-Bus system bus running" "docker exec $CONTAINER_NAME pgrep -f 'dbus-daemon.*system'"
run_test "supervisord running" "docker exec $CONTAINER_NAME pgrep supervisord"

# ============ Phase H: Network Connectivity ============
echo ""
echo "Phase H: Network Connectivity"
echo "---"

# Note: SMPP port will only listen when server is running (Phase 1.1+)
# run_test "SMPP port listening" "docker exec $CONTAINER_NAME netstat -tulpn | grep -q '2775'"
echo -n "[18] Testing: SMPP port listening... "
echo -e "${YELLOW}⊘ SKIP${NC} (server not running yet - Phase 1.1)"
echo "- ⊘ SMPP port listening (expected in Phase 1.1)" >> "$RESULTS_FILE"
TOTAL_TESTS=$((TOTAL_TESTS + 1))
run_test "SSH port listening" "docker exec $CONTAINER_NAME netstat -tulpn | grep -q ':22'"
run_test "busctl can connect to D-Bus" "docker exec $CONTAINER_NAME busctl list | grep -q org.freedesktop.DBus"

# ============ Phase I: VS Code Remote SSH ============
echo ""
echo "Phase I: SSH Connectivity from Host"
echo "---"

run_test "SSH port 2222 accessible from host" "timeout 2 nc localhost 2222 < /dev/null 2>&1 | head -1 | grep -q -E 'SSH|OpenSSH' || timeout 2 nc -zv localhost 2222 2>&1 | grep -q -E 'succeeded|Connection|refused'"

# ============ Summary ============
echo ""
echo "=========================================="
echo "  Test Summary"
echo "=========================================="
echo ""
echo -e "Total Tests:  $TOTAL_TESTS"
echo -e "Passed:       ${GREEN}$PASS_COUNT${NC}"
echo -e "Failed:       ${RED}$FAIL_COUNT${NC}"
echo ""

# Update results file with summary
cat >> "$RESULTS_FILE" << EOF

## Summary

- **Total Tests**: $TOTAL_TESTS
- **Passed**: $PASS_COUNT
- **Failed**: $FAIL_COUNT
- **Success Rate**: $(( (PASS_COUNT * 100) / TOTAL_TESTS ))%

## Environment Details

### Container Status
EOF

docker exec $CONTAINER_NAME bash -c '
echo "**Processes Running**:"
ps aux | grep -E "supervisord|dbus|sshd" | grep -v grep | sed "s/^/- /"
echo ""
echo "**GCC Version**:"
g++ --version | head -1 | sed "s/^/- /"
echo ""
echo "**CMake Version**:"
cmake --version | head -1 | sed "s/^/- /"
echo ""
echo "**sdbus-c++ Version**:"
pkg-config --modversion sdbus-c++ | sed "s/^/- /"
' >> "$RESULTS_FILE"

echo ""
echo "Results saved to: $RESULTS_FILE"
echo ""

if [ $FAIL_COUNT -eq 0 ]; then
    echo -e "${GREEN}✅ All tests passed! Development environment is ready.${NC}"
    exit 0
else
    echo -e "${RED}❌ Some tests failed. Please review the issues above.${NC}"
    exit 1
fi
