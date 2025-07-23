#!/bin/sh
# Download and extract mbedtls and curl sources with checksum validation

set -e

# Version information
MBEDTLS_VERSION=3.6.4
MBEDTLS_DIR=mbedtls
MBEDTLS_TAR=v${MBEDTLS_VERSION}.tar.gz
MBEDTLS_URL=https://github.com/Mbed-TLS/mbedtls/archive/refs/tags/${MBEDTLS_TAR}
MBEDTLS_SHA256=a1e01f6094bb744ead8caaa92d7c7102b7137a813f509b49afaebc0b8e51899c

CURL_VERSION=8.15.0
CURL_DIR=curl
CURL_TAR=curl-${CURL_VERSION}.tar.gz
CURL_URL=https://curl.se/download/${CURL_TAR}
CURL_SHA256=d85cfc79dc505ff800cb1d321a320183035011fa08cb301356425d86be8fc53c

# Download mbedtls if not already present
if [ ! -d "${MBEDTLS_DIR}" ]; then
    echo "Downloading mbedtls ${MBEDTLS_VERSION}..."
    curl -L "${MBEDTLS_URL}" -o "${MBEDTLS_TAR}"
    
    echo "Validating mbedtls checksum..."
    echo "${MBEDTLS_SHA256}  ${MBEDTLS_TAR}" | sha256sum -c
    
    echo "Extracting mbedtls..."
    tar xzf "${MBEDTLS_TAR}"
    mv "mbedtls-${MBEDTLS_VERSION}" "${MBEDTLS_DIR}"
    rm "${MBEDTLS_TAR}"
fi

# Download curl if not already present
if [ ! -d "${CURL_DIR}" ]; then
    echo "Downloading curl ${CURL_VERSION}..."
    curl -L "${CURL_URL}" -o "${CURL_TAR}"
    
    echo "Validating curl checksum..."
    echo "${CURL_SHA256}  ${CURL_TAR}" | sha256sum -c
    
    echo "Extracting curl..."
    tar xzf "${CURL_TAR}"
    mv "curl-${CURL_VERSION}" "${CURL_DIR}"
    rm "${CURL_TAR}"
    
    echo "Patching curl for cosmo compatibility..."
    patch -p0 < curl-setup.patch
fi

echo "All sources downloaded and extracted successfully."