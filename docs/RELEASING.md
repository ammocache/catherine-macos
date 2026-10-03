# Releasing

**Do not name release tags like `v1.0.0`.**

The ReXGlue SDK's CMake runs `git describe --match "v[0-9]*.[0-9]*"` in the
top-level project folder (this repository, not the SDK) and reads any matching
tag as an SDK version. A tag such as `v1.0.0` makes every fresh clone fail to
configure with *"floor version ... is behind tag version"*.

Use a prefix that does not match, for example `release-1.0`. The release *title*
can still say "v1.0".
