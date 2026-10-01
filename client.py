# File Name: client.py
# Author: Elio Decolli (eliodecolli@gmail.com)
# Last Modified: 20/09/2026
# Purpose: Exercises tracker registration and peer discovery.

"""Exercise the tracker started by main.cpp: python3 client.py --help.

Requests are [uint8 type][uint32 payload length][payload]. Responses are
payloads only (no packet header). The C++ serializer uses native byte order;
this client assumes a little-endian tracker with one-byte bools.

The scenario leaves registrations in memory: delisting is not implemented.
It tests discovery, not peer-to-peer connectivity; advertised ports are the
TCP source ports observed by the tracker, not listening service ports.
"""

import argparse
from contextlib import ExitStack
import socket
import struct
import sys
import uuid


TRACKER_REGISTER = 0
TRACKER_FETCH_PEERS = 3
UINT32 = struct.Struct("<I")
MAX_STRING_BYTES = 1024 * 1024
MAX_ITEMS = 10000


class ScenarioError(Exception):
    pass


def require(condition, message):
    if not condition:
        raise ScenarioError(message)


def encode_string(value):
    encoded = value.encode("utf-8")
    return UINT32.pack(len(encoded)) + encoded


def receive_exact(sock, size):
    data = bytearray()
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise ScenarioError(
                f"Tracker closed the connection: expected {size} bytes, "
                f"received {len(data)}. Check the tracker logs for a crash."
            )
        data.extend(chunk)
    return bytes(data)


def receive_uint32(sock):
    return UINT32.unpack(receive_exact(sock, UINT32.size))[0]


def receive_string(sock):
    size = receive_uint32(sock)
    require(size <= MAX_STRING_BYTES, f"Invalid string length: {size}")
    return receive_exact(sock, size).decode("utf-8")


def receive_count(sock):
    count = receive_uint32(sock)
    require(count <= MAX_ITEMS, f"Invalid item count: {count}")
    return count


def send_request(sock, packet_type, payload):
    # One outstanding request per connection. The current server does not
    # reassemble TCP frames, so keep requests small and send each in one call.
    # TCP can still fragment them; fixing that requires server-side buffering.
    sock.sendall(struct.pack("<BI", packet_type, len(payload)) + payload)


def register(sock, peer_id, attributes):
    payload = encode_string(peer_id) + UINT32.pack(len(attributes))
    for name, value in attributes:
        payload += encode_string(name) + encode_string(value)
    send_request(sock, TRACKER_REGISTER, payload)
    status = receive_exact(sock, 1)
    require(status in (b"\x00", b"\x01"), f"Invalid registration status: {status!r}")
    ok = status == b"\x01"
    return ok, "" if ok else receive_string(sock)


def fetch_peers(sock, peer_id):
    send_request(sock, TRACKER_FETCH_PEERS, encode_string(peer_id))
    peers = {}
    for _ in range(receive_count(sock)):
        found_id = receive_string(sock)
        ip = receive_string(sock)
        port = receive_uint32(sock)
        attributes = [
            (receive_string(sock), receive_string(sock))
            for _ in range(receive_count(sock))
        ]
        require(found_id not in peers, f"Duplicate peer in discovery: {found_id}")
        peers[found_id] = {"ip": ip, "port": port, "attributes": attributes}
    return peers


def check_discovery(sock, peer_id, expected, scenario_ids):
    peers = fetch_peers(sock, peer_id)
    require(peer_id not in peers, f"Discovery included the requesting peer {peer_id}")
    actual_ids = set(peers) & scenario_ids
    require(
        actual_ids == set(expected),
        f"Discovery mismatch: expected {sorted(expected)}, got {sorted(actual_ids)}",
    )
    for found_id, metadata in expected.items():
        require(
            peers[found_id] == metadata,
            f"Metadata mismatch for {found_id}: expected {metadata}, got {peers[found_id]}",
        )
    print(f"  PASS: discovered {len(expected)} scenario peers; self excluded "
          f"({len(peers)} total peers)", flush=True)


def run_scenario(host, port, timeout):
    run_id = uuid.uuid4().hex
    identities = [f"{run_id}-{name}" for name in ("laptop", "desktop", "phone")]
    attributes = [
        [("name", "Alice's laptop"), ("role", "seeder"), ("file", "holiday-photos.zip")],
        [("name", "Bob's desktop"), ("role", "leecher"), ("location", "Montréal")],
        [],  # A peer without optional metadata is valid too.
    ]
    scenario_ids = set(identities)
    registered = {}
    print(f"Tracker {host}:{port}; scenario {run_id}", flush=True)
    with ExitStack() as stack:
        sockets = []
        for peer_id, peer_attributes in zip(identities, attributes):
            sock = stack.enter_context(socket.create_connection((host, port), timeout))
            sockets.append(sock)
            print(f"Registering {peer_id}...", flush=True)
            ok, message = register(sock, peer_id, peer_attributes)
            require(ok, f"Fresh registration rejected: {message}")
            local_ip, local_port = sock.getsockname()[:2]
            registered[peer_id] = {
                "ip": local_ip,
                "port": local_port,
                "attributes": peer_attributes,
            }
            # With only the first peer present this also checks an empty list
            # on a fresh tracker. Unrelated peers from prior runs are allowed.
            check_discovery(sock, peer_id,
                            {key: value for key, value in registered.items() if key != peer_id},
                            scenario_ids)

        print("Checking discovery from all three connected peers...", flush=True)
        for sock, peer_id in zip(sockets, identities):
            check_discovery(sock, peer_id,
                            {key: value for key, value in registered.items() if key != peer_id},
                            scenario_ids)

        print("Trying the laptop's UUID from a different connection...", flush=True)
        with socket.create_connection((host, port), timeout) as duplicate:
            ok, message = register(duplicate, identities[0], [("role", "replacement")])
            require(not ok, "Duplicate registration was accepted")
            require(bool(message), "Duplicate rejection did not include an explanation")
            print(f"  PASS: duplicate rejected: {message}", flush=True)

        # A rejected duplicate must not replace the original endpoint or attributes.
        check_discovery(sockets[1], identities[1],
                        {key: value for key, value in registered.items() if key != identities[1]},
                        scenario_ids)
    print("PASS: registration, discovery, metadata, self-exclusion, and duplicate rejection")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--timeout", type=float, default=5.0,
                        help="socket operation timeout in seconds (default: 5)")
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error("--port must be between 1 and 65535")
    if not 0 < args.timeout < float("inf"):
        parser.error("--timeout must be finite and positive")
    try:
        run_scenario(args.host, args.port, args.timeout)
    except (OSError, ScenarioError, UnicodeError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        print("Ensure the tracker is running and inspect its logs. The current C++ "
              "receive path copies into an empty vector, and the peer-table "
              "registration condition is reversed.", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
