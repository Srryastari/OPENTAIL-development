"""OpenTail CORE bench: authenticated FX and graceful temporary-effect release.

Requires bleak and OPENTAIL_OWNER_KEY (32 hex chars). Keep PC connected to
OpenTail-CORE-Alpha Wi-Fi for the HTTP status assertion.
"""
import asyncio
import hashlib
import hmac
import json
import os
import urllib.request

from bleak import BleakClient, BleakScanner

BASE = "55a56022-4915-7a92-cf44-b8b7102e"
COMMAND = BASE + "af23"
REPLY = BASE + "af24"
CHALLENGE = BASE + "af28"
FX = bytes([1, 5, 0, 0, 1, 1, 255, 0, 0, 0, 0, 0, 25, 128, 128, 0, 30])
RELEASE = bytes([1, 9])


def http_show():
    with urllib.request.urlopen("http://192.168.4.1/api/v1/show", timeout=5) as response:
        return json.load(response)


async def command(client, payload, expected):
    await client.write_gatt_char(COMMAND, payload, response=True)
    reply = bytes(await client.read_gatt_char(REPLY))
    if reply != expected:
        raise RuntimeError(f"Expected {expected!r}, got {reply!r}")
    print(reply.decode())


async def main():
    key = bytes.fromhex(os.environ["OPENTAIL_OWNER_KEY"])
    if len(key) != 16:
        raise ValueError("OPENTAIL_OWNER_KEY must be 32 hex characters")
    device = await BleakScanner.find_device_by_name("OpenTail CORE", timeout=15)
    if device is None:
        raise RuntimeError("OpenTail CORE not found")

    async with BleakClient(device, timeout=20) as client:
        challenge = bytes(await client.read_gatt_char(CHALLENGE))
        mac = hmac.new(key, b"OpenTail-BLE-v2" + challenge, hashlib.sha256).digest()[:16]
        await command(client, bytes([1, 4]) + mac, b"OK AUTH")
        try:
            await command(client, FX, b"OK FX")
            before = await asyncio.to_thread(http_show)
            print("BEFORE:", before["active"], before["effect"])
        finally:
            # Release while the GATT link still exists: don't wait for Windows
            # or the controller's GAP supervision/disconnect notification.
            await command(client, RELEASE, b"OK RELEASED")
            after = await asyncio.to_thread(http_show)
            print("AFTER RELEASE:", after["active"], after["effect"], after["remaining_ms"])
            if after["active"]:
                raise AssertionError("Temporary effect remained active after acknowledged release")

    print("DISCONNECTED; GRACEFUL RELEASE PASS")


if __name__ == "__main__":
    asyncio.run(main())
