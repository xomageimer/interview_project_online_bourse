#include <gtest/gtest.h>

#include "network/server.h"

#include "utils/auxiliary.h"

#include "bourse/database_manager.h"
#include "bourse/request.h"
#include "bourse/response.h"

static std::shared_ptr<core::DataBaseManager> db_manager;
static std::unordered_map<int, std::weak_ptr<network::ISession>> active_sessions;
static std::unordered_map<std::string, int> active_users;

struct MockSession : public network::ISession {
    void addResponsesQueue(std::shared_ptr<core::IResponse>){
        return;
    }
};

void clearTable(const std::string& table_name) {
    db_manager->MakeTransaction("TRUNCATE TABLE " + table_name + " CASCADE");
    db_manager->CommitTransactions();
}

void refreshTableId(const std::string& table_name){
    db_manager->MakeTransaction("ALTER SEQUENCE " + table_name + "_id_seq RESTART WITH 1");
    db_manager->CommitTransactions();
}

void clearAllTables(){
    clearTable("deal_history");
    clearTable("requests");
    clearTable("data");

    refreshTableId("deal_history");
    refreshTableId("requests");
    refreshTableId("data");
}

TEST (bourse, test1_login_attempt) {
    clearAllTables();

    auto test_session = std::make_shared<MockSession>();
    auto req = std::make_shared<core::AuthorizationRequest>("test_name", "12345");
    auto resp = network::execRequest(req->getJson(),
                                     test_session,
                                     db_manager,
                                     active_sessions,
                                     active_users);

    EXPECT_TRUE(std::dynamic_pointer_cast<core::BadResponse>(resp.front()) != nullptr);
    EXPECT_EQ(unquoted(resp.front()->getJson()["message"].get<std::string>()), std::string("wrong password or name"));
}

TEST (bourse, test2_register) {
    auto test_session = std::make_shared<MockSession>();
    auto req = std::make_shared<core::RegistrationRequest>("test_name", "12345");
    auto resp = network::execRequest(req->getJson(),
                         test_session,
                         db_manager,
                         active_sessions,
                         active_users);
    EXPECT_TRUE(std::dynamic_pointer_cast<core::AccessAuthResponse>(resp.front()) != nullptr);
    EXPECT_EQ(active_users.count(unquoted(resp.front()->getJson()["user_id"].get<std::string>())), 1);

    resp = network::execRequest(req->getJson(),
                        test_session,
                        db_manager,
                        active_sessions,
                        active_users);
    EXPECT_TRUE(std::dynamic_pointer_cast<core::BadResponse>(resp.front()) != nullptr);
    EXPECT_EQ(unquoted(resp.front()->getJson()["message"].get<std::string>()), std::string("name occupied"));
}

TEST (bourse, test3_unloggin_request) {
    auto test_session = std::make_shared<MockSession>();
    auto req = std::make_shared<core::BalanceRequest>("test_name");
    auto resp = network::execRequest(req->getJson(),
                                     test_session,
                                     db_manager,
                                     active_sessions,
                                     active_users);
    EXPECT_TRUE(std::dynamic_pointer_cast<core::BadResponse>(resp.front()) != nullptr);
    EXPECT_EQ(unquoted(resp.front()->getJson()["message"].get<std::string>()), std::string("not authorized"));
}

std::string registerNewUser(const std::string& name, const std::string & password){
    auto test_session = std::make_shared<MockSession>();
    std::shared_ptr<core::IRequest> req = std::make_shared<core::RegistrationRequest>(name, password);
    auto resp = network::execRequest(req->getJson(),
                                     test_session,
                                     db_manager,
                                     active_sessions,
                                     active_users);
    return unquoted(resp.front()->getJson()["user_id"].get<std::string>());
}

TEST (bourse, test4_lot_operations){
    registerNewUser("test_user_2", "qwerty555");
    EXPECT_EQ(active_users.size(), 2);

    std::vector<std::pair<int, int>> lots {{10, 62}, {20, 63}};
    size_t i = 0;
    for (auto & [hash_id, id] : active_users) {
        auto req = std::make_shared<core::CreateLotRequest>(hash_id, lots[i].first, lots[i].second,
                                                       core::CreateLotRequest::OperationType::BUY_USD);
        auto test_session = std::make_shared<MockSession>();
        test_session->getUserId() = hash_id;
        auto resp = network::execRequest(req->getJson(),
                                    test_session,
                                    db_manager,
                                    active_sessions,
                                    active_users);
        EXPECT_TRUE(std::dynamic_pointer_cast<core::SuccessReqResponse>(resp.front()) != nullptr);
        EXPECT_EQ(resp.front()->getJson()["lot_id"].get<int>(), ++i);
    }

    auto user_id = registerNewUser("test_user_3", "iamracoon");
    EXPECT_EQ(active_users.size(), 3);

    auto req = std::make_shared<core::CreateLotRequest>(user_id, 50, 61,
                                                        core::CreateLotRequest::OperationType::SELL_USD);
    auto test_session = std::make_shared<MockSession>();
    test_session->getUserId() = user_id;
    auto resp = network::execRequest(req->getJson(),
                                     test_session,
                                     db_manager,
                                     active_sessions,
                                     active_users);
    EXPECT_TRUE(std::dynamic_pointer_cast<core::SuccessReqResponse>(resp.front()) != nullptr);
    EXPECT_EQ(resp.front()->getJson()["lot_id"].get<int>(), ++i);
}

int main(int argc, char **argv) {
    std::cout << argc << std::endl;

    if (argc < 6) {
        std::cerr << "Usage: tests <db_host> <db_port> <db_name> <db_user> <db_password> optional:<gtest args...>\n";
        return 1;
    }

    std::string connection_string = "host=" + std::string(argv[1]) + " port=" + std::string(argv[2]) +
                                    " dbname=" + std::string(argv[3]) + " user=" + std::string(argv[4]) + " password=" + std::string(argv[5]);
    db_manager = std::make_shared<core::DataBaseManager>(connection_string);

    int new_argc = argc - 6;
    if (new_argc)
        testing::InitGoogleTest(&new_argc, &argv[argc]);
    else testing::InitGoogleTest();
    return RUN_ALL_TESTS();
}