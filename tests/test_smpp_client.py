#!/usr/bin/env python3
"""
SMPP Client Test Suite for simple_smpp_server
Tests BIND, UNBIND, and ENQUIRE_LINK operations
"""

import sys
import time
import os

# Fix Unicode encoding on Windows
if os.name == 'nt':
    sys.stdout.reconfigure(encoding='utf-8')

import smpplib.client
import smpplib.consts
import smpplib.gsm

def test_bind_transmitter(host='127.0.0.1', port=2775):
    """Test BIND_TRANSMITTER operation"""
    print("\n" + "="*60)
    print("TEST 1: BIND_TRANSMITTER")
    print("="*60)

    try:
        client = smpplib.client.Client(host, port, timeout=10)
        print(f"[✓] Connected to {host}:{port}")

        # Send BIND_TRANSMITTER
        client.connect()
        print("[✓] Socket connected")

        client.bind_transmitter(system_id='test', password='test')
        print("[✓] BIND_TRANSMITTER successful (username: test, password: test)")

        # Send ENQUIRE_LINK
        client.enquire_link()
        print("[✓] ENQUIRE_LINK sent")
        time.sleep(0.5)

        # Unbind
        client.unbind()
        print("[✓] UNBIND successful")

        client.disconnect()
        print("[✓] Disconnected")
        return True

    except Exception as e:
        print(f"[✗] Error: {e}")
        return False

def test_bind_receiver(host='127.0.0.1', port=2775):
    """Test BIND_RECEIVER operation"""
    print("\n" + "="*60)
    print("TEST 2: BIND_RECEIVER")
    print("="*60)

    try:
        client = smpplib.client.Client(host, port, timeout=10)
        print(f"[✓] Connected to {host}:{port}")

        client.connect()
        print("[✓] Socket connected")

        client.bind_receiver(system_id='test', password='test')
        print("[✓] BIND_RECEIVER successful")

        # Send ENQUIRE_LINK
        client.enquire_link()
        print("[✓] ENQUIRE_LINK sent")
        time.sleep(0.5)

        client.unbind()
        print("[✓] UNBIND successful")

        client.disconnect()
        print("[✓] Disconnected")
        return True

    except Exception as e:
        print(f"[✗] Error: {e}")
        return False

def test_bind_transceiver(host='127.0.0.1', port=2775):
    """Test BIND_TRANSCEIVER operation"""
    print("\n" + "="*60)
    print("TEST 3: BIND_TRANSCEIVER")
    print("="*60)

    try:
        client = smpplib.client.Client(host, port, timeout=10)
        print(f"[✓] Connected to {host}:{port}")

        client.connect()
        print("[✓] Socket connected")

        client.bind_transceiver(system_id='test', password='test')
        print("[✓] BIND_TRANSCEIVER successful")

        # Send ENQUIRE_LINK
        client.enquire_link()
        print("[✓] ENQUIRE_LINK sent")
        time.sleep(0.5)

        client.unbind()
        print("[✓] UNBIND successful")

        client.disconnect()
        print("[✓] Disconnected")
        return True

    except Exception as e:
        print(f"[✗] Error: {e}")
        return False

def test_authentication_failure(host='127.0.0.1', port=2775):
    """Test authentication failure with wrong credentials"""
    print("\n" + "="*60)
    print("TEST 4: AUTHENTICATION FAILURE (Wrong Password)")
    print("="*60)

    try:
        client = smpplib.client.Client(host, port, timeout=10)
        print(f"[✓] Connected to {host}:{port}")

        client.connect()
        print("[✓] Socket connected")

        # Try with wrong password
        try:
            client.bind_transmitter(system_id='test', password='wrong_password')
            print("[✗] Should have failed but succeeded")
            return False
        except smpplib.smpp.SMPPSessionError as e:
            print(f"[✓] Correctly rejected: {e}")
            client.disconnect()
            return True

    except Exception as e:
        print(f"[✗] Unexpected error: {e}")
        return False

def test_enquire_link_keepalive(host='127.0.0.1', port=2775):
    """Test ENQUIRE_LINK keep-alive functionality"""
    print("\n" + "="*60)
    print("TEST 5: ENQUIRE_LINK KEEP-ALIVE (Multiple Pings)")
    print("="*60)

    try:
        client = smpplib.client.Client(host, port, timeout=10)
        print(f"[✓] Connected to {host}:{port}")

        client.connect()
        print("[✓] Socket connected")

        client.bind_transmitter(system_id='test', password='test')
        print("[✓] BIND_TRANSMITTER successful")

        # Send multiple ENQUIRE_LINK PDUs
        for i in range(5):
            client.enquire_link()
            print(f"[✓] ENQUIRE_LINK #{i+1} sent")
            time.sleep(0.5)

        client.unbind()
        print("[✓] UNBIND successful")

        client.disconnect()
        print("[✓] Disconnected")
        return True

    except Exception as e:
        print(f"[✗] Error: {e}")
        return False

def test_double_bind(host='127.0.0.1', port=2775):
    """Test that second BIND on same connection fails"""
    print("\n" + "="*60)
    print("TEST 6: DOUBLE BIND (Should Fail)")
    print("="*60)

    try:
        client = smpplib.client.Client(host, port, timeout=10)
        print(f"[✓] Connected to {host}:{port}")

        client.connect()
        print("[✓] Socket connected")

        client.bind_transmitter(system_id='test', password='test')
        print("[✓] First BIND_TRANSMITTER successful")

        # Try to bind again
        try:
            client.bind_transmitter(system_id='test', password='test')
            print("[✗] Should have failed but succeeded")
            client.disconnect()
            return False
        except smpplib.smpp.SMPPSessionError as e:
            print(f"[✓] Correctly rejected: {e}")
            client.disconnect()
            return True

    except Exception as e:
        print(f"[✗] Unexpected error: {e}")
        return False

def main():
    """Run all tests"""
    if len(sys.argv) > 1:
        host = sys.argv[1]
    else:
        host = '127.0.0.1'

    if len(sys.argv) > 2:
        port = int(sys.argv[2])
    else:
        port = 2775

    print("\n" + "="*60)
    print(f"SMPP Client Test Suite")
    print(f"Target: {host}:{port}")
    print("="*60)

    results = []

    results.append(("BIND_TRANSMITTER", test_bind_transmitter(host, port)))
    time.sleep(1)

    results.append(("BIND_RECEIVER", test_bind_receiver(host, port)))
    time.sleep(1)

    results.append(("BIND_TRANSCEIVER", test_bind_transceiver(host, port)))
    time.sleep(1)

    results.append(("AUTH_FAILURE", test_authentication_failure(host, port)))
    time.sleep(1)

    results.append(("ENQUIRE_LINK_KEEPALIVE", test_enquire_link_keepalive(host, port)))
    time.sleep(1)

    results.append(("DOUBLE_BIND", test_double_bind(host, port)))

    # Summary
    print("\n" + "="*60)
    print("TEST SUMMARY")
    print("="*60)

    passed = sum(1 for _, result in results if result)
    total = len(results)

    for test_name, result in results:
        status = "✓ PASS" if result else "✗ FAIL"
        print(f"{test_name:25} {status}")

    print("="*60)
    print(f"Result: {passed}/{total} tests passed")
    print("="*60)

    return 0 if passed == total else 1

if __name__ == '__main__':
    sys.exit(main())
