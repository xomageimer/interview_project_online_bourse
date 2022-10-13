#ifndef BOURSE_DATABASE_MANAGER_H
#define BOURSE_DATABASE_MANAGER_H

#include <memory>
#include <string>

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

        void CommitTransaction() {
            worker_->commit();
            worker_.reset();
        }

    private:
        pqxx::connection connection_database_;
        std::optional<pqxx::work> worker_;
    };
}


#endif //BOURSE_DATABASE_MANAGER_H
