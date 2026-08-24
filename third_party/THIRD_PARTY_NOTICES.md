# Third-Party Notices

This project vendors the following single-header libraries directly into
`third_party/`. No package manager or submodules are used — headers are
checked into the repo verbatim from a tagged upstream release so the build
stays fully offline and reproducible.

## miniaudio

- Source: https://github.com/mackron/miniaudio
- Version/tag: `0.11.21` (commit `4a5b74bef029b3592c54b6048650ee5f972c1a48`)
- File: `third_party/miniaudio/miniaudio.h`
- License: Public Domain (Unlicense) OR MIT-0, dual-licensed at the author's choice
- Used for: real-time audio device I/O and WAV sample decoding.

## nlohmann/json

- Source: https://github.com/nlohmann/json
- Version/tag: `v3.11.3` (commit `9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03`)
- File: `third_party/nlohmann/json.hpp` (amalgamated single header)
- License: MIT
- Used for: parsing the JSON composition/config files.

## doctest

- Source: https://github.com/doctest/doctest
- Version/tag: `v2.4.11` (commit `ae7a13539fb71f270b87eb2e874fbac80bc8dda2`)
- File: `third_party/doctest/doctest.h`
- License: MIT
- Used for: the `Tests` project's unit test framework.
