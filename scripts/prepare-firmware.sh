#!/usr/bin/env bash
set -euo pipefail
for target in rover gateway; do
  base="firmware/$target/include"
  if [[ -f "$base/local_config.h.example" && ! -f "$base/local_config.h" ]]; then
    cp "$base/local_config.h.example" "$base/local_config.h"
    echo "Created $base/local_config.h"
  fi
done
if [[ ! -f firmware/gateway/include/secrets.h ]]; then
  cp firmware/gateway/include/secrets.example.h firmware/gateway/include/secrets.h
  echo "Created firmware/gateway/include/secrets.h"
fi
echo "Edit local_config.h and secrets.h before flashing. These files are gitignored."
