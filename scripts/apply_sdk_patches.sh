#!/bin/zsh
# Apply this project's fixes to a fresh ReXGlue SDK checkout (v0.10.0, c94f5eb).
REPO=${0:A:h:h}
SDK=${REXSDK_DIR:-$REPO/../tools/rexglue-sdk}
for p in "$REPO"/patches/*.patch; do
  if git -C "$SDK" apply --reverse --check "$p" 2>/dev/null; then
    echo "already applied: ${p:t}"
  else
    git -C "$SDK" apply "$p" && echo "applied: ${p:t}" || { echo "FAILED: ${p:t}"; exit 1; }
  fi
done
