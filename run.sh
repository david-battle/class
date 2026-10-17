#!/usr/bin/env bash
set -euo pipefail
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$REPO"

make -s class

# Launch in its own session so a shell/tool timeout can't kill the game.
setsid ./class >class.log 2>&1 </dev/null &
echo "class started (pid $!)"
