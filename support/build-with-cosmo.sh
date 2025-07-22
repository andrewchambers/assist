#!/bin/sh
# Build script for compiling assist with Cosmopolitan libc

set -e

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "Building assist with Cosmopolitan libc"

# Step 1: Build cosmo-curl
echo "Building cosmo-curl..."
cd "$SCRIPT_DIR/cosmo-curl"
make -j$(nproc) all 

# Step 2: Build assist with cosmocc
echo "Building assist..."
cd "$PROJECT_ROOT"

# Build with cosmocc
make assist \
  CC=cosmocc \
  CFLAGS="-O1 -Ilib/tgc -Ilib/cJSON -I$SCRIPT_DIR/cosmo-curl/curl/include" \
  LDFLAGS="-L$SCRIPT_DIR/cosmo-curl" \
  LDLIBS="-lpthread $SCRIPT_DIR/cosmo-curl/libcurl.a $SCRIPT_DIR/cosmo-curl/mbedtls.a"

# Step 3: Generate platform-specific binaries using assimilate
echo "Generating platform-specific binaries..."

mkdir -p cosmo-bin

# Move the binary to cosmo-bin as assist.com
cp assist cosmo-bin/assist.com
echo "Build complete: $PROJECT_ROOT/cosmo-bin/assist.com"

# Generate ELF binaries for Linux
assimilate -e -x -o cosmo-bin/assist-linux-amd64 cosmo-bin/assist.com
assimilate -e -a -o cosmo-bin/assist-linux-arm64 cosmo-bin/assist.com

# Generate BSD binaries with -b flag to preserve FreeBSD compatibility
assimilate -e -b -x -o cosmo-bin/assist-freebsd-amd64 cosmo-bin/assist.com
assimilate -e -b -x -o cosmo-bin/assist-openbsd-amd64 cosmo-bin/assist.com
assimilate -e -b -x -o cosmo-bin/assist-netbsd-amd64 cosmo-bin/assist.com

# Generate Mach-O binaries (macOS)
assimilate -m -x -o cosmo-bin/assist-darwin-amd64 cosmo-bin/assist.com
# For Apple Silicon, use ELF format as per Cosmopolitan convention
assimilate -e -a -o cosmo-bin/assist-darwin-arm64 cosmo-bin/assist.com

# Make all binaries executable
chmod +x cosmo-bin/assist-*

echo "Platform-specific binaries generated in: $PROJECT_ROOT/cosmo-bin/"
