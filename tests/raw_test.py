#!/usr/bin/env python3
"""
Raw SMPP PDU Test - sends binary SMPP protocol messages directly
"""
import socket
import struct
import time
import sys

def create_bind_transmitter_pdu(system_id="test", password="test", seq=1):
    """Create a BIND_TRANSMITTER PDU"""
    # SMPP header format: command_length(4) | command_id(4) | command_status(4) | sequence_number(4)
    command_id = 0x00000002  # BIND_TRANSMITTER
    command_status = 0x00000000
    sequence_number = seq

    # Body: system_id (C-string) | password (C-string) | system_type (C-string) | interface_version (1) | addr_ton (1) | addr_npi (1) | address_range (C-string)
    body = b''
    body += system_id.encode('ascii') + b'\x00'
    body += password.encode('ascii') + b'\x00'
    body += b'SMPP\x00'  # system_type
    body += b'\x34'      # interface_version (0x34 = 3.4)
    body += b'\x01'      # addr_ton (1 = international)
    body += b'\x01'      # addr_npi (1 = ISDN)
    body += b'\x00'      # address_range (empty)

    command_length = 16 + len(body)

    # Build header (big-endian)
    header = struct.pack('>IIII', command_length, command_id, command_status, sequence_number)
    return header + body

def test_bind_transmitter(host='127.0.0.1', port=2775):
    """Test BIND_TRANSMITTER"""
    print("\n" + "="*60)
    print("Raw SMPP PDU Test - BIND_TRANSMITTER")
    print("="*60)

    try:
        # Connect
        print(f"\n[1] Connecting to {host}:{port}...")
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)
        sock.connect((host, port))
        print("    [OK] Connected")

        # Send BIND_TRANSMITTER
        print(f"\n[2] Sending BIND_TRANSMITTER PDU...")
        pdu = create_bind_transmitter_pdu("test", "test", seq=1)
        print(f"    PDU bytes ({len(pdu)}): {pdu.hex()}")
        sock.sendall(pdu)
        print("    [OK] PDU sent")

        # Receive response
        print(f"\n[3] Waiting for BIND_TRANSMITTER_RESP...")
        resp = sock.recv(1024)
        if len(resp) >= 16:
            cmd_length, cmd_id, cmd_status, seq_num = struct.unpack('>IIII', resp[:16])
            print(f"    Response header:")
            print(f"      command_length: {cmd_length}")
            print(f"      command_id:     0x{cmd_id:08x}")
            print(f"      command_status: 0x{cmd_status:08x}")
            print(f"      sequence_number: {seq_num}")

            if cmd_status == 0:
                print("    [OK] Bind successful!")

                # Try ENQUIRE_LINK
                print(f"\n[4] Sending ENQUIRE_LINK...")
                eq_pdu = struct.pack('>IIII', 16, 0x0000000F, 0, 2)
                sock.sendall(eq_pdu)
                eq_resp = sock.recv(1024)
                print("    [OK] ENQUIRE_LINK response received")

                # Close
                print(f"\n[5] Closing connection...")
                sock.close()
                print("    [OK] Connection closed")

                print("\n" + "="*60)
                print("[PASS] ALL TESTS PASSED")
                print("="*60 + "\n")
                return 0
            else:
                print(f"    [FAIL] Bind failed with status 0x{cmd_status:08x}")
                return 1
        else:
            print(f"    [FAIL] Incomplete response ({len(resp)} bytes)")
            return 1

    except socket.timeout:
        print("[FAIL] Socket timeout - server may not be listening")
        return 1
    except ConnectionRefusedError:
        print("[FAIL] Connection refused - is server running on this port?")
        return 1
    except Exception as e:
        print(f"[FAIL] Error: {e}")
        import traceback
        traceback.print_exc()
        return 1

if __name__ == '__main__':
    sys.exit(test_bind_transmitter())
