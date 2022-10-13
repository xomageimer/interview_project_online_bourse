#include <iostream>

#include "network/server.h"

#include <string>

int main(int argc, char* argv[])
{
    try
    {
        if (argc < 7)
        {
            std::cerr << "Usage: bourse_server <port> <db_host> <db_port> <db_name> <db_user> <db_password>\n";
            return 1;
        }

        boost::asio::io_context io_context;

        network::Server s(io_context, std::atoi(argv[1]));

        io_context.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }
}