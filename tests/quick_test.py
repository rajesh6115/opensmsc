#!/usr/bin/env python3
"""
Quick SMPP Server Test - Minimal Example
Tests basic BIND and ENQUIRE_LINK
"""

import sys
import os

# Fix Unicode encoding on Windows
if os.name == 'nt':
    sys.stdout.reconfigure(encoding='utf-8')

try:
    import smpplib.client
except ImportError:
    print("ERROR: smpplib not installed")
    print("Install with: pip install smpplib")
    sys.exit(1)

def quick_test():
    print("\n" + "="*50)
    print("Quick SMPP Server Test")
    print("="*50)

    try:
        # 1. Connect
        print("\n1. Connecting to 127.0.0.1:2775...")
        client = smpplib.client.Client('127.0.0.1', 2775, timeout=10)
        client.connect()
        print("   [OK] Connected")

        # 2. Bind
        print("\n2. Binding as TRANSMITTER...")
        client.bind_transmitter(system_id='test', password='test')
        print("   [OK] Bound successfully")

        # 3. Keep-alive
        print("\n3. Sending ENQUIRE_LINK (keep-alive)...")
        client.enquire_link()
        print("   [OK] Keep-alive sent, response received")

        # 4. Unbind
        print("\n4. Unbinding...")
        client.unbind()
        print("   [OK] Unbind successful")

        # 5. Disconnect
        print("\n5. Disconnecting...")
        client.disconnect()
        print("   [OK] Disconnected")

        print("\n" + "="*50)
        print("[PASS] ALL TESTS PASSED")
        print("="*50 + "\n")
        return 0

    except Exception as e:
        print(f"\n[FAIL] ERROR: {e}")
        print("\nTroubleshooting:")
        print("  - Is server running? Check: docker logs smsc-dev-container")
        print("  - Firewall blocking port 2775?")
        print("  - Credentials correct? (default: test / test)")
        import traceback
        traceback.print_exc()
        return 1

if __name__ == '__main__':
    sys.exit(quick_test())
