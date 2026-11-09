#!/usr/bin/env bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# 1. Load local environment configuration from .env.local if present
if [ -f "$SCRIPT_DIR/.env.local" ]; then
    while IFS='=' read -r key val || [ -n "$key" ]; do
        key="$(echo "$key" | xargs)"
        if [[ -n "$key" && ! "$key" =~ ^# ]]; then
            val="$(echo "$val" | xargs)"
            export "$key=$val"
        fi
    done < "$SCRIPT_DIR/.env.local"
fi

# 2. Invoke MeshX env script with the toolchain export script
# Redirect banner output to stderr so stdout remains clean JSON-RPC for MCP
if [ -n "$MESHX_EXPORT_SCRIPT" ]; then
    source "$REPO_ROOT/tools/scripts/env.sh" source "$MESHX_EXPORT_SCRIPT" >&2
else
    source "$REPO_ROOT/tools/scripts/env.sh" >&2
fi

# 3. Launch Python MCP Server using stdio transport
exec python3 "$SCRIPT_DIR/meshx_mcp_server.py"

