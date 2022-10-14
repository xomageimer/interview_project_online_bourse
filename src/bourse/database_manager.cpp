#include "database_manager.h"

core::DataBaseManager::DataBaseManager(const std::string &connection_string)  : connection_database_(
        connection_string.c_str()) {}

pqxx::result core::DataBaseManager::MakeTransaction(const std::string &transaction) {
    if (!worker_)
        worker_.emplace(connection_database_);
    return worker_->exec(transaction);
}

pqxx::row core::DataBaseManager::MakeTransaction1(const std::string &transaction)  {
    if (!worker_)
        worker_.emplace(connection_database_);
    return worker_->exec1(transaction);
}

void core::DataBaseManager::CommitTransactions()  {
    std::lock_guard<std::mutex> lock(mut_);
    worker_->commit();
    worker_.reset();
}
