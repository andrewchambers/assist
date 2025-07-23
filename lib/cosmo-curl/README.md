# cosmo-curl

Build configuration for libcurl to support minicoder's [Cosmopolitan Libc](https://github.com/jart/cosmopolitan) builds.

**This is NOT a replacement for system curl.** It's only used when building portable minicoder binaries with:

```bash
./support/build-with-cosmo.sh
```

Regular builds use your system's libcurl.

## Building

Before building, you need to download the source dependencies:

```bash
cd lib/cosmo-curl
./download.sh
```

The download script is idempotent and will skip downloads if the sources already exist.