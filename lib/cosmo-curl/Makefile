# Combined Makefile for building mbedtls and libcurl with cosmocc
CC = cosmocc
AR = cosmoar
CFLAGS = -O2

# Version information
# To update checksums: download the file and run `sha256sum filename`
MBEDTLS_VERSION = 3.6.4
MBEDTLS_DIR = mbedtls
MBEDTLS_TAR = v$(MBEDTLS_VERSION).tar.gz
MBEDTLS_URL = https://github.com/Mbed-TLS/mbedtls/archive/refs/tags/$(MBEDTLS_TAR)
MBEDTLS_SHA256 = a1e01f6094bb744ead8caaa92d7c7102b7137a813f509b49afaebc0b8e51899c

CURL_VERSION = 8.15.0
CURL_DIR = curl
CURL_TAR = curl-$(CURL_VERSION).tar.gz
CURL_URL = https://curl.se/download/$(CURL_TAR)
CURL_SHA256 = d85cfc79dc505ff800cb1d321a320183035011fa08cb301356425d86be8fc53c

# MbedTLS specific flags
MBEDTLS_CFLAGS = $(CFLAGS) -Iinclude -I$(MBEDTLS_DIR)/include

# Curl specific flags  
CURL_CFLAGS = $(CFLAGS) -I. -I$(CURL_DIR)/include -I$(CURL_DIR)/lib -I$(MBEDTLS_DIR)/include -DBUILDING_LIBCURL -DHAVE_CONFIG_H

# All mbedtls library sources
MBEDTLS_SRCS = \
    $(MBEDTLS_DIR)/library/aes.c \
    $(MBEDTLS_DIR)/library/aesni.c \
    $(MBEDTLS_DIR)/library/aesce.c \
    $(MBEDTLS_DIR)/library/aria.c \
    $(MBEDTLS_DIR)/library/asn1parse.c \
    $(MBEDTLS_DIR)/library/asn1write.c \
    $(MBEDTLS_DIR)/library/base64.c \
    $(MBEDTLS_DIR)/library/bignum.c \
    $(MBEDTLS_DIR)/library/bignum_core.c \
    $(MBEDTLS_DIR)/library/bignum_mod.c \
    $(MBEDTLS_DIR)/library/bignum_mod_raw.c \
    $(MBEDTLS_DIR)/library/block_cipher.c \
    $(MBEDTLS_DIR)/library/camellia.c \
    $(MBEDTLS_DIR)/library/ccm.c \
    $(MBEDTLS_DIR)/library/chacha20.c \
    $(MBEDTLS_DIR)/library/chachapoly.c \
    $(MBEDTLS_DIR)/library/cipher.c \
    $(MBEDTLS_DIR)/library/cipher_wrap.c \
    $(MBEDTLS_DIR)/library/cmac.c \
    $(MBEDTLS_DIR)/library/constant_time.c \
    $(MBEDTLS_DIR)/library/ctr_drbg.c \
    $(MBEDTLS_DIR)/library/debug.c \
    $(MBEDTLS_DIR)/library/des.c \
    $(MBEDTLS_DIR)/library/dhm.c \
    $(MBEDTLS_DIR)/library/ecdh.c \
    $(MBEDTLS_DIR)/library/ecdsa.c \
    $(MBEDTLS_DIR)/library/ecjpake.c \
    $(MBEDTLS_DIR)/library/ecp.c \
    $(MBEDTLS_DIR)/library/ecp_curves.c \
    $(MBEDTLS_DIR)/library/ecp_curves_new.c \
    $(MBEDTLS_DIR)/library/entropy.c \
    $(MBEDTLS_DIR)/library/entropy_poll.c \
    $(MBEDTLS_DIR)/library/error.c \
    $(MBEDTLS_DIR)/library/gcm.c \
    $(MBEDTLS_DIR)/library/hkdf.c \
    $(MBEDTLS_DIR)/library/hmac_drbg.c \
    $(MBEDTLS_DIR)/library/lmots.c \
    $(MBEDTLS_DIR)/library/lms.c \
    $(MBEDTLS_DIR)/library/md.c \
    $(MBEDTLS_DIR)/library/md5.c \
    $(MBEDTLS_DIR)/library/memory_buffer_alloc.c \
    $(MBEDTLS_DIR)/library/mps_reader.c \
    $(MBEDTLS_DIR)/library/mps_trace.c \
    $(MBEDTLS_DIR)/library/net_sockets.c \
    $(MBEDTLS_DIR)/library/nist_kw.c \
    $(MBEDTLS_DIR)/library/oid.c \
    $(MBEDTLS_DIR)/library/padlock.c \
    $(MBEDTLS_DIR)/library/pem.c \
    $(MBEDTLS_DIR)/library/pk.c \
    $(MBEDTLS_DIR)/library/pk_ecc.c \
    $(MBEDTLS_DIR)/library/pk_wrap.c \
    $(MBEDTLS_DIR)/library/pkcs12.c \
    $(MBEDTLS_DIR)/library/pkcs5.c \
    $(MBEDTLS_DIR)/library/pkcs7.c \
    $(MBEDTLS_DIR)/library/pkparse.c \
    $(MBEDTLS_DIR)/library/pkwrite.c \
    $(MBEDTLS_DIR)/library/platform.c \
    $(MBEDTLS_DIR)/library/platform_util.c \
    $(MBEDTLS_DIR)/library/poly1305.c \
    $(MBEDTLS_DIR)/library/psa_crypto.c \
    $(MBEDTLS_DIR)/library/psa_crypto_aead.c \
    $(MBEDTLS_DIR)/library/psa_crypto_cipher.c \
    $(MBEDTLS_DIR)/library/psa_crypto_client.c \
    $(MBEDTLS_DIR)/library/psa_crypto_driver_wrappers_no_static.c \
    $(MBEDTLS_DIR)/library/psa_crypto_ecp.c \
    $(MBEDTLS_DIR)/library/psa_crypto_ffdh.c \
    $(MBEDTLS_DIR)/library/psa_crypto_hash.c \
    $(MBEDTLS_DIR)/library/psa_crypto_mac.c \
    $(MBEDTLS_DIR)/library/psa_crypto_pake.c \
    $(MBEDTLS_DIR)/library/psa_crypto_rsa.c \
    $(MBEDTLS_DIR)/library/psa_crypto_se.c \
    $(MBEDTLS_DIR)/library/psa_crypto_slot_management.c \
    $(MBEDTLS_DIR)/library/psa_crypto_storage.c \
    $(MBEDTLS_DIR)/library/psa_its_file.c \
    $(MBEDTLS_DIR)/library/psa_util.c \
    $(MBEDTLS_DIR)/library/ripemd160.c \
    $(MBEDTLS_DIR)/library/rsa.c \
    $(MBEDTLS_DIR)/library/rsa_alt_helpers.c \
    $(MBEDTLS_DIR)/library/sha1.c \
    $(MBEDTLS_DIR)/library/sha256.c \
    $(MBEDTLS_DIR)/library/sha3.c \
    $(MBEDTLS_DIR)/library/sha512.c \
    $(MBEDTLS_DIR)/library/ssl_cache.c \
    $(MBEDTLS_DIR)/library/ssl_ciphersuites.c \
    $(MBEDTLS_DIR)/library/ssl_client.c \
    $(MBEDTLS_DIR)/library/ssl_cookie.c \
    $(MBEDTLS_DIR)/library/ssl_debug_helpers_generated.c \
    $(MBEDTLS_DIR)/library/ssl_msg.c \
    $(MBEDTLS_DIR)/library/ssl_ticket.c \
    $(MBEDTLS_DIR)/library/ssl_tls.c \
    $(MBEDTLS_DIR)/library/ssl_tls12_client.c \
    $(MBEDTLS_DIR)/library/ssl_tls12_server.c \
    $(MBEDTLS_DIR)/library/ssl_tls13_client.c \
    $(MBEDTLS_DIR)/library/ssl_tls13_generic.c \
    $(MBEDTLS_DIR)/library/ssl_tls13_keys.c \
    $(MBEDTLS_DIR)/library/ssl_tls13_server.c \
    $(MBEDTLS_DIR)/library/threading.c \
    $(MBEDTLS_DIR)/library/timing.c \
    $(MBEDTLS_DIR)/library/version.c \
    $(MBEDTLS_DIR)/library/version_features.c \
    $(MBEDTLS_DIR)/library/x509.c \
    $(MBEDTLS_DIR)/library/x509_create.c \
    $(MBEDTLS_DIR)/library/x509_crl.c \
    $(MBEDTLS_DIR)/library/x509_crt.c \
    $(MBEDTLS_DIR)/library/x509_csr.c \
    $(MBEDTLS_DIR)/library/x509write.c \
    $(MBEDTLS_DIR)/library/x509write_crt.c \
    $(MBEDTLS_DIR)/library/x509write_csr.c

# Core libcurl sources
CURL_SRCS = \
    $(CURL_DIR)/lib/altsvc.c \
    $(CURL_DIR)/lib/amigaos.c \
    $(CURL_DIR)/lib/asyn-ares.c \
    $(CURL_DIR)/lib/asyn-base.c \
    $(CURL_DIR)/lib/asyn-thrdd.c \
    $(CURL_DIR)/lib/bufq.c \
    $(CURL_DIR)/lib/bufref.c \
    $(CURL_DIR)/lib/cf-h1-proxy.c \
    $(CURL_DIR)/lib/cf-h2-proxy.c \
    $(CURL_DIR)/lib/cf-haproxy.c \
    $(CURL_DIR)/lib/cf-https-connect.c \
    $(CURL_DIR)/lib/cf-socket.c \
    $(CURL_DIR)/lib/cfilters.c \
    $(CURL_DIR)/lib/conncache.c \
    $(CURL_DIR)/lib/connect.c \
    $(CURL_DIR)/lib/content_encoding.c \
    $(CURL_DIR)/lib/cookie.c \
    $(CURL_DIR)/lib/cshutdn.c \
    $(CURL_DIR)/lib/curl_addrinfo.c \
    $(CURL_DIR)/lib/curl_des.c \
    $(CURL_DIR)/lib/curl_endian.c \
    $(CURL_DIR)/lib/curl_fnmatch.c \
    $(CURL_DIR)/lib/curl_get_line.c \
    $(CURL_DIR)/lib/curl_gethostname.c \
    $(CURL_DIR)/lib/curl_gssapi.c \
    $(CURL_DIR)/lib/curl_memrchr.c \
    $(CURL_DIR)/lib/curl_ntlm_core.c \
    $(CURL_DIR)/lib/curl_range.c \
    $(CURL_DIR)/lib/curl_rtmp.c \
    $(CURL_DIR)/lib/curl_sasl.c \
    $(CURL_DIR)/lib/curl_sha512_256.c \
    $(CURL_DIR)/lib/curl_sspi.c \
    $(CURL_DIR)/lib/curl_threads.c \
    $(CURL_DIR)/lib/curl_trc.c \
    $(CURL_DIR)/lib/cw-out.c \
    $(CURL_DIR)/lib/cw-pause.c \
    $(CURL_DIR)/lib/dict.c \
    $(CURL_DIR)/lib/dllmain.c \
    $(CURL_DIR)/lib/doh.c \
    $(CURL_DIR)/lib/dynhds.c \
    $(CURL_DIR)/lib/easy.c \
    $(CURL_DIR)/lib/easygetopt.c \
    $(CURL_DIR)/lib/easyoptions.c \
    $(CURL_DIR)/lib/escape.c \
    $(CURL_DIR)/lib/fake_addrinfo.c \
    $(CURL_DIR)/lib/file.c \
    $(CURL_DIR)/lib/fileinfo.c \
    $(CURL_DIR)/lib/fopen.c \
    $(CURL_DIR)/lib/formdata.c \
    $(CURL_DIR)/lib/ftp.c \
    $(CURL_DIR)/lib/ftplistparser.c \
    $(CURL_DIR)/lib/getenv.c \
    $(CURL_DIR)/lib/getinfo.c \
    $(CURL_DIR)/lib/gopher.c \
    $(CURL_DIR)/lib/hash.c \
    $(CURL_DIR)/lib/headers.c \
    $(CURL_DIR)/lib/hmac.c \
    $(CURL_DIR)/lib/hostip.c \
    $(CURL_DIR)/lib/hostip4.c \
    $(CURL_DIR)/lib/hostip6.c \
    $(CURL_DIR)/lib/hsts.c \
    $(CURL_DIR)/lib/http.c \
    $(CURL_DIR)/lib/http1.c \
    $(CURL_DIR)/lib/http2.c \
    $(CURL_DIR)/lib/http_aws_sigv4.c \
    $(CURL_DIR)/lib/http_chunks.c \
    $(CURL_DIR)/lib/http_digest.c \
    $(CURL_DIR)/lib/http_negotiate.c \
    $(CURL_DIR)/lib/http_ntlm.c \
    $(CURL_DIR)/lib/http_proxy.c \
    $(CURL_DIR)/lib/httpsrr.c \
    $(CURL_DIR)/lib/idn.c \
    $(CURL_DIR)/lib/if2ip.c \
    $(CURL_DIR)/lib/imap.c \
    $(CURL_DIR)/lib/krb5.c \
    $(CURL_DIR)/lib/ldap.c \
    $(CURL_DIR)/lib/llist.c \
    $(CURL_DIR)/lib/macos.c \
    $(CURL_DIR)/lib/md4.c \
    $(CURL_DIR)/lib/md5.c \
    $(CURL_DIR)/lib/memdebug.c \
    $(CURL_DIR)/lib/mime.c \
    $(CURL_DIR)/lib/mprintf.c \
    $(CURL_DIR)/lib/mqtt.c \
    $(CURL_DIR)/lib/multi.c \
    $(CURL_DIR)/lib/multi_ev.c \
    $(CURL_DIR)/lib/netrc.c \
    $(CURL_DIR)/lib/noproxy.c \
    $(CURL_DIR)/lib/openldap.c \
    $(CURL_DIR)/lib/parsedate.c \
    $(CURL_DIR)/lib/pingpong.c \
    $(CURL_DIR)/lib/pop3.c \
    $(CURL_DIR)/lib/progress.c \
    $(CURL_DIR)/lib/psl.c \
    $(CURL_DIR)/lib/rand.c \
    $(CURL_DIR)/lib/rename.c \
    $(CURL_DIR)/lib/request.c \
    $(CURL_DIR)/lib/rtsp.c \
    $(CURL_DIR)/lib/select.c \
    $(CURL_DIR)/lib/sendf.c \
    $(CURL_DIR)/lib/setopt.c \
    $(CURL_DIR)/lib/sha256.c \
    $(CURL_DIR)/lib/share.c \
    $(CURL_DIR)/lib/slist.c \
    $(CURL_DIR)/lib/smb.c \
    $(CURL_DIR)/lib/smtp.c \
    $(CURL_DIR)/lib/socketpair.c \
    $(CURL_DIR)/lib/socks.c \
    $(CURL_DIR)/lib/socks_gssapi.c \
    $(CURL_DIR)/lib/socks_sspi.c \
    $(CURL_DIR)/lib/speedcheck.c \
    $(CURL_DIR)/lib/splay.c \
    $(CURL_DIR)/lib/strcase.c \
    $(CURL_DIR)/lib/strdup.c \
    $(CURL_DIR)/lib/strequal.c \
    $(CURL_DIR)/lib/strerror.c \
    $(CURL_DIR)/lib/system_win32.c \
    $(CURL_DIR)/lib/telnet.c \
    $(CURL_DIR)/lib/tftp.c \
    $(CURL_DIR)/lib/transfer.c \
    $(CURL_DIR)/lib/uint-bset.c \
    $(CURL_DIR)/lib/uint-hash.c \
    $(CURL_DIR)/lib/uint-spbset.c \
    $(CURL_DIR)/lib/uint-table.c \
    $(CURL_DIR)/lib/url.c \
    $(CURL_DIR)/lib/urlapi.c \
    $(CURL_DIR)/lib/version.c \
    $(CURL_DIR)/lib/ws.c

# CurlX sources (utility functions)
CURLX_SRCS = \
    $(CURL_DIR)/lib/curlx/base64.c \
    $(CURL_DIR)/lib/curlx/dynbuf.c \
    $(CURL_DIR)/lib/curlx/inet_ntop.c \
    $(CURL_DIR)/lib/curlx/inet_pton.c \
    $(CURL_DIR)/lib/curlx/multibyte.c \
    $(CURL_DIR)/lib/curlx/nonblock.c \
    $(CURL_DIR)/lib/curlx/strparse.c \
    $(CURL_DIR)/lib/curlx/timediff.c \
    $(CURL_DIR)/lib/curlx/timeval.c \
    $(CURL_DIR)/lib/curlx/version_win32.c \
    $(CURL_DIR)/lib/curlx/wait.c \
    $(CURL_DIR)/lib/curlx/warnless.c \
    $(CURL_DIR)/lib/curlx/winapi.c

# VAUTH sources (authentication modules)
VAUTH_SRCS = \
    $(CURL_DIR)/lib/vauth/cleartext.c \
    $(CURL_DIR)/lib/vauth/cram.c \
    $(CURL_DIR)/lib/vauth/digest.c \
    $(CURL_DIR)/lib/vauth/digest_sspi.c \
    $(CURL_DIR)/lib/vauth/gsasl.c \
    $(CURL_DIR)/lib/vauth/krb5_gssapi.c \
    $(CURL_DIR)/lib/vauth/krb5_sspi.c \
    $(CURL_DIR)/lib/vauth/ntlm.c \
    $(CURL_DIR)/lib/vauth/ntlm_sspi.c \
    $(CURL_DIR)/lib/vauth/oauth2.c \
    $(CURL_DIR)/lib/vauth/spnego_gssapi.c \
    $(CURL_DIR)/lib/vauth/spnego_sspi.c \
    $(CURL_DIR)/lib/vauth/vauth.c

# VTLS sources (TLS backends)
VTLS_SRCS = \
    $(CURL_DIR)/lib/vtls/cipher_suite.c \
    $(CURL_DIR)/lib/vtls/gtls.c \
    $(CURL_DIR)/lib/vtls/hostcheck.c \
    $(CURL_DIR)/lib/vtls/keylog.c \
    $(CURL_DIR)/lib/vtls/mbedtls.c \
    $(CURL_DIR)/lib/vtls/mbedtls_threadlock.c \
    $(CURL_DIR)/lib/vtls/openssl.c \
    $(CURL_DIR)/lib/vtls/rustls.c \
    $(CURL_DIR)/lib/vtls/schannel.c \
    $(CURL_DIR)/lib/vtls/schannel_verify.c \
    $(CURL_DIR)/lib/vtls/vtls.c \
    $(CURL_DIR)/lib/vtls/vtls_scache.c \
    $(CURL_DIR)/lib/vtls/vtls_spack.c \
    $(CURL_DIR)/lib/vtls/wolfssl.c \
    $(CURL_DIR)/lib/vtls/x509asn1.c

# VQUIC sources (QUIC support)
VQUIC_SRCS = \
    $(CURL_DIR)/lib/vquic/curl_msh3.c \
    $(CURL_DIR)/lib/vquic/curl_ngtcp2.c \
    $(CURL_DIR)/lib/vquic/curl_osslq.c \
    $(CURL_DIR)/lib/vquic/curl_quiche.c \
    $(CURL_DIR)/lib/vquic/vquic-tls.c \
    $(CURL_DIR)/lib/vquic/vquic.c

# VSSH sources (SSH support)
VSSH_SRCS = \
    $(CURL_DIR)/lib/vssh/curl_path.c \
    $(CURL_DIR)/lib/vssh/libssh.c \
    $(CURL_DIR)/lib/vssh/libssh2.c \
    $(CURL_DIR)/lib/vssh/wolfssh.c

# All sources combined
LIBCURL_SRCS = $(CURL_SRCS) $(CURLX_SRCS) $(VAUTH_SRCS) $(VTLS_SRCS) $(VQUIC_SRCS) $(VSSH_SRCS)

# Object files
MBEDTLS_OBJS = $(MBEDTLS_SRCS:.c=.o)
LIBCURL_OBJS = $(LIBCURL_SRCS:.c=.o)

# Target libraries
MBEDTLS_LIB = mbedtls.a
LIBCURL_LIB = libcurl.a
TEST_PROG = test_curl

# Default target - build both libraries and test program
all: $(MBEDTLS_LIB) $(LIBCURL_LIB) $(TEST_PROG)

# Download and extract mbedtls
$(MBEDTLS_DIR):
	@echo "Downloading mbedtls $(MBEDTLS_VERSION)..."
	@curl -L $(MBEDTLS_URL) -o $(MBEDTLS_TAR)
	@echo "Validating mbedtls checksum..."
	@echo "$(MBEDTLS_SHA256)  $(MBEDTLS_TAR)" | sha256sum -c
	@echo "Extracting mbedtls..."
	@tar xzf $(MBEDTLS_TAR)
	@mv mbedtls-$(MBEDTLS_VERSION) $(MBEDTLS_DIR)
	@rm $(MBEDTLS_TAR)

# Download and extract curl
$(CURL_DIR):
	@echo "Downloading curl $(CURL_VERSION)..."
	@curl -L $(CURL_URL) -o $(CURL_TAR)
	@echo "Validating curl checksum..."
	@echo "$(CURL_SHA256)  $(CURL_TAR)" | sha256sum -c
	@echo "Extracting curl..."
	@tar xzf $(CURL_TAR)
	@mv curl-$(CURL_VERSION) $(CURL_DIR)
	@rm $(CURL_TAR)
	@echo "Patching curl for cosmo compatibility..."
	@patch -p0 < curl-setup.patch

# Build mbedtls library
$(MBEDTLS_LIB): $(MBEDTLS_DIR) $(MBEDTLS_OBJS)
	$(AR) rcs $@ $(MBEDTLS_OBJS)

# Build libcurl library
$(LIBCURL_LIB): $(CURL_DIR) $(LIBCURL_OBJS)
	$(AR) rcs $@ $(LIBCURL_OBJS)

# Build test program
$(TEST_PROG): test_curl.c $(LIBCURL_LIB) $(MBEDTLS_LIB)
	$(CC) $(CFLAGS) -I$(CURL_DIR)/include -o $@ $< $(LIBCURL_LIB) $(MBEDTLS_LIB)

# Pattern rules for compiling C files
$(MBEDTLS_DIR)/library/%.o: $(MBEDTLS_DIR)/library/%.c | $(MBEDTLS_DIR)
	$(CC) $(MBEDTLS_CFLAGS) -c $< -o $@

$(CURL_DIR)/lib/%.o: $(CURL_DIR)/lib/%.c | $(CURL_DIR)
	$(CC) $(CURL_CFLAGS) -c $< -o $@

$(CURL_DIR)/lib/curlx/%.o: $(CURL_DIR)/lib/curlx/%.c | $(CURL_DIR)
	$(CC) $(CURL_CFLAGS) -c $< -o $@

$(CURL_DIR)/lib/vauth/%.o: $(CURL_DIR)/lib/vauth/%.c | $(CURL_DIR)
	$(CC) $(CURL_CFLAGS) -c $< -o $@

$(CURL_DIR)/lib/vtls/%.o: $(CURL_DIR)/lib/vtls/%.c | $(CURL_DIR)
	$(CC) $(CURL_CFLAGS) -c $< -o $@

$(CURL_DIR)/lib/vquic/%.o: $(CURL_DIR)/lib/vquic/%.c | $(CURL_DIR)
	$(CC) $(CURL_CFLAGS) -c $< -o $@

$(CURL_DIR)/lib/vssh/%.o: $(CURL_DIR)/lib/vssh/%.c | $(CURL_DIR)
	$(CC) $(CURL_CFLAGS) -c $< -o $@

# Individual targets
mbedtls-lib: $(MBEDTLS_LIB)

libcurl-lib: $(LIBCURL_LIB)

# Clean build artifacts
clean:
	rm -f $(MBEDTLS_OBJS) $(LIBCURL_OBJS) $(MBEDTLS_LIB) $(LIBCURL_LIB) $(TEST_PROG)

# Clean everything including downloaded sources
distclean: clean
	rm -rf $(MBEDTLS_DIR) $(CURL_DIR)

# Clean individual targets
clean-mbedtls:
	rm -f $(MBEDTLS_OBJS) $(MBEDTLS_LIB)

clean-libcurl:
	rm -f $(LIBCURL_OBJS) $(LIBCURL_LIB)

# Install target (optional)
install: $(MBEDTLS_LIB) $(LIBCURL_LIB)
	@echo "Install target not implemented. Copy $(MBEDTLS_LIB) and $(LIBCURL_LIB) manually to desired location."

.PHONY: all clean distclean clean-mbedtls clean-libcurl mbedtls-lib libcurl-lib install