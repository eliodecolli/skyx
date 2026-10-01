/*
 * File Name: job.h
 * Author: Elio Decolli (eliodecolli@gmail.com)
 * Last Modified: 20/09/2026
 * Purpose: Declares file slicing and transfer job storage types.
 */

#pragma once

#include <vector>
#include <string>
#include <fstream>
#include <mutex>
#include <cstdint>

namespace skyx
{

    using fs_buf = std::vector<uint8_t>;



    #ifdef SKYX__TEST
    #include <printf.h>
    #define TEST_LOG(...)        printf("%s\n", __VA_ARGS__)
    #else
    #define TEST_LOG(...)        ((void)0)
    #endif


    struct fs_cell {
        uint64_t offset;

        fs_cell (uint64_t o) : offset{o} {}
    };

    class fs_job_header {
    private:
        std::vector< fs_cell >      m_cells;
        uint64_t                    len;
        std::string                 file_name;
        std::fstream                m_stream;

    public:
        uint64_t                      cell_size;

    public:
        std::vector< fs_cell >      get_missing_cells();
        void                        save();
        bool                        read();
        void                        add_cell(fs_cell c);
        bool                        has_slice(uint64_t offset);
        void                        configure(std::string f_name, uint64_t cell_size, uint64_t f_len);
        void                        configure(std::string f_name);
        uint64_t                    get_current_slice_count();

    #ifdef SKYX__TEST
        inline void                  print_header() {
            printf("=== HEADER ===\n");
            printf("Cell Count: %zu\n", m_cells.size());
            printf("Allocated Length: %d\n", len);
            printf("Cell Size: %d\n", cell_size);
            printf("==============\n");
        }
    #endif
    };

    class fs_file {
    private:
        fs_job_header   m_header;
        std::fstream    m_stream;
        std::mutex      M;

    private:
        inline static std::string get_header_name(std::string file);

    private:
        void            init_stream(char *file, uint64_t f_len);
        void            init_header(char *file, uint64_t cell_size, uint64_t f_len);

    public:
        void            open(char *file, uint64_t cell_size, uint64_t f_len);
        void            open(char *file);
        void            write(fs_cell c, fs_buf buf);
        void            slice(uint64_t offset, fs_buf *buf);

        uint64_t        get_current_slice_count();

        fs_file() = default;
        ~fs_file() = default;

    #ifdef SKYX__TEST
        inline void     print_header() {
            m_header.print_header();
        }
    #else
        void            print_header();
    #endif
    };
}
