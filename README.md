# zvec-harmonyos

Zvec `v0.4.0` 的非官方 HarmonyOS 交叉编译方案，提供固定上游版本的补丁、Windows PowerShell 构建脚本和最小 C API 生命周期 smoke 测试。

> 本仓库不是 Alibaba Zvec 或 HarmonyOS 的官方项目。当前结果来自真实设备验证，但仍应视为实验性移植。

## 已验证范围

| 项目 | 已验证配置 |
| --- | --- |
| Zvec | `v0.4.0` / `cfe9eed2f8c010d0d68add1a69cffb91fa0fbbb6` |
| 目标平台 | HarmonyOS `arm64-v8a`，API 9，`c++_shared` |
| 主机 | Windows PowerShell |
| 工具链 | DevEco Studio OpenHarmony SDK CMake toolchain |
| Host protoc | `21.12` Windows x64 |
| 输出 | `libzvec_c_api.so` |

补丁主要处理以下差异：

1. 使用 HarmonyOS SDK 提供的 libc++，跳过 Linux `libstdc++` 链接逻辑。
2. 将 OHOS toolchain、Ninja 和平台参数传递给 Arrow 与 LZ4 外部构建。
3. 禁用 HarmonyOS 不支持的 Linux 线程亲和性代码。
4. 补充 RocksDB 的 OHOS POSIX 头文件处理及 ANTLR 的 OHOS Clang 分支。

## 前置条件

- Git，且支持递归拉取 submodule。
- DevEco Studio 及其 OpenHarmony Native SDK。
- Windows x64 版 `protoc.exe` 21.12，下载自 [Protocol Buffers v21.12](https://github.com/protocolbuffers/protobuf/releases/tag/v21.12) 的 `protoc-21.12-win64.zip`。
- 足够的磁盘空间；完整构建会生成超过 1 GB 的中间文件。

### `protoc` 的作用与版本校验

`protoc` 是 Protocol Buffers 的代码生成器。Zvec 的构建依赖会在编译期间用它将 `.proto` 定义生成 C++ 头文件和源文件，这些生成文件随后才由 HarmonyOS 工具链交叉编译。它只在 Windows 主机上运行，不会打包进 HAP，也不能替换为 HarmonyOS arm64 可执行文件。

1. 从官方发布页下载并解压 [`protoc-21.12-win64.zip`](https://github.com/protocolbuffers/protobuf/releases/download/v21.12/protoc-21.12-win64.zip)。
2. 执行以下命令校验版本：

```powershell
& 'D:\path\to\protoc-21.12-win64\bin\protoc.exe' --version
```

预期输出：

```text
libprotoc 3.21.12
```

发布标签 `v21.12` 与命令输出中的 `3.21.12` 是 protobuf 当时的两种版本表示，指向同一版本。构建脚本通过 `-ProtocPath` 接收这个主机工具的完整路径。

## 干净构建

从空目录开始执行以下步骤：

```powershell
git clone https://github.com/Torry2022/zvec-harmonyos.git
cd zvec-harmonyos

git clone --recurse-submodules --branch v0.4.0 https://github.com/alibaba/zvec.git .work/zvec-0.4.0
git -C .work/zvec-0.4.0 rev-parse HEAD

$env:DEVECO_SDK_HOME = 'D:\path\to\DevEco Studio\sdk\default\openharmony'

.\build_zvec_ohos.ps1 `
  -SourceDir .work\zvec-0.4.0 `
  -BuildDir build\zvec-0.4.0 `
  -ProtocPath D:\path\to\protoc-21.12-win64\bin\protoc.exe
```

`rev-parse` 必须输出：

```text
cfe9eed2f8c010d0d68add1a69cffb91fa0fbbb6
```

构建产物位于：

```text
build/zvec-0.4.0/lib/libzvec_c_api.so
```

也可以通过 `-OpenHarmonySdk` 显式传入 SDK 目录。脚本会校验上游 commit、重复应用补丁的状态、最终产物和 SHA-256。

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

Copy-Item build\zvec-0.4.0\lib\libzvec_c_api.so entry\src\main\libs\arm64-v8a\
Copy-Item .work\zvec-0.4.0\src\include\zvec\c_api.h entry\src\main\third_party\zvec\include\zvec\
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

构建产物当前依赖 `libz.so`、`libc++_shared.so` 和 `libc.so`。DevEco Studio 通常会为使用 `c++_shared` 的 Native 模块自动打包 `libc++_shared.so`；集成后仍应检查生成的 HAP 或构建中间产物，确认它与 `libzvec_c_api.so` 一同存在。不要从其他 NDK 或不同 SDK 版本复制 C++ 运行库。

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

本仓库已从新的递归克隆和空构建目录完成构建，并在真实 HarmonyOS 设备上通过上述测试。不要在正式应用启动时自动执行 smoke 测试。

## 已知限制

- 目前只固定验证 Zvec `v0.4.0`，不保证补丁可直接用于其他版本。
- 当前脚本面向 Windows PowerShell 与 DevEco Studio OpenHarmony SDK。
- 仓库不包含 Zvec 上游源码、模型权重、构建目录或预编译 `.so`。

## 许可证

本仓库的脚本、补丁和示例使用 [Apache License 2.0](LICENSE)。Zvec 上游同样使用 Apache License 2.0，详情见 [NOTICE](NOTICE) 与上游仓库。
