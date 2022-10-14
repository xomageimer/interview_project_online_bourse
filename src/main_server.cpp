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

        std::string connection_string = "host=" + std::string(argv[2]) + " port=" + std::string(argv[3]) +
                " dbname=" + std::string(argv[4]) + " user=" + std::string(argv[5]) + " password=" + std::string(argv[6]);
        network::Server s(io_context, std::atoi(argv[1]), std::make_shared<core::DataBaseManager>(connection_string));

        io_context.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }
}