#!/bin/bash

VERSION_FILE="main/version.h"
VERSION_TXT="VERSION.txt"

# Read version from VERSION.txt
if [ ! -f "$VERSION_TXT" ]; then
    echo "ERROR: VERSION.txt not found"
    exit 1
fi

VERSION=$(cat "$VERSION_TXT" | tr -d '\n' | xargs)

# Parse version into MAJOR.MINOR.BUILD
IFS='.' read -r MAJOR MINOR BUILD <<< "$VERSION"

# Increment BUILD number
BUILD=$((BUILD + 1))

# Write back to VERSION.txt
NEW_VERSION="$MAJOR.$MINOR.$BUILD"
echo "$NEW_VERSION" > "$VERSION_TXT"

# Get git info if available
GIT_HASH=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
BUILD_DATE=$(date +"%Y-%m-%d %H:%M:%S")

# Generate version header
cat > "$VERSION_FILE" << EOF
// Generated from VERSION.txt - edit VERSION.txt to change version
#pragma once

#define VERSION_MAJOR $MAJOR
#define VERSION_MINOR $MINOR
#define VERSION_BUILD $BUILD
#define VERSION_STRING "$NEW_VERSION"
#define BUILD_DATE "$BUILD_DATE"
#define GIT_HASH "$GIT_HASH"
EOF

echo "Version incremented to $NEW_VERSION"
