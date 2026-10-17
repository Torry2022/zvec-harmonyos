# zvec-harmonyos

[简体中文](README.md) | [English](README_EN.md)

Zvec `v0.7.0` 的非官方 HarmonyOS 交叉编译方案，提供固定上游版本的补丁、Windows PowerShell 构建脚本和最小 C API 生命周期 smoke 测试。

> 本仓库不是 Alibaba Zvec 或 HarmonyOS 的官方项目。0.7.0 已完成鸿蒙交叉编译和真机生命周期测试，仍应视为实验性移植；此前发布的 0.6.0 保留作为回退版本。

## 获取预编译库

不需要修改 Zvec 或验证交叉编译过程时，可从 [GitHub Release](https://github.com/Torry2022/zvec-harmonyos/releases/tag/v0.7.0-ohos.1) 下载 `zvec-0.7.0-harmonyos-arm64-v8a.zip`。此前的 [0.6.0 Release](https://github.com/Torry2022/zvec-harmonyos/releases/tag/v0.6.0-ohos.1) 保留供回退。压缩包包含：

- HarmonyOS `arm64-v8a` 的 `libzvec_c_api.so`。
- 对应版本的 `zvec/c_api.h`。
- 构建来源、目标 ABI、运行库依赖和真机验证信息。
- Apache 2.0 许可证、项目来源说明与第三方许可材料。

下载后使用同一 Release 中的 `SHA256SUMS` 校验文件：

```powershell
Get-FileHash .\zvec-0.7.0-harmonyos-arm64-v8a.zip -Algorithm SHA256
```

预编译库仍需按[集成到 HarmonyOS Native 模块](#集成到-harmonyos-native-模块)一节放入应用并配置 CMake。需要审计补丁、升级依赖或自行控制工具链时，请继续使用下方的源码构建流程。

## 构建目标与验证状态

| 项目 | 已验证配置 |
| --- | --- |
| Zvec | `v0.7.0` / `8321c1314a559fd5f909e92498f43e5194bf9b99` |
| 目标平台 | HarmonyOS `arm64-v8a`，API 9，`c++_shared` |
| 主机 | Windows PowerShell |
| 工具链 | DevEco Studio OpenHarmony SDK CMake toolchain |
| 宿主 C 编译器 | Windows GCC / Clang，用于 Snowball 代码生成 |
| 0.7.0 验证状态 | 干净构建、重复构建和三轮真机生命周期测试通过 |
| 真机 | HBN-AL80，HarmonyOS 6.1.1.120 |
| 输出 | `libzvec_c_api.so` |

补丁和构建脚本主要处理以下差异：

1. 使用 HarmonyOS SDK 提供的 libc++，跳过 Linux `libstdc++` 链接逻辑。
2. 将 OHOS toolchain、Ninja 和平台参数传递给 Arrow 与 LZ4 外部构建。
3. 禁用 HarmonyOS 不支持的 Linux 线程亲和性代码。
4. 补充 RocksDB 的 OHOS POSIX 头文件处理及 ANTLR 的 OHOS Clang 分支。
5. 使用 Windows 宿主编译器生成 Snowball 词干处理代码，再由 HarmonyOS 工具链编译目标代码。

## 前置条件

- Git for Windows，且支持递归拉取 submodule。
- DevEco Studio 及其 OpenHarmony Native SDK。
- Windows 宿主 C 编译器（GCC 或 Clang），用于 Snowball 代码生成。
- 足够的磁盘空间；完整构建会生成超过 1 GB 的中间文件。

Zvec 0.7.0 已移除 protobuf/protoc 构建依赖，Snowball 改用 CMake 调度宿主编译器，不再需要旧脚本要求的 GNU Make、Perl 和 sed。

## 干净构建

从空目录开始执行以下步骤：

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

`rev-parse` 必须输出：

```text
8321c1314a559fd5f909e92498f43e5194bf9b99
```

构建产物位于：

```text
build/zvec-0.7.0/lib/libzvec_c_api.so
```

也可以通过 `-OpenHarmonySdk` 显式传入 SDK 目录。`cc`、`gcc` 或 `clang` 已在 `PATH` 中时，可省略 `-HostCCompiler`。脚本会校验上游 commit、重复应用补丁的状态、最终产物和 SHA-256。

## 集成到 HarmonyOS Native 模块

建议在 HarmonyOS 模块中保持以下目录结构：

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

从本仓库构建目录和固定版本的上游源码复制产物：

```powershell
New-Item -ItemType Directory -Force entry\src\main\libs\arm64-v8a | Out-Null
New-Item -ItemType Directory -Force entry\src\main\third_party\zvec\include\zvec | Out-Null

Copy-Item build\zvec-0.7.0\lib\libzvec_c_api.so entry\src\main\libs\arm64-v8a\
Copy-Item .work\zvec-0.7.0\src\include\zvec\c_api.h entry\src\main\third_party\zvec\include\zvec\
```

然后在应用的 Native CMake 中导入：

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

0.7.0 构建产物动态依赖 `libc++_shared.so` 和 `libc.so`，不再动态依赖 0.6.0 中的 `libz.so`。DevEco Studio 通常会为使用 `c++_shared` 的 Native 模块自动打包 `libc++_shared.so`；集成后仍应检查生成的 HAP 或构建中间产物，确认它与 `libzvec_c_api.so` 一同存在。不要从其他 NDK 或不同 SDK 版本复制 C++ 运行库。

本次 Release 配置的 `.so` 大小为 29,266,848 字节（约 27.9 MiB），相较保留的 0.6.0 构建产物 52,343,728 字节缩小约 44.1%。这是本次工具链和配置下的产物对比，不代表所有平台或配置的固定结果。

## Smoke 测试

首次集成建议按以下步骤验证：

1. 将 [`smoke/zvec_smoke.cpp`](smoke/zvec_smoke.cpp) 和 [`smoke/zvec_smoke.h`](smoke/zvec_smoke.h) 加入应用的 Native 源码目录。
2. 把 `zvec_smoke.cpp` 加入应用 Native target 的 `add_library(...)` 源文件列表，并按上一节链接 `zvec`。
3. 从 Node-API 测试入口或临时 Native 调试入口调用 `RunZvecSmokeTest(collectionPath, result)`。
4. `collectionPath` 必须位于应用可写沙箱中，例如在 ArkTS 获取 `context.filesDir` 后拼接 `zvec-smoke` 子目录，再传给 Native 层。
5. 在真机运行并检查返回文本；测试完成后删除临时入口，正式启动流程不得自动执行 smoke 测试。

[`smoke/zvec_smoke.cpp`](smoke/zvec_smoke.cpp) 连续执行三轮完整生命周期：

```text
create -> insert -> query -> close -> reopen -> query -> destroy
```

将 `RunZvecSmokeTest()` 编入测试 HAP，并传入应用可写目录中的 collection 路径。通过结果为：

```text
PASS zvec lifecycle and retrieval smoke test
```

0.7.0 已通过全新源码目录和空构建目录的交叉编译，重复执行构建脚本也通过。在 HBN-AL80 真机上通过签名测试 HAP 运行上述三轮测试，库自身报告 `Runtime v0.7.0`，结果为 PASS。测试使用独立临时 collection 目录，不涉及已有业务数据。不要在正式应用启动时自动执行 smoke 测试。

### 全文检索补测

同一 0.7.0 Release 库已在 Mate 90 Pro Max（CMM-AL10）完成独立签名测试 HAP 的全文检索补测：英文 `standard` 与中文二元 `ngram` 分词、匹配集合、有限分数、布尔 AND、空结果，以及跨进程重开、删除和更新后的持久化均通过。测试共执行 30 次查询断言，不代表完整检索质量或性能评测。

测试源码、构建方式及边界见 [全文检索验证](smoke/FTS_VALIDATION.md)。不需要重新编译或更换已经发布的库。

## 已知限制

- 当前脚本固定 Zvec `v0.7.0`，不保证补丁可直接用于其他版本。旧版构建流程可从 `v0.6.0-ohos.1` 标签获取；0.4.0 与 0.6.0 补丁仍保留。
- 上游 0.7.0 的 C++ 公共接口改为 snake_case；本仓库使用 C API，仍需使用同版本头文件和库重新构建集成方。
- 原生命周期 smoke 与新增全文 smoke 的覆盖范围不同；全文补测仍不覆盖 Jieba 词典分词、混合检索、量化索引性能或 0.6.0 数据目录迁移，不据此宣称这些能力已验证。
- 上游在本次 `OHOS/aarch64` 配置中禁用了 RaBitQ 与 DiskANN，不能把上游 Release 中这两类功能的改进描述为本鸿蒙产物已支持。
- 当前脚本面向 Windows PowerShell 与 DevEco Studio OpenHarmony SDK。
- 仓库不包含 Zvec 上游源码、模型权重、构建目录或预编译 `.so`。

## 许可证

本仓库的脚本、补丁和示例使用 [Apache License 2.0](LICENSE)。Zvec 上游同样使用 Apache License 2.0，详情见 [NOTICE](NOTICE) 与上游仓库。
