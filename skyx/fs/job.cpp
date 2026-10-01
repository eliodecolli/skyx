/*
 * File Name: job.cpp
 * Author: Elio Decolli (eliodecolli@gmail.com)
 * Last Modified: 20/09/2026
 * Purpose: Implements persistent file slicing and transfer metadata.
 */

#include <job.h>
#include <assert.h>
#include <printf.h>
#include <format>
#include <algorithm>
#include <filesystem>

namespace skyx
{
    #define OPEN_MODE__INITIAL              (std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc)
    #define OPEN_MODE__EXISTING             (std::ios::binary | std::ios::in | std::ios::out)


    // helper methods

    template<typename T>
    void read_stream(std::fstream &s, T *dest) {
        s.read(reinterpret_cast<char*>(dest), sizeof(*dest));
    }

    template<typename T>
    void write_stream(std::fstream &s, const T &val) {
        s.write(reinterpret_cast<const char*>(&val), sizeof(val));
    }

    // actual implementation

    std::string skyx::fs_file::get_header_name(std::string file_name) {
        return std::format("{}__head.fsh", file_name);
    }

    void skyx::fs_file::init_stream(char *file, uint64_t f_len) {
        std::scoped_lock _(M);   // eh, let's me cautious

        if ( !std::filesystem::exists(file) && f_len > 0 ) {
            m_stream.open(file, OPEN_MODE__INITIAL);
            m_stream.seekp(f_len - 1);
            m_stream.put('\0');
        }
        else {
            m_stream.open(file, OPEN_MODE__EXISTING);
        }
    }

    uint64_t skyx::fs_file::get_current_slice_count() {
        return m_header.get_current_slice_count();
    }

    void skyx::fs_file::init_header(char *file, uint64_t cell_size, uint64_t f_len) {
        auto h_name = get_header_name(file);
        if ( cell_size > 0 && f_len > 0 ) {
            m_header.configure(h_name, cell_size, f_len);
        }
        else {
            m_header.configure(h_name);
        }

        if ( !m_header.read() ) {
            // empty header file, first dump the current info that we have
            TEST_LOG("Header file was empty");
            m_header.save();
        }
    }

    void skyx::fs_file::open(char *file, uint64_t cell_size, uint64_t f_len) {
        init_stream(file, f_len);
        init_header(file, cell_size, f_len);
    }

    void skyx::fs_file::open(char *file) {
        init_stream(file, 0);
        init_header(file, 0, 0);
    }

    void skyx::fs_file::write(fs_cell cell, fs_buf buf) {
        if ( cell.offset % m_header.cell_size != 0 ) {
            // TODO: Throw
            return;
        }

        std::scoped_lock _(M);

        m_stream.seekp(cell.offset, std::ios::beg);
        m_stream.write((char*)buf.data(), buf.size());

        // update the metadata
        m_header.add_cell(cell);
        m_header.save();
    }

    void skyx::fs_file::slice(uint64_t offset, fs_buf *buf) {
        std::scoped_lock _(M);

        if ( !m_header.has_slice(offset) ) {
            TEST_LOG(std::format("Slice {} not present", offset).c_str());
            return;
        }

        // get to the fucking slice
        m_stream.seekg(offset, m_stream.beg);
        for ( uint64_t i = 0; i < m_header.cell_size; i++ ) {
            uint8_t c;
            m_stream.read(reinterpret_cast<char*>(&c), sizeof(uint8_t));
            buf->emplace_back(c);
        }

        // get back at the beginning
        m_stream.seekg(0);
    }

    uint64_t skyx::fs_job_header::get_current_slice_count() {
        return m_cells.size();
    }

    void skyx::fs_job_header::configure(std::string f_name) {
        if ( !std::filesystem::exists(f_name) ) {
            m_stream.open(f_name, OPEN_MODE__INITIAL);
            TEST_LOG("Open header as NEW file");
        }
        else {
            TEST_LOG("Open header as EXISTING file");
            m_stream.open(f_name, OPEN_MODE__EXISTING);
        }
    }

    void skyx::fs_job_header::configure(std::string f_name, uint64_t cell_size, uint64_t f_len) {
        this->cell_size = cell_size;
        this->len = f_len;
        this->file_name = f_name;

        this->configure(f_name);
    }

    void skyx::fs_job_header::add_cell(fs_cell cell) {
        m_cells.emplace_back(cell);
    }

    bool skyx::fs_job_header::has_slice(uint64_t offset) {
        return std::any_of(m_cells.cbegin(), m_cells.cend(), [offset] (const fs_cell &i) {
            return i.offset == offset;
        });
    }

    void skyx::fs_job_header::save() {
        // format the current header:
        // -> Completed File Length (uint64_t)
        // -> Cell Size (uint64_t)
        // -> Total Cell Count (uint64_t)
        //  -> Cell Data (uint64_t for the moment)
        m_stream.clear();
        m_stream.seekp(0);

        write_stream(m_stream, len);
        write_stream(m_stream, cell_size);
        write_stream(m_stream, m_cells.size());
        for ( auto &c : m_cells ) {
            m_stream.write(reinterpret_cast<const char*>(&c.offset), sizeof(uint64_t));
        }
    }

    bool skyx::fs_job_header::read() {
        if ( m_stream.peek() == std::ifstream::traits_type::eof() ) {
            // header is empty
            m_stream.clear();
            return false;
        }

        m_cells.clear();
        m_stream.seekg(0);

        // read the total len of the file
        read_stream(m_stream, &len);

        // read the cell size
        read_stream(m_stream, &cell_size);

        // read the total number of cells
        uint64_t total_cells;
        read_stream(m_stream, &total_cells);

        // now update each cell
        for ( uint64_t i = 0; i < total_cells; i++ ) {
            uint64_t c_offset;
            read_stream(m_stream, &c_offset);
            m_cells.emplace_back(fs_cell { c_offset });
        }

        return true;
    }

    std::vector<fs_cell> skyx::fs_job_header::get_missing_cells() {
        std::vector<fs_cell> retval;
        uint64_t l_offset = 0;
        for ( auto &c : m_cells ) {
            if ( c.offset != l_offset ) {
                retval.emplace_back(fs_cell { l_offset });
            }

            l_offset += cell_size;
        }

        return retval;
    }
}
