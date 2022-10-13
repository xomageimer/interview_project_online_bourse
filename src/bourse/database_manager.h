#ifndef BOURSE_DATABASE_MANAGER_H
#define BOURSE_DATABASE_MANAGER_H

#include <memory>
#include <string>
#include <mutex>

#include <pqxx/pqxx>

namespace core {
    struct DataBaseManager : public std::enable_shared_from_this<DataBaseManager> {
    public:
        explicit DataBaseManager(const std::string &connection_string) : connection_database_(
                connection_string.c_str()) {}

        auto Get() { return shared_from_this(); }

        pqxx::result MakeTransaction(std::string const &transaction) {
            if (!worker_)
                worker_.emplace(connection_database_);
            return worker_->exec(transaction);
        }

        pqxx::row MakeTransaction1(std::string const &transaction) {
            if (!worker_)
                worker_.emplace(connection_database_);
            return worker_->exec1(transaction);
        }

        void CommitTransactions() {
            std::lock_guard<std::mutex> lock(mut_);
            worker_->commit();
            worker_.reset();
        }

    private:
        pqxx::connection connection_database_;
        std::optional<pqxx::work> worker_;
        std::mutex mut_;
    };
}


#endif //BOURSE_DATABASE_MANAGER_H
