#!/bin/bash
# Sets up the environment for building and running the project.
# This script should be sourced, not executed.
# Usage: source tools/scripts/env.sh <extra commands>
# Example: source tools/scripts/env.sh source ~/esp/esp-idf/export.sh

# Exit if not sourced
(return 0 2>/dev/null) || { echo "This script must be sourced, not executed."; exit 1; }

# Set the project root directory
export PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
echo "Project root set to: $PROJECT_ROOT"
export PATH="$PROJECT_ROOT/tools/scripts:$PATH"

# make sure python3 is available
if ! command -v python3 &> /dev/null; then
    echo "python3 could not be found, please install it."
    return 1
fi

# Make *.py as executable
find "$PROJECT_ROOT/tools/scripts" -name "*.py" -exec chmod +x {} \;

# Execute additional command line arguments if provided like source env.sh source ~/esp/esp-idf/export.sh
if [ "$#" -gt 0 ]; then
    "$@"
fi
