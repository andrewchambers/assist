#!/bin/sh
# Build script for compiling minicoder with Cosmopolitan libc

set -e

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "Building minicoder with Cosmopolitan libc"

# Step 1: Build cosmo-curl
echo "Building cosmo-curl..."
cd "$PROJECT_ROOT/lib/cosmo-curl"
# Download dependencies if needed
./download.sh
# Build everything
make all 

# Step 2: Build minicoder with cosmocc
echo "Building minicoder..."
cd "$PROJECT_ROOT"

# Build with cosmocc
make minicoder \
  CC=cosmocc \
  CFLAGS="-O1 -Ilib/cJSON -I$PROJECT_ROOT/lib/cosmo-curl/curl/include" \
  LDFLAGS="-L$PROJECT_ROOT/lib/cosmo-curl" \
  LDLIBS="-lpthread $PROJECT_ROOT/lib/cosmo-curl/libcurl.a $PROJECT_ROOT/lib/cosmo-curl/mbedtls.a" \
  VERSION="${VERSION:-dev}"

# Step 3: Generate platform-specific binaries using assimilate
echo "Generating platform-specific binaries..."

mkdir -p cosmo-bin

# Move the binary to cosmo-bin as minicoder.com
cp minicoder cosmo-bin/minicoder.com
echo "Build complete: $PROJECT_ROOT/cosmo-bin/minicoder.com"

# Generate ELF binaries for Linux
assimilate -e -x -o cosmo-bin/minicoder-linux-amd64 cosmo-bin/minicoder.com
assimilate -e -a -o cosmo-bin/minicoder-linux-arm64 cosmo-bin/minicoder.com

# Generate BSD binaries with -b flag to preserve FreeBSD compatibility
assimilate -e -b -x -o cosmo-bin/minicoder-freebsd-amd64 cosmo-bin/minicoder.com
assimilate -e -b -x -o cosmo-bin/minicoder-openbsd-amd64 cosmo-bin/minicoder.com
assimilate -e -b -x -o cosmo-bin/minicoder-netbsd-amd64 cosmo-bin/minicoder.com

# Generate Mach-O binaries (macOS)
assimilate -m -x -o cosmo-bin/minicoder-darwin-amd64 cosmo-bin/minicoder.com
# For Apple Silicon, use ELF format as per Cosmopolitan convention
assimilate -e -a -o cosmo-bin/minicoder-darwin-arm64 cosmo-bin/minicoder.com

# Make all binaries executable
chmod +x cosmo-bin/minicoder-*

echo "Platform-specific binaries generated in: $PROJECT_ROOT/cosmo-bin/"

# Step 4: Build man pages
echo "Building man pages..."
cd "$PROJECT_ROOT"
make man

# Copy man pages to cosmo-bin
mkdir -p cosmo-bin/man
cp doc/minicoder.1 cosmo-bin/man/
cp doc/minicoder-model-config.5 cosmo-bin/man/

echo "Man pages generated in: $PROJECT_ROOT/cosmo-bin/man/"
