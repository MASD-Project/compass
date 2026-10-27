#!/bin/bash

# Wrapper for Compass, the ORE Studio orientation tool. It bootstraps a
# Python virtual environment, then hands every argument to src/compass.py.
# Run './compass.sh --help' for the full, always-current command set
# (index, search, where/status, fleet, list, show, add, goto, ...).

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV_PATH="$SCRIPT_DIR/venv"

# On Windows (Git Bash / MSYS2) the venv layout uses Scripts/ not bin/.
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
    VENV_BIN="$VENV_PATH/Scripts"
else
    VENV_BIN="$VENV_PATH/bin"
fi

# --- Optional leading flag: --no-deps ---
# Bootstrap the venv but skip the pip install. The only dependency
# (pystache) is needed solely by the `add`/scaffold command; the stdlib-only
# commands (env, where, status, search, list, show, ...) never import it. With
# --no-deps a fresh checkout bootstraps with just `python3 -m venv` — no
# network — which is what CI uses to run `env init`. Must be the first arg.
NO_DEPS=0
if [ "$1" = "--no-deps" ]; then
    NO_DEPS=1
    shift
fi

# --- Auto-bootstrap Virtual Environment ---
if [ ! -d "$VENV_PATH" ]; then
    echo "🌱 Virtual environment not found. Creating one at $VENV_PATH..."

    if ! python3 -m venv "$VENV_PATH"; then
        echo "❌ Error: Failed to create virtual environment."
        echo "   You may need to install the venv package: sudo apt install python3-venv"
        exit 1
    fi
fi

# Resolve the venv interpreter by probe, not by name. A POSIX venv holds
# `python3`, but the venv module always writes `python.exe` on Windows,
# whatever name created it. The first candidate that exists wins, so this
# is a no-op on Linux and macOS. Falls back to the POSIX name so a venv
# that holds no interpreter fails the way it always did.
resolve_venv_python() {
    for candidate in "$VENV_BIN/python3" "$VENV_BIN/python3.exe" \
                     "$VENV_BIN/python" "$VENV_BIN/python.exe"; do
        if [ -f "$candidate" ]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done
    printf '%s\n' "$VENV_BIN/python3"
}

VENV_PYTHON="$(resolve_venv_python)"

# Install/refresh requirements if they exist (future-proofing for NLP
# packages), unless --no-deps requested a stdlib-only bootstrap. Re-syncs
# whenever requirements.txt changes, not just on first venv creation, so a
# stale venv from before a new dependency was added self-heals.
REQUIREMENTS_FILE="$SCRIPT_DIR/requirements.txt"
STAMP_PATH="$VENV_PATH/.requirements.stamp"
if [ "$NO_DEPS" -eq 1 ]; then
    :
elif [ -f "$REQUIREMENTS_FILE" ] && { [ ! -f "$STAMP_PATH" ] || [ "$REQUIREMENTS_FILE" -nt "$STAMP_PATH" ]; }; then
    echo "📦 Installing dependencies from requirements.txt..."
    # Drive pip as an interpreter module, never through the `pip` shim: on
    # Windows, pip.exe cannot overwrite its own running binary and fails
    # with "ERROR: To modify pip, please run the following command:
    # ... -m pip install --upgrade pip" -- invoking through the
    # interpreter module sidesteps the self-lock.
    "$VENV_PYTHON" -m pip install --upgrade pip -q
    "$VENV_PYTHON" -m pip install -r "$REQUIREMENTS_FILE" -q
    cp "$REQUIREMENTS_FILE" "$STAMP_PATH"
fi

source "$VENV_BIN/activate"

# Change to the repository root so that relative paths (e.g. --parent-dir
# doc/agile/...) resolve correctly. Use git rev-parse for robustness;
# fall back to SCRIPT_DIR if not inside a git repository.
REPO_ROOT=$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel 2>/dev/null || echo "$SCRIPT_DIR")
cd "$REPO_ROOT"

"$VENV_PYTHON" "$SCRIPT_DIR/src/compass.py" "$@"
