#include "zvec_smoke.h"

#include <cstring>
#include <sstream>

#include <zvec/c_api.h>

namespace {

bool SetError(zvec_error_code_t code, const char* step, std::string& result) {
    char* message = nullptr;
    zvec_get_last_error(&message);
    std::ostringstream stream;
    stream << step << " failed: code=" << static_cast<int>(code)
           << " message=" << (message ? message : "unknown");
    zvec_free(message);
    result = stream.str();
    return false;
}

bool VerifyNearest(zvec_collection_t* collection, std::string& result) {
    const float queryVector[3] = {0.05f, 0.9f, 0.05f};
    zvec_vector_query_t* query = zvec_vector_query_create();
    if (!query) {
        result = "create query failed";
        return false;
    }
    zvec_vector_query_set_field_name(query, "embedding");
    zvec_vector_query_set_query_vector(query, queryVector, sizeof(queryVector));
    zvec_vector_query_set_topk(query, 3);
    zvec_vector_query_set_filter(query, "");
    zvec_vector_query_set_include_doc_id(query, true);

    zvec_doc_t** docs = nullptr;
    size_t count = 0;
    zvec_error_code_t code = zvec_collection_query(collection, query, &docs, &count);
    zvec_vector_query_destroy(query);
    if (code != ZVEC_OK) {
        return SetError(code, "query", result);
    }
    const char* actual = count > 0 ? zvec_doc_get_pk_copy(docs[0]) : nullptr;
    const bool matched = actual && std::strcmp(actual, "doc-b") == 0;
    if (!matched) {
        result = std::string("unexpected nearest document: ") + (actual ? actual : "none");
    }
    zvec_free(const_cast<char*>(actual));
    zvec_docs_free(docs, count);
    return matched;
}

bool CreateCollection(const std::string& path, zvec_collection_t** collection, std::string& result) {
    zvec_collection_schema_t* schema = zvec_collection_schema_create("smoke_collection");
    zvec_index_params_t* flat = zvec_index_params_create(ZVEC_INDEX_TYPE_FLAT);
    if (!schema || !flat) {
        zvec_collection_schema_destroy(schema);
        zvec_index_params_destroy(flat);
        result = "create schema failed";
        return false;
    }
    zvec_index_params_set_metric_type(flat, ZVEC_METRIC_TYPE_L2);
    zvec_field_schema_t* id = zvec_field_schema_create("id", ZVEC_DATA_TYPE_STRING, false, 0);
    zvec_field_schema_t* text = zvec_field_schema_create("text", ZVEC_DATA_TYPE_STRING, false, 0);
    zvec_field_schema_t* embedding =
        zvec_field_schema_create("embedding", ZVEC_DATA_TYPE_VECTOR_FP32, false, 3);
    zvec_field_schema_set_index_params(embedding, flat);

    zvec_error_code_t code = zvec_collection_schema_add_field(schema, id);
    if (code == ZVEC_OK) code = zvec_collection_schema_add_field(schema, text);
    if (code == ZVEC_OK) code = zvec_collection_schema_add_field(schema, embedding);
    if (code == ZVEC_OK) code = zvec_collection_create_and_open(path.c_str(), schema, nullptr, collection);
    zvec_index_params_destroy(flat);
    zvec_collection_schema_destroy(schema);
    return code == ZVEC_OK || SetError(code, "create collection", result);
}

bool InsertFixture(zvec_collection_t* collection, std::string& result) {
    const char* ids[] = {"doc-a", "doc-b", "doc-c"};
    const char* texts[] = {"alpha", "beta", "gamma"};
    const float vectors[][3] = {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};
    zvec_doc_t* docs[3] = {nullptr, nullptr, nullptr};
    for (size_t i = 0; i < 3; ++i) {
        docs[i] = zvec_doc_create();
        if (!docs[i]) {
            result = "create document failed";
            return false;
        }
        zvec_doc_set_pk(docs[i], ids[i]);
        zvec_doc_add_field_by_value(docs[i], "id", ZVEC_DATA_TYPE_STRING, ids[i], std::strlen(ids[i]));
        zvec_doc_add_field_by_value(docs[i], "text", ZVEC_DATA_TYPE_STRING, texts[i], std::strlen(texts[i]));
        zvec_doc_add_field_by_value(docs[i], "embedding", ZVEC_DATA_TYPE_VECTOR_FP32,
                                    vectors[i], sizeof(vectors[i]));
    }
    size_t successCount = 0;
    size_t errorCount = 0;
    zvec_error_code_t code = zvec_collection_insert(
        collection, const_cast<const zvec_doc_t**>(docs), 3, &successCount, &errorCount);
    for (zvec_doc_t* doc : docs) zvec_doc_destroy(doc);
    if (code != ZVEC_OK) return SetError(code, "insert", result);
    if (successCount != 3 || errorCount != 0) {
        result = "insert count mismatch";
        return false;
    }
    code = zvec_collection_flush(collection);
    return code == ZVEC_OK || SetError(code, "flush", result);
}

bool RunCycle(const std::string& path, std::string& result) {
    zvec_collection_t* collection = nullptr;
    if (!CreateCollection(path, &collection, result)) return false;
    if (!InsertFixture(collection, result) || !VerifyNearest(collection, result)) {
        zvec_collection_close(collection);
        return false;
    }
    zvec_error_code_t code = zvec_collection_close(collection);
    if (code != ZVEC_OK) return SetError(code, "close", result);

    collection = nullptr;
    code = zvec_collection_open(path.c_str(), nullptr, &collection);
    if (code != ZVEC_OK) return SetError(code, "reopen", result);
    if (!VerifyNearest(collection, result)) {
        zvec_collection_close(collection);
        return false;
    }
    code = zvec_collection_destroy(collection);
    if (code != ZVEC_OK) {
        zvec_collection_close(collection);
        return SetError(code, "destroy", result);
    }
    code = zvec_collection_close(collection);
    return code == ZVEC_OK || SetError(code, "close destroyed collection", result);
}

} // namespace

bool RunZvecSmokeTest(const std::string& collectionPath, std::string& result) {
    for (int cycle = 1; cycle <= 3; ++cycle) {
        if (!RunCycle(collectionPath, result)) {
            result = "cycle " + std::to_string(cycle) + ": " + result;
            return false;
        }
    }
    result = "PASS zvec lifecycle and retrieval smoke test";
    return true;
}
