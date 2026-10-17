# zvec-harmonyos

[简体中文](README.md) | [English](README_EN.md)

An unofficial HarmonyOS cross-compilation setup for Zvec `v0.7.0`, providing pinned upstream patches, Windows PowerShell build scripts, and a minimal C API lifecycle smoke test.

> This is not an official Alibaba Zvec or HarmonyOS project. Version 0.7.0 has passed cross-compilation and device lifecycle tests but remains an experimental port. The previously released 0.6.0 is available as a fallback.

## Prebuilt library

If you do not need to modify Zvec or reproduce the build, download `zvec-0.7.0-harmonyos-arm64-v8a.zip` from [Release v0.7.0-ohos.2](https://github.com/Torry2022/zvec-harmonyos/releases/tag/v0.7.0-ohos.2). The previous [ohos.1](https://github.com/Torry2022/zvec-harmonyos/releases/tag/v0.7.0-ohos.1) and [0.6.0 Release](https://github.com/Torry2022/zvec-harmonyos/releases/tag/v0.6.0-ohos.1) remain available for fallback. The archive includes:

- `libzvec_c_api.so` for HarmonyOS `arm64-v8a`.
- The matching `zvec/c_api.h` header.
- Build provenance, target ABI, runtime dependencies, and device validation details.
- Apache 2.0 license, project attribution, and third-party license materials.

Verify the download against `SHA256SUMS` from the same Release:

```powershell
Get-FileHash .\zvec-0.7.0-harmonyos-arm64-v8a.zip -Algorithm SHA256
```

The prebuilt library still needs to be placed in your application and linked through CMake as described under [Native integration](#native-integration). To review patches, upgrade dependencies, or control the toolchain, use the source build steps below.

## Build target and validation

| Item | Validated configuration |
| --- | --- |
| Zvec | `v0.7.0` / `8321c1314a559fd5f909e92498f43e5194bf9b99` |
| Target | HarmonyOS `arm64-v8a`, API 9, `c++_shared` |
| Host | Windows PowerShell |
| Toolchain | DevEco Studio OpenHarmony SDK CMake toolchain |
| Host C compiler | Windows GCC / Clang, for Snowball code generation |
| 0.7.0 validation | Clean build, repeated build, and three device lifecycle rounds passed |
| Devices | HBN-AL80 (ohos.1); Mate 90 Pro Max / CMM-AL10 (ohos.2) |
| Output | `libzvec_c_api.so` |

The patches and scripts address these platform differences:

1. Use the HarmonyOS SDK's libc++ instead of Linux `libstdc++` linkage.
2. Pass the OHOS toolchain, Ninja, and platform options to Arrow and LZ4 external builds.
3. Disable Linux thread-affinity code unsupported on HarmonyOS.
4. Adjust RocksDB POSIX includes for OHOS and add an OHOS Clang branch in ANTLR.
5. Generate Snowball code using a Windows host compiler, then compile the generated code with the HarmonyOS target toolchain.

Source builds also apply `patches/zvec-0.7.0-fts-readonly.patch`, based on the read-only memtable change in upstream [PR #770](https://github.com/alibaba/zvec/pull/770), which has not been merged. It avoids allocating a million hash buckets per column family when opening full-text indexes read-only. Writable memtable settings, the index format, and the public C API are unchanged. The `v0.7.0-ohos.2` prebuilt archive includes this fix. See the [read-only full-text memory comparison](smoke/FTS_READONLY_VALIDATION.md) for validation results.

## Prerequisites

- Git for Windows with recursive submodule support.
- DevEco Studio and its OpenHarmony Native SDK.
- A Windows host C compiler (GCC or Clang) for Snowball code generation.
- Sufficient disk space: a full build produces more than 1 GB of intermediate files.

Zvec 0.7.0 no longer depends on protobuf/protoc for the build. Snowball now uses CMake to invoke the host compiler; GNU Make, Perl, and sed required by the older script are no longer needed.

## Clean build

Start from an empty directory:

```powershell
git clone https://github.com/Torry2022/zvec-harmonyos.git
cd zvec-harmonyos

git clone --recurse-submodules --branch v0.7.0 https://github.com/alibaba/zvec.git .work/zvec-0.7.0
git -C .work/zvec-0.7.0 rev-parse HEAD

$env:DEVECO_SDK_HOME = 'D:\path\to\DevEco Studio\sdk\default\openharmony'

.\build_zvec_ohos.ps1 `
  -SourceDir .work\zvec-0.7.0 `
  -BuildDir build\zvec-0.7.0 `
  -HostCCompiler D:\path\to\gcc.exe
```

`rev-parse` must print:

```text
8321c1314a559fd5f909e92498f43e5194bf9b99
```

Build output:

```text
build/zvec-0.7.0/lib/libzvec_c_api.so
```

You can pass the SDK directory explicitly with `-OpenHarmonySdk`. If `cc`, `gcc`, or `clang` is already on PATH, omit `-HostCCompiler`. The script checks the upstream commit, repeated patch application state, output artifact, and SHA-256.

## Native integration

Use the following layout in your HarmonyOS module:

```text
entry/src/main/
|-- cpp/
|   `-- CMakeLists.txt
|-- libs/
|   `-- arm64-v8a/
|       `-- libzvec_c_api.so
`-- third_party/
    `-- zvec/
        `-- include/
            `-- zvec/
                `-- c_api.h
```

Copy the artifacts from the build directory and pinned upstream source:

```powershell
New-Item -ItemType Directory -Force entry\src\main\libs\arm64-v8a | Out-Null
New-Item -ItemType Directory -Force entry\src\main\third_party\zvec\include\zvec | Out-Null

Copy-Item build\zvec-0.7.0\lib\libzvec_c_api.so entry\src\main\libs\arm64-v8a\
Copy-Item .work\zvec-0.7.0\src\include\zvec\c_api.h entry\src\main\third_party\zvec\include\zvec\
```

Import the library in the application's Native CMake configuration:

```cmake
add_library(zvec SHARED IMPORTED)
set_target_properties(zvec PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_LIST_DIR}/../libs/${OHOS_ARCH}/libzvec_c_api.so"
)

target_include_directories(entry PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../third_party/zvec/include"
)
target_link_libraries(entry PUBLIC zvec)
```

The 0.7.0 artifact dynamically depends on `libc++_shared.so` and `libc.so`, unlike 0.6.0 which also depended on `libz.so`. DevEco Studio usually packages `libc++_shared.so` automatically for Native modules using `c++_shared`; still inspect the generated HAP or build intermediates to confirm both it and `libzvec_c_api.so` are included. Do not copy a C++ runtime from a different NDK or SDK version.

This Release's `.so` is 29,269,792 bytes (about 27.9 MiB), approximately 44.1% smaller than the retained 0.6.0 artifact at 52,343,728 bytes. This comparison applies to these builds and configurations, not every platform or set of build options.

## Smoke tests

For initial integration:

1. Add [smoke/zvec_smoke.cpp](smoke/zvec_smoke.cpp) and [smoke/zvec_smoke.h](smoke/zvec_smoke.h) to the application's Native sources.
2. Add `zvec_smoke.cpp` to the Native target's `add_library(...)` source list and link `zvec` as shown above.
3. Call `RunZvecSmokeTest(collectionPath, result)` from a test Node-API or temporary Native debug entry point.
4. Keep `collectionPath` in the application's writable sandbox. For example, obtain `context.filesDir` in ArkTS, append a `zvec-smoke` subdirectory, and pass that path to Native code.
5. Run on a device and inspect the returned text. Remove the temporary entry point afterwards; never run this smoke test automatically during normal app startup.

[smoke/zvec_smoke.cpp](smoke/zvec_smoke.cpp) runs three complete lifecycle rounds:

```text
create -> insert -> query -> close -> reopen -> query -> destroy
```

Compile `RunZvecSmokeTest()` into a test HAP and pass a collection path in the writable sandbox. Expected output:

```text
PASS zvec lifecycle and retrieval smoke test
```

`v0.7.0-ohos.1` passed cross-compilation from fresh source and build directories, as well as a repeated script run. A signed test HAP passed all three rounds on an HBN-AL80 device; the library reported `Runtime v0.7.0`. Tests used independent temporary collection directories without touching business data. Do not run the smoke test automatically during normal application startup.

### Additional full-text tests

The `v0.7.0-ohos.1` library passed full-text tests in a separately signed HAP on a Mate 90 Pro Max (CMM-AL10): English `standard` and Chinese bigram `ngram` tokenization, result ID sets, finite scores, Boolean AND, empty results, cross-process reopening, and persistence after deletion and updates. The suite executed 30 query assertions; it is not a full retrieval-quality or performance evaluation.

See the [full-text validation record](smoke/FTS_VALIDATION.md) for code, build instructions, and coverage boundaries. The final `v0.7.0-ohos.2` prebuilt library passed the same full-text suite and three lifecycle rounds on this device. For 500 real-corpus queries in each of two index states, its complete Top100 IDs and scores matched the original library, and the returned text matched the frozen corpus. See the [read-only full-text validation](smoke/FTS_READONLY_VALIDATION.md) for memory measurements and coverage.

## Known limitations

- Scripts pin Zvec `v0.7.0`; patches are not guaranteed to apply to other versions. Retrieve the old build workflow from tag `v0.6.0-ohos.1`. The 0.4.0 and 0.6.0 patches remain in the repository.
- Upstream 0.7.0 changed public C++ interfaces to snake_case. This port uses the C API; consumers still need to rebuild against the matching header and library.
- The original lifecycle smoke test and full-text suite cover different capabilities. Full-text testing does not cover Zvec's built-in Jieba dictionary tokenizer, hybrid retrieval, quantized-index performance, or migration of 0.6.0 data directories. These are not claimed as validated.
- Upstream disables RaBitQ and DiskANN in this `OHOS/aarch64` configuration; their upstream release improvements do not imply support in this port.
- Scripts target Windows PowerShell and the DevEco Studio OpenHarmony SDK.
- The repository does not contain upstream Zvec sources, model weights, build directories, or prebuilt `.so` files.

## License

Scripts, patches, and examples use [Apache License 2.0](LICENSE). Upstream Zvec also uses Apache License 2.0; see [NOTICE](NOTICE) and the upstream repository for attribution.
