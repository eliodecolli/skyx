/*
 * File Name: test_db.cpp
 * Purpose: Tests file indexing and validation against a real SQLite database.
 */

#include <persistence/db.h>
#include <blake3.h>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>

// The Makefile supplies these; fallbacks also allow standalone/editor parsing.
#ifndef SKYX_TEST_BUILD_FLAGS
#define SKYX_TEST_BUILD_FLAGS "unknown (not supplied by build)"
#endif

#ifndef SKYX_TEST_BLAKE3_BACKEND
#define SKYX_TEST_BLAKE3_BACKEND "unknown (not supplied by build)"
#endif

#ifndef SKYX_TEST_EXPECT_SIMD
#define SKYX_TEST_EXPECT_SIMD 0
#endif

using namespace skyx::persistence;

// Defined in db.cpp; measure the production file reader, not the in-memory reference.
std::string get_file_hash(const std::string &path);

// Query the vendored dispatcher's actual runtime selection, not just compiler flags.
extern "C" std::size_t blake3_simd_degree(void);

#ifdef SKYX_ASSERT
#undef SKYX_ASSERT
#endif

#define SKYX_ASSERT(expr, msg) \
    do { \
        if (!(expr)) { \
            std::fprintf(stderr, "\033[31m Assert Error:\033[0m %s\n", msg); \
            std::abort(); \
        } \
    } while (false)

struct IndexRow
{
    sqlite3_int64 modified;
    std::string path_key;
    std::string hash;
};

void write_document(const std::string &path, const std::string &contents)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    SKYX_ASSERT(file.is_open(), "Could not create test document.");
    file.write(contents.data(), contents.size());
    file.close();
    SKYX_ASSERT(!file.fail(), "Could not write test document.");
}

bool is_hex_digest_256(const std::string &hash)
{
    return hash.size() == 64 && hash.find_first_not_of("0123456789abcdef") == std::string::npos;
}

IndexRow read_index_row(sqlite3 *db, const std::string &path)
{
    sqlite3_stmt *stmt = nullptr;
    SKYX_ASSERT(sqlite3_prepare_v2(db,
        "SELECT modified, path_key, hash FROM skyx_index WHERE path=?",
        -1, &stmt, nullptr) == SQLITE_OK, "Could not prepare index query.");
    SKYX_ASSERT(sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK,
        "Could not bind document path.");
    SKYX_ASSERT(sqlite3_step(stmt) == SQLITE_ROW, "Document was not stored in the index.");

    IndexRow row;
    row.modified = sqlite3_column_int64(stmt, 0);
    SKYX_ASSERT(sqlite3_column_type(stmt, 1) == SQLITE_TEXT, "Path key must be text.");
    SKYX_ASSERT(sqlite3_column_type(stmt, 2) == SQLITE_TEXT, "File hash must be text.");
    row.path_key.assign(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
        sqlite3_column_bytes(stmt, 1));
    row.hash.assign(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)),
        sqlite3_column_bytes(stmt, 2));

    SKYX_ASSERT(sqlite3_step(stmt) == SQLITE_DONE, "Duplicate rows found for document.");
    SKYX_ASSERT(sqlite3_finalize(stmt) == SQLITE_OK, "Could not finalize index query.");
    SKYX_ASSERT(is_hex_digest_256(row.path_key), "Path key must be a hexadecimal SHA-256 digest.");
    SKYX_ASSERT(is_hex_digest_256(row.hash), "File hash must be a hexadecimal BLAKE3 digest.");
    return row;
}

void assert_file_status(const IndexValidationResult &validation, const std::string &path, FileStatus expected)
{
    SKYX_ASSERT(validation.success, "Index validation failed.");
    const auto entry = std::find_if(validation.result.begin(), validation.result.end(),
        [&path](const FileIndexStatus &entry) { return entry.path == path; });
    SKYX_ASSERT(entry != validation.result.end(), "Document missing from validation results.");
    SKYX_ASSERT(entry->status == expected, "Unexpected document validation status.");
}

std::string reference_blake3(const std::string &contents)
{
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    // One update over the entire input provides a reference for the file read loop.
    blake3_hasher_update(&hasher, contents.data(), contents.size());
    uint8_t digest[BLAKE3_OUT_LEN];
    blake3_hasher_finalize(&hasher, digest, sizeof(digest));

    std::string hash;
    for (const auto byte : digest)
    {
        char hex[3];
        std::snprintf(hex, sizeof(hex), "%02x", static_cast<unsigned int>(byte));
        hash += hex;
    }
    return hash;
}

void report_hash_timing(std::ostream &report, const std::string &path, const std::string &expected_hash)
{
    const auto bytes = std::filesystem::file_size(path);
    const auto start = std::chrono::steady_clock::now();
    const auto hash = get_file_hash(path);
    const auto stop = std::chrono::steady_clock::now();
    const double seconds = std::chrono::duration<double>(stop - start).count();
    const double mib = static_cast<double>(bytes) / (1024.0 * 1024.0);

    report << path << '\t' << bytes << '\t'
           << std::fixed << std::setprecision(6) << mib << '\t'
           << seconds * 1000.0 << '\t';
    if (bytes > 0 && seconds > 0.0)
    {
        report << mib / seconds;
    }
    else
    {
        report << "N/A";
    }
    report << '\t' << (hash == expected_hash ? "OK" : "FAIL") << '\n';
    report.flush();
    SKYX_ASSERT(report.good(), "Could not write hashing timing report.");
    SKYX_ASSERT(hash == expected_hash, "Timed file hash does not match the expected digest.");
}

void test_streaming_hashes(FilePersistence &persistence, sqlite3 *db, std::ostream &report)
{
    // Exercise large streaming reads, including a partial final read.
    constexpr std::size_t chunk_size = 30 * 1024 * 1024;
    const std::string path = "test_docs/large.bin";
    for (const auto size : {chunk_size - 1, chunk_size, chunk_size + 257})
    {
        std::string contents(size, '\0');
        for (std::size_t i = 0; i < contents.size(); ++i)
        {
            contents[i] = static_cast<char>(i % 251);
        }
        write_document(path, contents);
        SKYX_ASSERT(persistence.index_file(path, true) == FileIndexResult::OK,
            "Could not index a large document.");
        const auto expected_hash = reference_blake3(contents);
        SKYX_ASSERT(read_index_row(db, path).hash == expected_hash,
            "Streaming BLAKE3 hash differs from a single-update hash.");
        report_hash_timing(report, path, expected_hash);
        assert_file_status(persistence.validate_index_tree(), path, FileStatus::INDEXED);
    }
}

void test_remove_index(FilePersistence &persistence, sqlite3 *db)
{
    const auto initial = persistence.validate_index_tree();
    SKYX_ASSERT(initial.success, "Could not validate index before removal tests.");
    const std::string document = "test_docs/remove's document.txt";
    const std::string unrelated = "test_docs/remove_unrelated.txt";
    const std::string contents = "same content, different paths";
    write_document(document, contents);
    write_document(unrelated, contents);
    SKYX_ASSERT(persistence.index_file(unrelated, false) == FileIndexResult::OK,
        "Could not index unrelated removal fixture.");
    const auto preserved = read_index_row(db, unrelated);

    const auto assert_index_state = [&](bool document_indexed)
    {
        const auto validation = persistence.validate_index_tree();
        SKYX_ASSERT(validation.success &&
            validation.result.size() == initial.result.size() + 1 + (document_indexed ? 1 : 0),
            "Removal changed an unexpected number of index entries.");
        for (const auto &entry : initial.result)
        {
            assert_file_status(validation, entry.path, entry.status);
        }
        assert_file_status(validation, unrelated, FileStatus::INDEXED);
        const auto retained = read_index_row(db, unrelated);
        SKYX_ASSERT(retained.hash == preserved.hash && retained.path_key == preserved.path_key &&
            retained.modified == preserved.modified, "Removal changed an unrelated index entry.");
        if (document_indexed)
        {
            assert_file_status(validation, document, FileStatus::INDEXED);
        }
        else
        {
            SKYX_ASSERT(std::none_of(validation.result.begin(), validation.result.end(),
                [&document](const FileIndexStatus &entry) { return entry.path == document; }),
                "Removed document still appears in the index.");
        }
    };

    SKYX_ASSERT(persistence.remove_index(document) == FileIndexResult::NOT_FOUND,
        "Removing an existing but never-indexed file should return NOT_FOUND.");
    SKYX_ASSERT(persistence.remove_index("test_docs/never_indexed.txt") == FileIndexResult::NOT_FOUND,
        "Removing a nonexistent, never-indexed path should return NOT_FOUND.");
    assert_index_state(false);

    SKYX_ASSERT(persistence.index_file(document, false) == FileIndexResult::OK,
        "Could not index removal fixture.");
    assert_index_state(true);
    SKYX_ASSERT(persistence.remove_index(document) == FileIndexResult::OK,
        "Could not remove an indexed document.");
    assert_index_state(false);
    SKYX_ASSERT(std::filesystem::exists(document) && get_file_hash(document) == preserved.hash,
        "Removing an index must not delete or modify the document.");

    SKYX_ASSERT(persistence.remove_index(document) == FileIndexResult::NOT_FOUND,
        "Removing the same index twice should return NOT_FOUND.");
    assert_index_state(false);
    SKYX_ASSERT(persistence.index_file(document, false) == FileIndexResult::OK,
        "A removed document should be indexable again without force.");
    assert_index_state(true);

    SKYX_ASSERT(std::filesystem::remove(document), "Could not delete removal fixture.");
    assert_file_status(persistence.validate_index_tree(), document, FileStatus::LOST);
    SKYX_ASSERT(persistence.remove_index(document) == FileIndexResult::OK,
        "Removing an index must work even when its file no longer exists.");
    assert_index_state(false);

    SKYX_ASSERT(persistence.remove_index(unrelated) == FileIndexResult::OK,
        "Could not clean up the remaining removal-test index.");
    const auto final_state = persistence.validate_index_tree();
    SKYX_ASSERT(final_state.success && final_state.result.size() == initial.result.size(),
        "Removal tests did not restore the original index entries.");
    for (const auto &entry : initial.result)
    {
        assert_file_status(final_state, entry.path, entry.status);
    }
    SKYX_ASSERT(std::filesystem::remove(unrelated), "Could not clean up removal fixture.");
}

void test_validate_index(FilePersistence &persistence, sqlite3 *db)
{
    const auto initial = persistence.validate_index_tree();
    SKYX_ASSERT(initial.success, "Could not validate index before single-file validation tests.");
    const std::string path = "test_docs/validation's document.txt";
    const FilePersistence &reader = persistence;

    const auto assert_state = [&](FileStatus expected, bool indexed)
    {
        const auto status = reader.validate_index(path);
        SKYX_ASSERT(status.path == path, "Single-file validation returned the wrong path.");
        SKYX_ASSERT(status.status == expected, "Unexpected single-file validation status.");

        const auto tree = reader.validate_index_tree();
        SKYX_ASSERT(tree.success && tree.result.size() == initial.result.size() + (indexed ? 1 : 0),
            "Validation changed the index or returned an unexpected number of entries.");
        for (const auto &entry : initial.result)
        {
            assert_file_status(tree, entry.path, entry.status);
        }
        if (indexed)
        {
            assert_file_status(tree, path, expected);
        }
        else
        {
            SKYX_ASSERT(std::none_of(tree.result.begin(), tree.result.end(),
                [&path](const FileIndexStatus &entry) { return entry.path == path; }),
                "An unindexed document appeared in tree validation.");
        }
    };

    SKYX_ASSERT(!std::filesystem::exists(path), "Expected a fresh validation fixture.");
    assert_state(FileStatus::NOT_INDEXED, false);
    write_document(path, "original contents");
    assert_state(FileStatus::NOT_INDEXED, false);

    SKYX_ASSERT(persistence.index_file(path, false) == FileIndexResult::OK,
        "Could not index single-file validation fixture.");
    assert_state(FileStatus::INDEXED, true);
    write_document(path, "changed contents");
    assert_state(FileStatus::CORRUPT, true);
    SKYX_ASSERT(persistence.index_file(path, true) == FileIndexResult::OK,
        "Could not reindex single-file validation fixture.");
    assert_state(FileStatus::INDEXED, true);

    // Seed an empty stored digest to exercise INVALID_HASH through the public APIs.
    sqlite3_stmt *stmt = nullptr;
    SKYX_ASSERT(sqlite3_prepare_v2(db, "UPDATE skyx_index SET hash='' WHERE path=?",
        -1, &stmt, nullptr) == SQLITE_OK, "Could not prepare invalid-hash fixture.");
    SKYX_ASSERT(sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK,
        "Could not bind invalid-hash fixture path.");
    SKYX_ASSERT(sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) == 1,
        "Could not clear the fixture's stored hash.");
    SKYX_ASSERT(sqlite3_finalize(stmt) == SQLITE_OK, "Could not finalize invalid-hash fixture.");
    assert_state(FileStatus::INVALID_HASH, true);

    SKYX_ASSERT(std::filesystem::remove(path), "Could not remove validation fixture.");
    assert_state(FileStatus::LOST, true);
    write_document(path, "changed contents");
    assert_state(FileStatus::INVALID_HASH, true);
    SKYX_ASSERT(persistence.index_file(path, true) == FileIndexResult::OK,
        "Could not repair an invalid stored hash by reindexing.");
    assert_state(FileStatus::INDEXED, true);
    SKYX_ASSERT(std::filesystem::remove(path), "Could not remove repaired validation fixture.");
    assert_state(FileStatus::LOST, true);
    write_document(path, "changed contents");
    assert_state(FileStatus::INDEXED, true);

    SKYX_ASSERT(persistence.remove_index(path) == FileIndexResult::OK,
        "Could not remove single-file validation index.");
    assert_state(FileStatus::NOT_INDEXED, false);
    SKYX_ASSERT(std::filesystem::remove(path), "Could not clean up validation fixture.");
    assert_state(FileStatus::NOT_INDEXED, false);
}

void test_file_indexing(std::ostream &report)
{
    FilePersistence persistence;
    SKYX_ASSERT(persistence.open_connection() == DatabaseConnectionResult::OK,
        "Could not open index.db.");

    auto validation = persistence.validate_index_tree();
    SKYX_ASSERT(validation.success && validation.result.empty(), "Expected a fresh, empty index.");
    SKYX_ASSERT(std::filesystem::create_directory("test_docs"), "Expected a fresh test_docs directory.");

    const std::string document = "test_docs/document's name.txt";
    const std::string empty_document = "test_docs/empty.txt";
    const std::string binary_document = "test_docs/binary.bin";
    const std::string missing_document = "test_docs/missing.txt";
    const std::string abc_hash = "6437b3ac38465133ffb63b75273a8db548c558465d79db03fd359c6cd5bd9d85";
    const std::string empty_hash = "af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262";
    const std::string document_path_key = "5e7790ebdbd072c4432816ad203869a594e0071da5f18c2cc0f4d96952f078b0";

    write_document(document, "abc");
    write_document(empty_document, "");
    write_document(binary_document, std::string("abc\0def", 7));

    SKYX_ASSERT(persistence.index_file(missing_document, false) == FileIndexResult::NOT_FOUND,
        "Indexing a missing file should return NOT_FOUND.");
    SKYX_ASSERT(persistence.index_file(missing_document, true) == FileIndexResult::NOT_FOUND,
        "Forced indexing of a missing file should return NOT_FOUND.");

    SKYX_ASSERT(persistence.index_file("test_docs", false) == FileIndexResult::ERROR,
        "Directories must not be indexed as empty files.");

    const auto before_index = std::time(nullptr);
    SKYX_ASSERT(persistence.index_file(document, false) == FileIndexResult::OK,
        "Could not index a new document.");
    SKYX_ASSERT(persistence.index_file(empty_document, false) == FileIndexResult::OK,
        "Could not index an empty document.");
    SKYX_ASSERT(persistence.index_file(binary_document, true) == FileIndexResult::OK,
        "Forced indexing of a new binary document should insert it.");

    sqlite3 *db = nullptr;
    SKYX_ASSERT(sqlite3_open_v2("index.db", &db, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK,
        "Could not open index for verification.");
    const auto original = read_index_row(db, document);
    SKYX_ASSERT(original.hash == abc_hash, "Incorrect BLAKE3 hash for abc.");
        SKYX_ASSERT(original.path_key == document_path_key, "Path key must still use SHA-256.");
    SKYX_ASSERT(original.modified >= before_index && original.modified <= std::time(nullptr),
        "Incorrect modification timestamp.");
    SKYX_ASSERT(read_index_row(db, empty_document).hash == empty_hash,
        "Incorrect BLAKE3 hash for an empty file.");
    const auto binary = read_index_row(db, binary_document);
    SKYX_ASSERT(binary.hash == reference_blake3(std::string("abc\0def", 7)),
            "Binary hashing must include bytes after a null byte.");
    SKYX_ASSERT(binary.path_key != original.path_key, "Different paths must have different keys.");

    report_hash_timing(report, document, abc_hash);
    report_hash_timing(report, empty_document, empty_hash);
    report_hash_timing(report, binary_document, binary.hash);

    validation = persistence.validate_index_tree();
    SKYX_ASSERT(validation.result.size() == 3, "Expected exactly three indexed documents.");
    assert_file_status(validation, document, FileStatus::INDEXED);
    assert_file_status(validation, empty_document, FileStatus::INDEXED);
    assert_file_status(validation, binary_document, FileStatus::INDEXED);

    SKYX_ASSERT(persistence.index_file(document, false) == FileIndexResult::ALREADY_INDEXED,
        "Indexing the same path twice should return ALREADY_INDEXED.");
    write_document(document, "abcd");
    assert_file_status(persistence.validate_index_tree(), document, FileStatus::CORRUPT);
    SKYX_ASSERT(persistence.index_file(document, false) == FileIndexResult::ALREADY_INDEXED,
        "Changed files must not be reindexed without force.");
    const auto unchanged = read_index_row(db, document);
    SKYX_ASSERT(unchanged.hash == original.hash && unchanged.modified == original.modified,
        "Index changed without forced reindexing.");

    const auto before_update = std::time(nullptr);
    SKYX_ASSERT(persistence.index_file(document, true) == FileIndexResult::OK,
        "Could not force reindex a changed document.");
    const auto updated = read_index_row(db, document);
    SKYX_ASSERT(updated.hash == reference_blake3("abcd"), "Forced reindexing did not store the BLAKE3 hash.");
    report_hash_timing(report, document, updated.hash);
    SKYX_ASSERT(updated.path_key == original.path_key, "Reindexing changed the path key.");
    SKYX_ASSERT(updated.modified >= before_update && updated.modified <= std::time(nullptr),
        "Forced reindexing did not refresh the timestamp.");
    validation = persistence.validate_index_tree();
    SKYX_ASSERT(validation.result.size() == 3, "Reindexing should update, not insert another row.");
    assert_file_status(validation, document, FileStatus::INDEXED);

    SKYX_ASSERT(std::filesystem::remove(binary_document), "Could not remove binary document.");
    SKYX_ASSERT(persistence.index_file(binary_document, true) == FileIndexResult::NOT_FOUND,
        "Reindexing a deleted document should return NOT_FOUND.");
    validation = persistence.validate_index_tree();
    SKYX_ASSERT(validation.result.size() == 3, "Missing documents should remain in the index.");
    assert_file_status(validation, document, FileStatus::INDEXED);
    assert_file_status(validation, empty_document, FileStatus::INDEXED);
    assert_file_status(validation, binary_document, FileStatus::LOST);

    test_streaming_hashes(persistence, db, report);
    test_remove_index(persistence, db);
    test_validate_index(persistence, db);

    SKYX_ASSERT(sqlite3_close(db) == SQLITE_OK, "Could not close verification connection.");
    std::filesystem::remove_all("test_docs");
    SKYX_ASSERT(!std::filesystem::exists("test_docs"), "Test documents were not cleaned up.");
}

int main(int argc, char **argv)
{
    SKYX_ASSERT(argc <= 2, "Usage: test_db [timing_report.txt]");
    const char *report_path = argc == 2 ? argv[1] : "hash_timing_report.txt";
    std::ofstream report(report_path, std::ios::trunc);
    SKYX_ASSERT(report.is_open(), "Could not open hashing timing report.");
    const auto now = std::time(nullptr);
    const auto simd_degree = blake3_simd_degree();
    report << "SKYX BLAKE3 file hashing timing report\n"
           << "Run: " << std::put_time(std::localtime(&now), "%Y-%m-%d %H:%M:%S %Z") << '\n'
           << "One additional get_file_hash() measurement per fixture, using a steady clock.\n"
           << "Includes file checks/open/read, buffer allocation, BLAKE3, and hex formatting.\n"
           << "Excludes fixture creation, reference hashing, SQLite, and report writing.\n"
           << "Files were recently written/read and may be cached by the operating system.\n"
           << "Shared compiler flags: " << SKYX_TEST_BUILD_FLAGS << '\n'
           << "BLAKE3 backend: " << SKYX_TEST_BLAKE3_BACKEND << '\n'
           << "BLAKE3 runtime SIMD width: " << simd_degree << " chunks (1 means scalar).\n"
           << "Sanitizer instrumentation, when present in the flags above, affects timings.\n\n"
           << "File\tBytes\tMiB\tElapsed_ms\tMiB_per_second\tStatus\n";
    report.flush();
    SKYX_ASSERT(report.good(), "Could not write hashing timing report header.");
    SKYX_ASSERT(!SKYX_TEST_EXPECT_SIMD || simd_degree > 1,
        "BLAKE3 SIMD was requested but the runtime dispatcher selected scalar hashing.");

    const auto start = std::chrono::steady_clock::now();
    test_file_indexing(report);
    const double total_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    report << "\nTotal test runtime (including setup, assertions, and document cleanup): "
           << total_ms << " ms\nResult: PASS\n";
    report.close();
    SKYX_ASSERT(!report.fail(), "Could not finish hashing timing report.");
    std::printf("\033[32mTests passed successfully.\033[0m\n");
    std::printf("Hash timing report: %s\n", report_path);
    return 0;
}
