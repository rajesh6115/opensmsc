# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [0.1.0] - 2026-04-20

### Status
🟡 **Pre-Release** - Architecture design complete, implementation not started

### Added

#### Architecture & Design
- ✅ Process-per-client microservices architecture (v2.0)
  - SMPPServer: TCP acceptor + dispatcher
  - SmppClientHandler: Per-client protocol handler
  - Authenticator: Separate D-Bus service
  - spdlog: Logging library (not a service)

#### D-Bus Interfaces
- ✅ com.opensmsc.SMPPServer interface
  - ClaimSocket(port) method for socket handoff
  - GetConnectionCount() method
  - GetConnections() method
  - Shutdown() method
  - Properties: Port, Status, MaxConnections, Uptime

- ✅ com.opensmsc.SMPPAuthenticator interface
  - Authenticate(username, password) method
  - ValidateUsername(username) method
  - GetAccountInfo(username) method
  - ReloadCredentials() method
  - Properties: Status, CredentialCount, LastReload, ConfigPath

- ✅ D-Bus service files for systemd integration

#### Documentation
- ✅ README.md - Updated with microservices vision
- ✅ SYSTEM_OOAD_DESIGN.md - System-wide architecture
- ✅ CURRENT_STATUS.md - Implementation status
- ✅ BUILD_AND_TEST_PLAN.md - Build & test procedures
- ✅ DBUS_INTERFACES.md - Complete D-Bus specs with examples
- ✅ doc/SmppServer/ARCHITECTURE_UPDATE_v2.md - Migration from v1 to v2
- ✅ doc/SmppServer/README.md - SmppServer LLD navigation

#### Diagrams
- ✅ connection_establishment.puml - TCP handshake + socket handoff
- ✅ bind_transmitter_flow.puml - BIND authentication flow
- ✅ enquire_link_flow.puml - Keep-alive message flow
- ✅ PlantUML README with usage instructions

#### Versioning
- ✅ VERSION file (0.1.0)
- ✅ version.h header for C++ code
- ✅ Versioning policy documentation

### Not Yet Implemented

- ❌ SMPPServer executable (code)
- ❌ SmppClientHandler executable (code)
- ❌ Authenticator service (code)
- ❌ SMPP protocol implementation
- ❌ Socket I/O (ASIO)
- ❌ PDU parsing
- ❌ Session management
- ❌ Message handlers (BIND, UNBIND, ENQUIRE_LINK)
- ❌ D-Bus integration (runtime)
- ❌ spdlog integration
- ❌ Unit tests
- ❌ Integration tests

### Implementation Roadmap

**Phase 1: Foundation**
- Phase 1.A: SMPPServer executable (TCP + dispatcher)
  - TcpServer class
  - IpValidator class
  - ProcessSpawner class
  - SocketTransfer class (D-Bus)
  - Main server logic

- Phase 1.B: SmppClientHandler executable (protocol + auth)
  - SmppMessage class
  - SmppSession class
  - SmppMessageParser class
  - SmppMessageProcessor class
  - Handler classes (BIND, UNBIND, ENQUIRE_LINK)
  - DBusAuthenticator class
  - Main handler logic

- Phase 1.C: Infrastructure
  - Authenticator D-Bus service
  - spdlog integration
  - Build system (CMake)

**Phase 2: Message Operations**
- SUBMIT_SM (send messages)
- DELIVER_SM (receive messages)
- Message queuing
- Message persistence

**Phase 3: Observability**
- Prometheus metrics
- Grafana dashboards
- Health checks
- Alerting

**Phase 4: Clustering & HA**
- Load balancing
- Failover
- Replication

---

## Versioning Scheme

### 0.1.x (Pre-Release)
- Architecture and design phase
- Only patch version bumps (0.1.0 → 0.1.1 → 0.1.2)
- No major/minor version bumps until MVP working

### 1.0.0 (Stable MVP)
- Minimum viable product released
- Core functionality working (BIND, UNBIND, ENQUIRE_LINK)
- Ready for initial deployment
- Full test coverage

### 1.x.x (Enhancement)
- New features and improvements
- Minor version bumps for new features
- Patch version bumps for bug fixes

### 2.0.0+
- Major version bumps for breaking changes
- Architecture changes that require client updates

---

## How to Update Version

### For Patch Releases (0.1.0 → 0.1.1)

When making bug fixes or documentation updates:

```bash
# Update VERSION file
echo "0.1.1" > VERSION

# Update version.h
# Change: #define SMPP_SERVER_VERSION_PATCH 1

# Commit
git commit -am "[Release] Version 0.1.1 - <description>"

# Tag
git tag -a v0.1.1 -m "Release version 0.1.1"

# Push
git push origin main --tags
```

### For Minor/Major Releases (0.1.x → 1.0.0)

When releasing MVP or significant features:

```bash
# Update VERSION file
echo "1.0.0" > VERSION

# Update version.h
# Change: #define SMPP_SERVER_VERSION_MAJOR 1
# Change: #define SMPP_SERVER_VERSION_MINOR 0
# Change: #define SMPP_SERVER_VERSION_PATCH 0

# Commit
git commit -am "[Release] Version 1.0.0 - Stable MVP"

# Tag
git tag -a v1.0.0 -m "Release version 1.0.0 - Stable MVP"

# Push
git push origin main --tags
```

---

## Tags and Releases

All releases are tagged in git:

```bash
# View all tags
git tag -l

# View specific release
git show v0.1.0
```

GitHub releases are created from these tags for:
- Release notes
- Binary downloads (future)
- Changelog

---

**Last Updated**: 2026-04-20  
**Current Version**: 0.1.0  
**Next Milestone**: 1.0.0 (MVP with working BIND/UNBIND)
