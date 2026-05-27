#include <iostream>

#ifndef _WIN32
#define _WIN32_WINNT 0x0A00
#endif // _WIN32

#define ASIO_STANDALONE
#include <asio.hpp>
#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>

// #include "../include/RadishDB.h"

std::vector<char> buffer(20 * 1024);

void GetAllBytes(asio::ip::tcp::socket& socket) {
    socket.async_read_some(
        asio::buffer(buffer.data(), buffer.size()),
        [&socket](const std::error_code errorCode, const std::size_t length) {
            if (errorCode) return;

            std::cout << "\n\nRead: " << length << " bytes\n\n";
            for (std::size_t i{}; i < length; ++i) {
                std::cout << buffer[i];
            }

            GetAllBytes(socket);
        }
    );
}

int main() {
    asio::error_code errorCode;
    asio::io_context context;



    asio::ip::tcp::endpoint endpoint(asio::ip::make_address("51.38.81.49", errorCode), 80);
    asio::ip::tcp::socket socket(context);

    socket.connect(endpoint);

    if (errorCode) {
        std::cerr << "Failed to connect to server: " << errorCode.message() << std::endl;
        return 1;
    }

    if (socket.is_open()) {
        GetAllBytes(socket);

        std::string request =
            "GET /index.html HTTP/1.1\r\n"
            "Host: example.com\r\n"
            "Connection: close\r\n\r\n";

        socket.write_some(asio::buffer(request.data(), request.size()), errorCode);
    }

    return 0;
}