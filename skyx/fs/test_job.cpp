/*
 * File Name: test_job.cpp
 * Author: Elio Decolli (eliodecolli@gmail.com)
 * Last Modified: 20/09/2026
 * Purpose: Tests file slicing and transfer metadata storage.
 */

#include <assert.h>
#include <printf.h>
#include <format>

#include "job.h"

#define ASSERT(expr, msg)       \
if (!expr) { \
    printf("\033[31m Assert Error:\033[0m %s\n", msg); \
} \

void run_test_job() {
    {
        skyx::fs_file file;
        file.open("test.txt", 3, 1024);
        file.write(skyx::fs_cell { 3 }, skyx::fs_buf {'H', 'e', 'l'});
        file.write(skyx::fs_cell { 6 }, skyx::fs_buf { 'l', 'o', '.'});
        file.write(skyx::fs_cell { 12 }, skyx::fs_buf { 'T', 'e', 'e' });

        // file.write(fs_cell { 15 }, fs_buf { 'h', 'e', 'e' });

        ASSERT((std::filesystem::exists("test.txt")), "File was not created");
        ASSERT((std::filesystem::exists("test.txt__head.fsh")), "Header was not created");
    }

    //try to load it again
    {
        skyx::fs_file f2;
        f2.open("test.txt");
        skyx::fs_buf buf;
        f2.slice(3, &buf);

        std::string h;
        for (const auto &t : buf) h += t;
        ASSERT((h == "Hel"), std::format("Content not present: {}", h).c_str());

        f2.print_header();
    }
}


int main() {
    printf("Running tests...\n");
    run_test_job();

    return 0;
}
