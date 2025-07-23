#!/bin/sh
# Publish minicoder binaries to itch.io using butler
# Requires: butler CLI tool and BUTLER_API_KEY environment variable

set -e

# Configuration
ITCH_USER="andrewchambers"
ITCH_GAME="minicoder"

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

echo "Preparing to publish minicoder to itch.io..."

# Check for clean git checkout
if [ -n "$(git status --porcelain)" ]; then
    echo "Error: Git repository has uncommitted changes"
    echo "Please commit or stash your changes before publishing"
    exit 1
fi

# Get current version from git tag
CURRENT_TAG="$(git describe --exact-match --tags 2>/dev/null || true)"
if [ -z "$CURRENT_TAG" ]; then
    echo "Error: No git tag found at current commit"
    echo "Please create a version tag (e.g., git tag v0.1.0) before publishing"
    exit 1
fi

# Validate tag format (should start with 'v')
if ! echo "$CURRENT_TAG" | grep -q '^v[0-9]'; then
    echo "Error: Tag '$CURRENT_TAG' doesn't look like a version tag"
    echo "Version tags should start with 'v' followed by a version number (e.g., v0.1.0)"
    exit 1
fi

# Extract version without 'v' prefix
VERSION="${CURRENT_TAG#v}"

echo "Publishing version: $VERSION"

# Check if butler is installed
if ! command -v butler >/dev/null 2>&1; then
    echo "Error: butler is not installed. Please install it from:"
    echo "https://itch.io/docs/butler/"
    exit 1
fi

# Check if API key is set
if [ -z "$BUTLER_API_KEY" ]; then
    echo "Error: BUTLER_API_KEY environment variable is not set"
    echo "Get your API key from: https://itch.io/user/settings/api-keys"
    exit 1
fi

# Clean any existing build
echo "Cleaning previous builds..."
rm -rf cosmo-bin

# Build with cosmopolitan
echo "Building cosmopolitan binaries for version $VERSION..."
VERSION="$VERSION" "$SCRIPT_DIR/build-with-cosmo.sh"

# Verify build succeeded
if [ ! -d "$PROJECT_ROOT/cosmo-bin" ]; then
    echo "Error: Build failed - cosmo-bin directory not found"
    exit 1
fi

# Push each platform binary
echo "Publishing platform binaries..."

# Portable binary (.com)
echo "Pushing portable binary..."
butler push "cosmo-bin/minicoder.com" "${ITCH_USER}/${ITCH_GAME}:portable" --userversion "$VERSION"

# Linux binaries
echo "Pushing Linux AMD64 binary..."
butler push "cosmo-bin/minicoder-linux-amd64" "${ITCH_USER}/${ITCH_GAME}:linux-amd64" --userversion "$VERSION"

echo "Pushing Linux ARM64 binary..."
butler push "cosmo-bin/minicoder-linux-arm64" "${ITCH_USER}/${ITCH_GAME}:linux-arm64" --userversion "$VERSION"

# BSD binaries
echo "Pushing FreeBSD AMD64 binary..."
butler push "cosmo-bin/minicoder-freebsd-amd64" "${ITCH_USER}/${ITCH_GAME}:freebsd-amd64" --userversion "$VERSION"

echo "Pushing OpenBSD AMD64 binary..."
butler push "cosmo-bin/minicoder-openbsd-amd64" "${ITCH_USER}/${ITCH_GAME}:openbsd-amd64" --userversion "$VERSION"

echo "Pushing NetBSD AMD64 binary..."
butler push "cosmo-bin/minicoder-netbsd-amd64" "${ITCH_USER}/${ITCH_GAME}:netbsd-amd64" --userversion "$VERSION"

# macOS binaries
echo "Pushing macOS AMD64 binary..."
butler push "cosmo-bin/minicoder-darwin-amd64" "${ITCH_USER}/${ITCH_GAME}:macos-amd64" --userversion "$VERSION"

echo "Pushing macOS ARM64 binary..."
butler push "cosmo-bin/minicoder-darwin-arm64" "${ITCH_USER}/${ITCH_GAME}:macos-arm64" --userversion "$VERSION"

echo "All binaries published successfully!"
echo "View your game at: https://${ITCH_USER}.itch.io/${ITCH_GAME}"