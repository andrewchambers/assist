# cosmo-curl

Build configuration for libcurl to support minicoder's [Cosmopolitan Libc](https://github.com/jart/cosmopolitan) builds.

**This is NOT a replacement for system curl.** It's only used when building portable minicoder binaries with:

```bash
./support/build-with-cosmo.sh
```

Regular builds use your system's libcurl.