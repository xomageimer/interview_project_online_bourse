#include "database_manager.h"

core::DataBaseManager::DataBaseManager(std::string const &owner_name, const std::string &connection_string)
        : owner_name_(owner_name), connection_database_(
        connection_string.c_str()) {
    createTables();
}

pqxx::result core::DataBaseManager::MakeTransaction(const std::string &transaction) {
    if (!worker_)
        worker_.emplace(connection_database_);
    return worker_->exec(transaction);
}

pqxx::row core::DataBaseManager::MakeTransaction1(const std::string &transaction) {
    if (!worker_)
        worker_.emplace(connection_database_);
    return worker_->exec1(transaction);
}

void core::DataBaseManager::CommitTransactions() {
    std::lock_guard<std::mutex> lock(mut_);
    worker_->commit();
    worker_.reset();
}

void core::DataBaseManager::createTables() {
    auto is_data_exists = MakeTransaction("SELECT EXISTS (\n"
                                          "   SELECT * FROM information_schema.tables\n"
                                          "   WHERE  table_schema = 'public'\n"
                                          "   AND    table_name   = 'data'\n"
                                          "   );");
    if (!is_data_exists[0][0].as<bool>()) {
        MakeTransaction("create table data\n"
                        "(\n"
                        "    id          serial  not null\n"
                        "        constraint data_pk\n"
                        "            primary key,\n"
                        "    user_name   varchar not null,\n"
                        "    password    varchar not null,\n"
                        "    usd_count   integer,\n"
                        "    rub_balance integer\n"
                        ");\n"
                        "\n"
                        "alter table data\n"
                        "    owner to " + owner_name_ + ";\n"
                                                        "\n"
                                                        "create unique index data_user_name_uindex\n"
                                                        "    on data (user_name);\n"
                                                        "\n"
        );
    }

    auto is_requests_exists = MakeTransaction("SELECT EXISTS (\n"
                                              "   SELECT * FROM information_schema.tables\n"
                                              "   WHERE  table_schema = 'public'\n"
                                              "   AND    table_name   = 'requests'\n"
                                              "   );");
    if (!is_requests_exists[0][0].as<bool>()) {
        MakeTransaction("create table requests\n"
                        "(\n"
                        "    id                serial  not null\n"
                        "        constraint requests_pk\n"
                        "            primary key,\n"
                        "    user_id           integer not null\n"
                        "        constraint requests_data_id_fk\n"
                        "            references data,\n"
                        "    type_of_operation varchar,\n"
                        "    usd_count         integer,\n"
                        "    rub_price         integer,\n"
                        "    active            boolean\n"
                        ");\n"
                        "\n"
                        "alter table requests\n"
                        "    owner to " + owner_name_ + ";\n"
                                                        "\n"
        );
    }

    auto is_deal_history_exists = MakeTransaction("SELECT EXISTS (\n"
                                                  "   SELECT * FROM information_schema.tables\n"
                                                  "   WHERE  table_schema = 'public'\n"
                                                  "   AND    table_name   = 'deal_history'\n"
                                                  "   );");
    if (!is_deal_history_exists[0][0].as<bool>()) {
        MakeTransaction("create table deal_history\n"
                        "(\n"
                        "    id         serial  not null\n"
                        "        constraint deal_history_pk\n"
                        "            primary key,\n"
                        "    buyer_id   integer not null\n"
                        "        constraint deal_history_data_id_fk\n"
                        "            references data,\n"
                        "    seller_id  integer not null\n"
                        "        constraint deal_history_data_id_fk_2\n"
                        "            references data,\n"
                        "    request_id integer not null\n"
                        "        constraint deal_history_requests_id_fk\n"
                        "            references requests,\n"
                        "    date       timestamp\n"
                        ");\n"
                        "\n"
                        "alter table deal_history\n"
                        "    owner to " + owner_name_ + ";\n"
                                                        "\n"
                                                        "create unique index deal_history_request_id_uindex\n"
                                                        "    on deal_history (request_id);\n"
                                                        "");
    }

    CommitTransactions();
}


