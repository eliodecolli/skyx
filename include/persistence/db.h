#pragma once

#include <cstdint>
#include <string>
#include <sqlite3.h>
#include <vector>

namespace skyx
{
    namespace persistence
    {
        enum class FileStatus : uint8_t
        {
            INDEXED,
            LOST,
            CORRUPT,
            NOT_INDEXED,
            INVALID_HASH,
        };

        enum class FileIndexResult : uint8_t
        {
            OK,
            NOT_FOUND,
            ALREADY_INDEXED,
            ERROR,
        };

        enum class DatabaseConnectionResult : uint8_t
        {
            OK,
            ERROR
        };

        struct FileIndexStatus
        {
            std::string     path;
            FileStatus      status;
        };

        struct IndexValidationResult
        {
            bool                            success;
            std::vector<FileIndexStatus>    result;
        };

        class FilePersistence
        {
          private:
              sqlite3   *m_db;

          private:
              bool              file_indexed(const std::string &path);
              FileIndexResult   apply_index(const std::string &path, const std::string &hash);
              FileIndexResult   update_index(const std::string &path, const std::string &hash);
              FileIndexStatus   validate_file(const bool check_index, const std::string &path, const std::string &hash) const;

          private:
              struct index_file_t
              {
                  uint64_t ts;
                  std::string path;
                  std::string hash;
              };
              bool              get_indexed_file(const std::string &path_key, index_file_t *dest) const;

          public:
              DatabaseConnectionResult      open_connection();
              FileIndexResult               index_file(const std::string &path, bool force);
              FileIndexResult               remove_index(const std::string &path);
              IndexValidationResult         validate_index_tree() const;
              FileIndexStatus               validate_index(const std::string &path) const;

          public:
              ~FilePersistence();
        };
    }
}
