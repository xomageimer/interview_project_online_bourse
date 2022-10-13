#include <iostream>
#include <thread>

#include "network/client.h"

int main(int argc, char *argv[]) {
    try {
        if (argc != 3) {
            std::cerr << "Usage: bourse_client <host> <port>\n";
            return 1;
        }

        boost::asio::io_context io_context;

        boost::asio::ip::tcp::resolver resolver(io_context);
        boost::asio::ip::tcp::resolver::results_type endpoints =
                resolver.resolve(argv[1], argv[2]);

        boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
        ctx.load_verify_file("server_certificate.crt");

        network::Client c(io_context, ctx, endpoints);

        std::thread t([&io_context]() { io_context.run(); });

        std::string msg {};
        while (std::cin >> msg && msg != "EXIT") {
            c.write(core::makeRequest(msg));
        }

        c.close();
        t.join();

    } catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
}