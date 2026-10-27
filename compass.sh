#!/bin/bash
# Compass — compass-ng developer toolkit.
# This root-level entry point forwards to the real script so compass is
# discoverable from the repo root without knowing the project layout.
exec "$(dirname "${BASH_SOURCE[0]}")/projects/compass.legacy/compass.sh" "$@"
