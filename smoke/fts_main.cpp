#include <zvec/c_api.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include <stdexcept>
#include <string>

static void Check(zvec_error_code_t code, const char* step) {
    if (code == ZVEC_OK) return;
    char* message = nullptr;
    zvec_get_last_error(&message);
    std::string error = std::string(step) + ": " + (message ? message : "unknown");
    zvec_free(message);
    throw std::runtime_error(error);
}

static void Write(zvec_collection_t* collection, const char* id, const char* text, bool upsert) {
    zvec_doc_t* doc = zvec_doc_create();
    if (!doc) throw std::runtime_error("create doc");
    zvec_doc_set_pk(doc, id);
    Check(zvec_doc_add_field_by_value(doc, "content", ZVEC_DATA_TYPE_STRING, text, std::strlen(text)), "set text");
    const float vector[] = {1, 0, 0};
    Check(zvec_doc_add_field_by_value(doc, "vec", ZVEC_DATA_TYPE_VECTOR_FP32, vector, sizeof(vector)), "set vector");
    const zvec_doc_t* docs[] = {doc};
    size_t ok = 0, failed = 0;
    auto code = upsert ? zvec_collection_upsert(collection, docs, 1, &ok, &failed) :
        zvec_collection_insert(collection, docs, 1, &ok, &failed);
    zvec_doc_destroy(doc);
    Check(code, "write");
    if (ok != 1 || failed) throw std::runtime_error("write counts");
}

static void Query(zvec_collection_t* collection, const char* text,
                  const std::set<std::string>& expected, bool expression = false) {
    auto* query = zvec_vector_query_create();
    auto* fts = zvec_fts_create();
    Check(zvec_vector_query_set_field_name(query, "content"), "query field");
    Check(zvec_vector_query_set_topk(query, 20), "topk");
    Check(expression ? zvec_fts_set_query_string(fts, text) : zvec_fts_set_match_string(fts, text), "query text");
    Check(zvec_vector_query_set_fts(query, fts), "fts payload");
    zvec_fts_destroy(fts);
    zvec_doc_t** docs = nullptr;
    size_t count = 0;
    auto code = zvec_collection_query(collection, query, &docs, &count);
    zvec_vector_query_destroy(query);
    Check(code, "search");
    std::set<std::string> actual;
    bool validScores = true;
    for (size_t i = 0; i < count; ++i) {
        const char* id = zvec_doc_get_pk_copy(docs[i]);
        if (id) actual.insert(id);
        const float score = zvec_doc_get_score(docs[i]);
        validScores = validScores && std::isfinite(score);
        std::printf("hit query=%s id=%s score=%g\n", text, id ? id : "NULL", score);
        zvec_free(const_cast<char*>(id));
    }
    zvec_docs_free(docs, count);
    if (actual != expected || !validScores || actual.size() != count) {
        throw std::runtime_error(std::string("unexpected result set: ") + text);
    }
    std::printf("PASS query=%s count=%zu\n", text, count);
}

static zvec_collection_t* Create(const char* path, const char* tokenizer) {
    auto* schema = zvec_collection_schema_create("fts_smoke");
    auto* fts = zvec_index_params_create(ZVEC_INDEX_TYPE_FTS);
    auto* flat = zvec_index_params_create(ZVEC_INDEX_TYPE_FLAT);
    Check(zvec_index_params_set_fts_params(fts, tokenizer, nullptr,
        std::strcmp(tokenizer, "ngram") == 0 ? "{\"ngram_min\":2,\"ngram_max\":2}" : "{}"), "fts config");
    auto* field = zvec_field_schema_create("content", ZVEC_DATA_TYPE_STRING, false, 0);
    Check(zvec_field_schema_set_index_params(field, fts), "text index");
    Check(zvec_collection_schema_add_field(schema, field), "text schema");
    auto* vec = zvec_field_schema_create("vec", ZVEC_DATA_TYPE_VECTOR_FP32, false, 3);
    Check(zvec_index_params_set_metric_type(flat, ZVEC_METRIC_TYPE_L2), "metric");
    Check(zvec_field_schema_set_index_params(vec, flat), "vector index");
    Check(zvec_collection_schema_add_field(schema, vec), "vector schema");
    zvec_collection_t* collection = nullptr;
    auto code = zvec_collection_create_and_open(path, schema, nullptr, &collection);
    zvec_collection_schema_destroy(schema);
    zvec_index_params_destroy(fts);
    zvec_index_params_destroy(flat);
    Check(code, "create");
    return collection;
}

int ZvecFtsMain(int argc, char** argv) {
    if (argc != 4) { std::fprintf(stderr, "usage: fts_smoke <new-path> <standard|ngram> <create|reopen|verify>\n"); return 2; }
    zvec_collection_t* collection = nullptr;
    try {
        std::printf("Runtime %s tokenizer=%s phase=%s\n", zvec_get_version(), argv[2], argv[3]);
        const bool ngram = std::strcmp(argv[2], "ngram") == 0;
        if (std::strcmp(argv[3], "create") == 0) {
            collection = Create(argv[1], argv[2]);
            Write(collection, "a", ngram ? "鸿蒙端侧检索支持中文资料，型号Qwen3-4B。" : "hello world local search", false);
            Write(collection, "b", ngram ? "鸿蒙设备运行本地模型，内存16GB。" : "hello model memory 16GB", false);
            Write(collection, "c", ngram ? "咖啡制作指南，与模型无关。" : "coffee brewing guide", false);
            Check(zvec_collection_flush(collection), "flush");
        } else {
            Check(zvec_collection_open(argv[1], nullptr, &collection), "open");
        }
        const bool final = std::strcmp(argv[3], "verify") == 0;
        Query(collection, ngram ? "鸿蒙" : "hello", final ? std::set<std::string>{"a"} : std::set<std::string>{"a", "b"});
        Query(collection, ngram ? "检索" : "search", final ? std::set<std::string>{} : std::set<std::string>{"a"});
        Query(collection, "xyzneverexists", {});
        if (!final) {
            Query(collection, ngram ? "Qwen3" : "16GB", ngram ? std::set<std::string>{"a"} : std::set<std::string>{"b"});
            Query(collection, ngram ? "鸿蒙 AND 检索" : "hello AND search", {"a"}, true);
        }
        if (std::strcmp(argv[3], "reopen") == 0) {
            const char* ids[] = {"b"}; size_t ok = 0, failed = 0;
            Check(zvec_collection_delete(collection, ids, 1, &ok, &failed), "delete");
            if (ok != 1 || failed) throw std::runtime_error("delete count");
            Write(collection, "a", ngram ? "鸿蒙设备已经更新文档。" : "hello updated document", true);
            Check(zvec_collection_flush(collection), "flush changes");
            Query(collection, ngram ? "鸿蒙" : "hello", {"a"});
            Query(collection, ngram ? "检索" : "search", {});
        }
        Check(zvec_collection_close(collection), "close");
        collection = nullptr;
        std::puts("PASS full-text phase");
        return 0;
    } catch (const std::exception& error) {
        if (collection) zvec_collection_close(collection);
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}

#ifndef ZVEC_FTS_NAPI
int main(int argc, char** argv) { return ZvecFtsMain(argc, argv); }
#endif
