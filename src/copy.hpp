#ifndef SRC_CPP_COPY_HPP_
#define SRC_CPP_COPY_HPP_

#include <string>

#include "./disk.hpp"
#include "./threading.hpp"

#include "thread_pool.hpp"

extern thread_pool pool;
void copy_buffered(std::string infile, std::string outfile, std::error_code& ec) {
    try {
        fs::remove(outfile, ec);
        if (ec.value() != 0)
            return;

        FileDisk in(infile, 0);
        FileDisk out(outfile);

        uint64_t size = fs::file_size(infile);

        const uint64_t BUF_SIZE = 8*1024*1024;
        auto buffer = std::make_unique<uint8_t[]>(BUF_SIZE);
        auto next_buffer = std::make_unique<uint8_t[]>(BUF_SIZE);

        uint64_t read_pos = 0;
        uint64_t write_pos = 0;

        std::future<bool> reader;
        auto startRead = [&reader, &read_pos, &in, &next_buffer, size, BUF_SIZE] {
            if (read_pos < size) {
                uint64_t buf_size = std::min(BUF_SIZE, size - read_pos);

                reader = pool.submit([&in, buf = next_buffer.get(), read_pos, buf_size] {
                    in.Read(read_pos, buf, buf_size);
                });
                read_pos += buf_size;
            }
        };

        startRead();
        while (write_pos < size) {
            reader.wait();
            buffer.swap(next_buffer);
            startRead();

            uint64_t buf_size = std::min(BUF_SIZE, size - write_pos);
            out.Write(write_pos, buffer.get(), buf_size);
            write_pos += buf_size;
        }
    } catch (...) {
        ec = std::make_error_code(std::errc::bad_message);
    }
}

#endif