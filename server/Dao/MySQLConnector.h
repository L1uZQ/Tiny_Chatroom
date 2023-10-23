#include <iostream>
#include<string>
#include <mysql/mysql.h>
#include <unordered_map>
#include<json/json.h>
#include "../Util/MyTime.h"
#include "../config/mysql_config.h"
using namespace std;

//单例模式，数据库连接器
class MySQLConnector {
private:
    MYSQL *conn;
    static MySQLConnector *connector;
    void init();

public:
    static MySQLConnector *getMySQLConnector();

    pair<int, string> queryUser(const string& account, const string& password);

    bool queryUser(const string& account);

    bool insertUser(const string& account, const string& username, const string& password);

    ~MySQLConnector();
};
