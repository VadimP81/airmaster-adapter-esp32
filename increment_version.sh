#!/bin/bash

VERSION_FILE="main/version.h"
BUILD_NUM_FILE=".build_number"

# Read or initialize build number
if [ -f "$BUILD_NUM_FILE" ]; then
    BUILD_NUM=$(cat "$BUILD_NUM_FILE")
else
    BUILD_NUM=0
fi

# Increment build number
BUILD_NUM=$((BUILD_NUM + 1))
echo $BUILD_NUM > "$BUILD_NUM_FILE"

# Get git info if available
GIT_HASH=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
BUILD_DATE=$(date +"%Y-%m-%d %H:%M:%S")

# Generate version header
cat > "$VERSION_FILE" << EOF
// Auto-generated version file - do not edit manually
#pragma once

#define VERSION_MAJOR 1
#define VERSION_MINOR 1
#define VERSION_BUILD ${BUILD_NUM}
#define VERSION_STRING "1.1.${BUILD_NUM}"
#define BUILD_DATE "${BUILD_DATE}"
#define GIT_HASH "${GIT_HASH}"
EOF

echo "Version updated to 1.1.${BUILD_NUM} (build #${BUILD_NUM})"
