"""Local bridge network configuration — auto-discovery.

Discovers the bridge on the local network using the DHCP hostname
``mv-bridge`` (as configured in firmware settings.cpp).  Falls back to
ARP-table MAC matching when hostname resolution is unavailable.

Used by:
- ``set_power.py`` (repo root)
- ``skills/api-validation/validate_api.py``
- ``skills/api-validation/test_inverter_endpoints.py``
- ``skills/log-analysis/*.py``
- ``skills/strategy-comparison/compare_strategies.py``

Subdirectory scripts add the repo root to ``sys.path`` and then import
``BRIDGE_BASE_URL`` from this module.
"""

import re
import socket
import subprocess

# Must match firmware settings: DHCP_HOSTNAME and ETH_MAC.
_HOSTNAME = "mv-bridge"
_ETH_MAC = "02-a1-82-32-10-42"

BRIDGE_PORT = 8080


def _resolve_hostname() -> str | None:
    """Try DNS/mDNS resolution of the bridge hostname."""
    for name in (_HOSTNAME, f"{_HOSTNAME}.local"):
        try:
            return socket.gethostbyname(name)
        except socket.gaierror:
            continue
    return None


def _ping_broadcast() -> None:
    """Send a broadcast ping to populate the ARP table with LAN neighbors."""
    try:
        # Get local subnet broadcast address from default interface
        result = subprocess.run(
            ["powershell", "-NoProfile", "-Command",
             "(Get-NetIPAddress -AddressFamily IPv4 "
             "| Where-Object { $_.PrefixOrigin -ne 'WellKnown' -and $_.IPAddress -ne '127.0.0.1' } "
             "| Select-Object -First 1).IPAddress"],
            capture_output=True, text=True, timeout=5,
        )
        local_ip = result.stdout.strip()
        if not local_ip:
            return
        # Derive broadcast from local IP assuming /24
        parts = local_ip.split(".")
        if len(parts) == 4:
            broadcast = f"{parts[0]}.{parts[1]}.{parts[2]}.255"
            subprocess.run(
                ["ping", "-n", "1", "-w", "500", broadcast],
                capture_output=True, timeout=3,
            )
    except (subprocess.SubprocessError, OSError):
        pass


def _find_by_mac() -> str | None:
    """Populate ARP via broadcast ping, then search for the bridge MAC."""
    _ping_broadcast()
    try:
        result = subprocess.run(
            ["arp", "-a"],
            capture_output=True, text=True, timeout=5,
        )
        for line in result.stdout.splitlines():
            if _ETH_MAC.lower() in line.lower():
                match = re.search(r"(\d+\.\d+\.\d+\.\d+)", line)
                if match:
                    return match.group(1)
    except (subprocess.SubprocessError, OSError):
        pass
    return None


def discover_bridge() -> str:
    """Return the bridge IP, trying hostname resolution then ARP lookup."""
    ip = _resolve_hostname() or _find_by_mac()
    if ip is None:
        raise RuntimeError(
            f"Cannot discover bridge: hostname '{_HOSTNAME}' unresolvable "
            f"and MAC '{_ETH_MAC}' not found in ARP table. "
            f"Ensure the bridge is powered on and on the same LAN."
        )
    return ip


BRIDGE_HOST = discover_bridge()
BRIDGE_BASE_URL = f"http://{BRIDGE_HOST}:{BRIDGE_PORT}"
