"""Local bridge network configuration.

This file holds the bridge's LAN address. It is **gitignored** so your
personal home-LAN IP never enters the repository. After cloning, edit
``BRIDGE_HOST`` below to match the IP your bridge gets from DHCP.

The committed (placeholder) version uses an RFC 5737 documentation-range
IP (``192.0.2.48``) which is guaranteed not to route to any real device.

Used by:
- ``set_power.py`` (repo root)
- ``skills/api-validation/validate_api.py``
- ``skills/api-validation/test_inverter_endpoints.py``
- ``skills/log-analysis/*.py``
- ``skills/strategy-comparison/compare_strategies.py``

Subdirectory scripts add the repo root to ``sys.path`` and then import
``BRIDGE_BASE_URL`` from this module.
"""

# Replace with the IP your bridge gets on your home LAN.
BRIDGE_HOST = "192.0.2.48"  # RFC 5737 documentation placeholder
BRIDGE_PORT = 8080
BRIDGE_BASE_URL = f"http://{BRIDGE_HOST}:{BRIDGE_PORT}"
