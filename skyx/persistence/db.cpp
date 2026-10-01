#include <array>
#include <persistence/db.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <openssl/evp.h>
#include <blake3.h>

#define SKYX_HASH_CHUNK_SIZE         (1024 * 1024)

std::string blake3(std::ifstream &stream)
{
    if ( !stream ) return "";

    blake3_hasher hasher;
    blake3_hasher_init(&hasher);

    std::vector<char> buffer(SKYX_HASH_CHUNK_SIZE);
    while ( stream.read(buffer.data(), buffer.size()) || stream.gcount() > 0 )
    {
        blake3_hasher_update(&hasher, buffer.data(), stream.gcount());
    }

    if ( stream.bad() || !stream.eof() ) return "";

    std::array<uint8_t, BLAKE3_OUT_LEN> output;
    blake3_hasher_finalize(&hasher, output.data(), output.size());

    std::stringstream ss;
    for ( unsigned int i = 0; i < output.size(); i++ )
    {
        ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned int>(output[i]);
    }

    return ss.str();
}

std::string sha256(const std::vector<char> &input)
{
    EVP_MD_CTX *context = EVP_MD_CTX_new();
    if ( !context ) return "";

    if ( EVP_DigestInit(context, EVP_sha256()) != 1 )
    {
        EVP_MD_CTX_free(context);
        return "";
    }

    if ( EVP_DigestUpdate(context, input.data(), input.size()) != 1 )
    {
        EVP_MD_CTX_free(context);
        return "";
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int length = 0;
    if ( EVP_DigestFinal_ex(context, hash, &length) != 1 )
    {
        EVP_MD_CTX_free(context);
        return "";
    }

    EVP_MD_CTX_free(context);

    std::stringstream ss;
    for ( unsigned int i = 0; i < length; i++ )
    {
        ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned int>(hash[i]);
    }

    return ss.str();
}


std::string get_file_hash(const std::string &path)
{
    if ( !std::filesystem::is_regular_file(path) ) return "";
    std::ifstream f(path, std::ios::binary);
    return blake3(f);
}

std::string get_string_hash(const std::string &str)
{
    std::vector<char> v {
        str.begin(),
        str.end()
    };
    return sha256(v);
}


namespace skyx
{
    namespace persistence
    {
        FilePersistence::~FilePersistence()
        {
            sqlite3_close_v2(m_db);
        }

        FileIndexResult FilePersistence::apply_index(const std::string &path, const std::string &hash)
        {
            std::string path_key = get_string_hash(path);

            if ( path_key == "" )
            {
                std::printf("FilePersistence:: Error while generating path key for file: %s\n", path.c_str());
                return FileIndexResult::ERROR;
            }

            std::string query = "INSERT INTO skyx_index (modified, path, path_key, hash) VALUES (?, ?, ?, ?)";
            sqlite3_stmt *stmt;
            if ( sqlite3_prepare_v2(m_db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK )
            {
                std::printf("FilePersistence:: Error while perparing SQL statement: %s\n", sqlite3_errmsg(m_db));
                return FileIndexResult::ERROR;
            }
            sqlite3_bind_int64(stmt, 1, (sqlite3_int64)time(NULL));                                 // modified
            sqlite3_bind_text64(stmt, 2, path.c_str(), path.length(), SQLITE_STATIC, SQLITE_UTF8);  // path
            sqlite3_bind_text64(stmt, 3, path_key.c_str(), path_key.length(), SQLITE_STATIC, SQLITE_UTF8);  // path key
            sqlite3_bind_text64(stmt, 4, hash.c_str(), hash.length(), SQLITE_STATIC, SQLITE_UTF8);  // hash

            if ( sqlite3_step(stmt) != SQLITE_DONE )
            {
                std::printf("FilePersistence:: Error indexing file %s: %s\n", path.c_str(), sqlite3_errmsg(m_db));
                sqlite3_finalize(stmt);
                return FileIndexResult::ERROR;
            }

            std::printf("FilePersistence:: Indexed file %s\n", path.c_str());
            sqlite3_finalize(stmt);

            return FileIndexResult::OK;
        }

        FileIndexResult FilePersistence::remove_index(const std::string &path)
        {
            const auto path_key = get_string_hash(path);
            if ( !file_indexed(path) )
            {
                std::printf("FilePersistence:: Index for %s not found.\n", path.c_str());
                return FileIndexResult::NOT_FOUND;
            }

            const std::string expr = "DELETE FROM skyx_index WHERE path_key=?";
            sqlite3_stmt *stmt;
            if ( sqlite3_prepare_v2(m_db, expr.c_str(), -1, &stmt, nullptr) != SQLITE_OK )
            {
                std::printf("FilePersistence:: Failed to prepare SQL query for index removal.\n");
                return FileIndexResult::ERROR;
            }

            sqlite3_bind_text(stmt, 1, path_key.c_str(), -1, SQLITE_STATIC);

            if ( sqlite3_step(stmt) != SQLITE_DONE )
            {
                std::printf(
                    "FilePersistence:: Error while trying to remove index for %s (%s): %s\n",
                    path.c_str(),
                    path_key.c_str(),
                    sqlite3_errmsg(m_db)
                );

                sqlite3_finalize(stmt);
                return FileIndexResult::ERROR;
            }

            sqlite3_finalize(stmt);
            return FileIndexResult::OK;
        }

        FileIndexResult FilePersistence::update_index(const std::string &path, const std::string &hash)
        {
            std::string path_key = get_string_hash(path);

            if ( path_key == "" )
            {
                std::printf("FilePersistence:: Error while generating path key for file: %s\n", path.c_str());
                return FileIndexResult::ERROR;
            }

            std::string query = "UPDATE skyx_index SET modified=?, hash=? WHERE path_key=?";
            sqlite3_stmt *stmt;
            if ( sqlite3_prepare_v2(m_db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK )
            {
                std::printf("FilePersistence:: Error while perparing SQL statement: %s\n", sqlite3_errmsg(m_db));
                return FileIndexResult::ERROR;
            }
            sqlite3_bind_int64(stmt, 1, (sqlite3_int64)time(NULL));                                         // modified
            sqlite3_bind_text64(stmt, 2, hash.c_str(), hash.length(), SQLITE_STATIC, SQLITE_UTF8);          // hash
            sqlite3_bind_text64(stmt, 3, path_key.c_str(), path_key.length(), SQLITE_STATIC, SQLITE_UTF8);  // path key


            if ( sqlite3_step(stmt) != SQLITE_DONE )
            {
                std::printf("FilePersistence:: Error indexing file %s: %s\n", path.c_str(), sqlite3_errmsg(m_db));
                sqlite3_finalize(stmt);
                return FileIndexResult::ERROR;
            }

            std::printf("FilePersistence:: Indexed file %s\n", path.c_str());
            sqlite3_finalize(stmt);

            return FileIndexResult::OK;
        }

        DatabaseConnectionResult FilePersistence::open_connection()
        {
            if ( sqlite3_open("index.db", &m_db) != SQLITE_OK )
            {
                std::printf("FilePersistence:: Error connecting to database: %s\n", sqlite3_errmsg(m_db));
                return DatabaseConnectionResult::ERROR;
            }

            std::printf("FilePersistence:: Database connected.\n");
            return DatabaseConnectionResult::OK;
        }

        FileIndexResult FilePersistence::index_file(const std::string &path, bool force)
        {
            if ( !std::filesystem::exists(path) )
            {
                std::printf("FilePersistence:: File %s not found.\n", path.c_str());
                return FileIndexResult::NOT_FOUND;
            }

            bool reindex = false;

            if ( file_indexed(path) )
            {
                if ( !force )
                {
                    std::printf("FilePersistence:: File %s has already been indexed.\n", path.c_str());
                    return FileIndexResult::ALREADY_INDEXED;
                }
                else
                {
                    reindex = true;
                }
            }

            std::string hash = get_file_hash(path);
            if ( hash == "" )
            {
                std::printf("FilePersistence:: Error while generating BLAKE3 hash for file: %s\n", path.c_str());
                return FileIndexResult::ERROR;
            }

            if ( reindex )
            {
                return update_index(path, hash);
            }
            else
            {
                return apply_index(path, hash);
            }
        }

        bool FilePersistence::file_indexed(const std::string &path)
        {
            const std::string path_key = get_string_hash(path);
            const std::string expression = "SELECT EXISTS ( SELECT 1 FROM skyx_index WHERE path_key=? );";

            // too lazy to check for errors here
            sqlite3_stmt *stmt;
            sqlite3_prepare_v2(m_db, expression.c_str(), -1, &stmt, nullptr);
            sqlite3_bind_text(stmt, 1, path_key.c_str(), -1, SQLITE_STATIC);

            bool exists = false;
            if ( sqlite3_step(stmt) == SQLITE_ROW )
            {
                exists = sqlite3_column_int(stmt, 0) != 0;
            }
            sqlite3_finalize(stmt);

            return exists;
        }

        IndexValidationResult  FilePersistence::validate_index_tree() const
        {
            IndexValidationResult retval;
            const std::string expr = "SELECT path, hash FROM skyx_index";
            sqlite3_stmt *stmt;
            if ( sqlite3_prepare_v2(m_db, expr.c_str(), -1, &stmt, nullptr) != SQLITE_OK )
            {
                retval.success = false;
                return retval;
            }

            while ( sqlite3_step(stmt) == SQLITE_ROW )
            {
                auto path = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
                auto hash = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));

                auto val_result = validate_file(false, path, hash);
                retval.result.emplace_back(val_result);
            }

            retval.success = true;
            sqlite3_finalize(stmt);
            return retval;
        }

        bool FilePersistence::get_indexed_file(const std::string &path_key, FilePersistence::index_file_t *dest) const
        {
            if ( path_key == "" )
            {
                return false;
            }

            const std::string expr = "SELECT modified, path, hash FROM skyx_index where path_key=?";
            sqlite3_stmt *stmt;
            if ( sqlite3_prepare_v2(m_db, expr.c_str(), -1, &stmt, nullptr) != SQLITE_OK )
            {
                return false;
            }

            sqlite3_bind_text(stmt, 1, path_key.c_str(), -1, SQLITE_STATIC);

            bool result = true;
            if ( sqlite3_step(stmt) == SQLITE_ROW )
            {
                dest->ts = static_cast<uint64_t>(sqlite3_column_int64(stmt, 0));
                dest->path = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
                dest->hash = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));
            }
            else
            {
                result = false;
            }

            sqlite3_finalize(stmt);
            return result;
        }

        FileIndexStatus FilePersistence::validate_file(
            const bool check_index,
            const std::string &path,
            const std::string &hash) const
        {
            FileIndexStatus status;
            status.path = path;

            if ( !std::filesystem::exists(path) )
            {
                std::printf("FilePersistence:: Error validating %s: File not found.\n", path.c_str());
                status.status = FileStatus::LOST;
            }
            else
            {
                auto validate_hash = [&path, &status] (const std::string &hash)
                {
                    if ( get_file_hash(path) != hash )
                    {
                        std::printf("FilePersistence:: Error validating %s: Hash does not match index.\n", path.c_str());
                        status.status = FileStatus::CORRUPT;
                    }
                    else
                    {
                        status.status = FileStatus::INDEXED;
                    }
                };

                if ( check_index )
                {
                    index_file_t index;
                    if ( get_indexed_file(get_string_hash(path), &index) )
                    {
                        validate_hash(index.hash);
                    }
                    else
                    {
                        status.status = FileStatus::NOT_INDEXED;
                    }
                }
                else
                {
                    if ( hash != "" )
                    {
                        validate_hash(hash);
                    }
                    else
                    {
                        status.status = FileStatus::INVALID_HASH;
                    }
                }
            }

            return status;
        }

        FileIndexStatus FilePersistence::validate_index(const std::string &path) const
        {
            FileIndexStatus status;
            status.path = path;

            index_file_t index;
            if ( !get_indexed_file(get_string_hash(path), &index) )
            {
                status.status = FileStatus::NOT_INDEXED;
            }
            else
            {
                status = validate_file(false, path, index.hash);
            }

            return status;
        }
    }
}
