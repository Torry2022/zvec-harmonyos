# Zvec 0.7.0 全文检索验证

2026-10-17，在 Mate 90 Pro Max（CMM-AL10，设备系统参数 `OpenHarmony-7.0.0.109`）上，使用独立签名测试包 `com.torry2022.retrievalprobe` 验证。

测试使用公开 `v0.7.0-ohos.1` Release 中的同一 arm64 库，SHA-256：

```text
7eb0b464f6ecc281611ca1a2b6bc6988bfef24ba0f771aaa72672b6403f99ed9
```

该库已导出 `zvec_fts_create`、`zvec_fts_set_match_string`、`zvec_fts_set_query_string`、`zvec_index_params_set_fts_params`、`zvec_vector_query_set_fts`，此次补充运行验证，没有更改库或上游补丁。

## 测试范围

| 配置 | 样本 | 检查 |
| --- | --- | --- |
| standard | 3 条英文文档，含 16GB | 关键词、数字英文混合项、AND、空结果、完整命中集合和有限分数 |
| ngram | 3 条中文文档，含 Qwen3-4B | 中文二元切分、混合文本、AND、空结果、完整命中集合和有限分数 |

每种配置执行三次独立进程启动，共 30 次查询断言：

1. `create`：创建新 collection、写入 3 条文档、flush、查询、close。
2. `reopen`：进程退出后打开原 collection、重查、删除文档 b、upsert 文档 a、flush，核对删除结果和旧关键词消失。
3. `verify`：再次退出并打开，核对删除、更新已经持久化。

```text
standard create: PASS
ngram create: PASS
standard reopen: PASS
ngram reopen: PASS
standard verify: PASS
ngram verify: PASS
```

每次重新验证需使用新的测试路径；不要指向已有业务 collection。代码遵循上游 FTS 样例，保留一个固定的占位向量字段，但查询目标始终是文本字段；这不构成向量或混合检索验证。

## 构建与调用

```powershell
.\build_fts_smoke.ps1 -SourceDir .work\zvec-0.7.0 `
  -LibraryDir build\zvec-0.7.0\lib `
  -OpenHarmonySdk 'D:\path\to\sdk\default\openharmony'
```

在允许执行调试 ELF 的 arm64 OHOS 环境中，将输出目录内的程序与库放在一起，依次运行：

```sh
LD_LIBRARY_PATH=. ./zvec_fts_smoke ./new-standard standard create
LD_LIBRARY_PATH=. ./zvec_fts_smoke ./new-standard standard reopen
LD_LIBRARY_PATH=. ./zvec_fts_smoke ./new-standard standard verify
LD_LIBRARY_PATH=. ./zvec_fts_smoke ./new-ngram ngram create
LD_LIBRARY_PATH=. ./zvec_fts_smoke ./new-ngram ngram reopen
LD_LIBRARY_PATH=. ./zvec_fts_smoke ./new-ngram ngram verify
```

部分商业真机禁止 shell 执行上传的 ELF。本次使用签名测试 HAP：将 `fts_main.cpp` 编入 Native target，设置 `ZVEC_FTS_NAPI` 编译定义，通过 Node-API 调用 `int ZvecFtsMain(int argc, char** argv)`；传入应用 `filesDir` 下的独立 collection 路径、分词器名及阶段。每个阶段运行后退出测试包，再启动下一个阶段。返回值为 0 表示该阶段通过，详细断言输出到 stdout/stderr。

不修改 App 服务、模型或实验数据。未验证 Jieba 词典、短语查询、混合检索、大数据性能、断电恢复和旧版数据迁移。
