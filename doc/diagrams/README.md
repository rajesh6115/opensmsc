# PlantUML Diagrams

This directory contains sequence diagrams for the Simple SMPP Server microservices architecture, rendered in PlantUML format.

## Files

### connection_establishment.puml
**Title**: Connection Establishment Flow

Shows the complete lifecycle of a new client connection:
- TCP three-way handshake (SYN/SYN-ACK/ACK)
- IP validation against whitelist
- Process spawning (SmppClientHandler)
- Socket handoff via D-Bus (ClaimSocket method)
- Direct communication setup

**Key Participants**:
- Client (SMPP client)
- SMPPServer (connection acceptor)
- SmppClientHandler (per-client handler)

---

### bind_transmitter_flow.puml
**Title**: BIND_TRANSMITTER Authentication Flow

Shows SMPP authentication during BIND operation:
- BIND_TRANSMITTER request parsing
- Credential extraction
- Authenticator service call (D-Bus)
- Credential validation
- Session state management
- Response to client

**Key Participants**:
- SMPP Client
- SmppClientHandler
- Authenticator (D-Bus service)
- spdlog (logging library)

---

### enquire_link_flow.puml
**Title**: ENQUIRE_LINK Keep-Alive Flow

Shows keep-alive message handling:
- ENQUIRE_LINK request parsing
- Session validation
- Response preparation
- Logging
- Keep-alive response to client

**Key Participants**:
- SMPP Client
- SmppClientHandler
- spdlog (logging library)

---

## Viewing Diagrams

### Online (Recommended)

Use the [PlantUML Online Editor](http://www.plantuml.com/plantuml/uml/):
1. Copy the contents of any `.puml` file
2. Paste into the editor
3. Diagrams render automatically

### Command Line

Install PlantUML:
```bash
# Ubuntu/Debian
sudo apt-get install plantuml

# macOS
brew install plantuml
```

Generate PNG:
```bash
plantuml connection_establishment.puml
# Creates: connection_establishment.png
```

Generate SVG (scalable):
```bash
plantuml -tsvg connection_establishment.puml
# Creates: connection_establishment.svg
```

### In Documentation

Markdown with PlantUML support (requires plugin):
```markdown
```plantuml
!include ./diagrams/connection_establishment.puml
`​``
```

### Visual Studio Code

Install **PlantUML** extension:
1. Open any `.puml` file
2. Right-click → "Preview Current Diagram"
3. View renders in side panel

---

## Editing Diagrams

PlantUML syntax reference:
- [Official Documentation](https://plantuml.com/guide)
- [Sequence Diagram Guide](https://plantuml.com/sequence-diagram)

### Common Elements

**Participants**:
```plantuml
participant "Display Name" as alias
```

**Messages**:
```plantuml
actor1 -> actor2: Message text
actor1 <- actor2: Response
actor1 -> actor1: Internal action
```

**Autonumbering**:
```plantuml
autonumber
```

**Notes**:
```plantuml
note over actor1: Note text
note over actor1, actor2: Spanning note
```

**Alternatives**:
```plantuml
alt Condition 1
    actor1 -> actor2: Action
else Condition 2
    actor2 -> actor1: Alternative
end
```

---

## CI/CD Integration

To generate diagrams automatically in CI/CD:

```bash
#!/bin/bash
cd doc/diagrams
for file in *.puml; do
    plantuml -tsvg "$file"
done
```

---

## Theme Customization

Current theme: `plain` with custom styling.

To change theme:
```plantuml
!theme default
!theme dark
!theme blueprint
```

Custom colors:
```plantuml
skinparam backgroundColor #FEFEFE
skinparam sequenceMessageAlign center
skinparam actor {
    BackgroundColor #FF6B6B
    BorderColor #000000
}
```

---

## Performance Notes

- PlantUML rendering is fast (<1 second per diagram)
- SVG output is scalable and recommended for web
- PNG suitable for printed documentation
- Keep diagrams under 50 participants for readability

---

**Last Updated**: 2026-04-20
