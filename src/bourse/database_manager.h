#ifndef BOURSE_DATABASE_MANAGER_H
#define BOURSE_DATABASE_MANAGER_H

#include <memory>
#include <string>
#include <mutex>

#include <pqxx/pqxx>

namespace core {
    struct DataBaseManager : public std::enable_shared_from_this<DataBaseManager> {
    public:
        explicit DataBaseManager(std::string const & owner_name, const std::string &connection_string);

        auto Get() { return shared_from_this(); }

        pqxx::result MakeTransaction(std::string const &transaction);

        pqxx::row MakeTransaction1(std::string const &transaction);

        void CommitTransactions();

    private:
        void createTables();

        std::string owner_name_;
        pqxx::connection connection_database_;
        std::optional<pqxx::work> worker_ = std::nullopt;
        std::mutex mut_;
    };
}


#endif //BOURSE_DATABASE_MANAGER_H
