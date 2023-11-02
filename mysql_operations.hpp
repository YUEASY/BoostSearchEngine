#ifndef MYSQL_OPERATIONS_HPP
#define MYSQL_OPERATIONS_HPP

#include "util.hpp"
#include "md5.h"
#include <cstring>
#include <mutex>

namespace ns_operation
{
    class TableBase
    {
    protected:
        MYSQL *mysql; // 一个对象就是一个客户端，管理一张表
        std::mutex mtx;

        const char *HOST = "172.17.0.2";
        const char *PORT = "3306";
        const char *USER = "root";
        const char *PASSWD = "123456";
        const char *DB_NAME = "search_engine";

    public:
        // 初始化
        TableBase()
        {
            mysql = ns_util::SQLUtil::MysqlInit(DB_NAME, HOST, PORT, USER, PASSWD);
            if (mysql == nullptr)
            {
                exit(-1);
            }
        }
        ~TableBase()
        {
            ns_util::SQLUtil::MysqlDestroy(mysql);
        }

        virtual bool Insert(const Json::Value &root) = 0;
        virtual bool Update(const string &id, const Json::Value &root) = 0;
        virtual bool Delete(const string &id) = 0;
        virtual bool SelectAll(Json::Value &roots) = 0;
        virtual bool SelectOne(const string &id, Json::Value &root) = 0;
        virtual bool Clear() = 0;
    };

    class TableDoc : public TableBase
    {
        // doc_info
        // +---------+--------------+------+-----+---------+----------------+
        // | Field   | Type         | Null | Key | Default | Extra          |
        // +---------+--------------+------+-----+---------+----------------+
        // | id      | int(11)      | NO   | PRI | NULL    | auto_increment |
        // | doc_id  | int(11)      | NO   |     | NULL    |                |
        // | title   | varchar(256) | YES  |     | NULL    |                |
        // | content | text         | YES  |     | NULL    |                |
        // | url     | varchar(256) | YES  |     | NULL    |                |
        // | path    | varchar(256) | YES  |     | NULL    |                | // 预留
        // +---------+--------------+------+-----+---------+----------------+
    public:
        bool Insert(const Json::Value &doc)
        {
            std::string sql;
            // sql.append("insert ignore into doc_info(title, content, url) values(");
            // sql.append("\'" + doc["title"].asString() + "\', ");
            // sql.append("\'" + doc["content"].asString() + "\', ");
            // sql.append("\'" + doc["url"].asString() + "\');");

            sql.append("insert ignore into doc_info(doc_id,title, content, url) values(");
            sql.append("\'" + doc["doc_id"].asString() + "\', ");
            sql.append("\'" + doc["title"].asString() + "\', ");
            sql.append("\'" + doc["content"].asString() + "\', ");
            sql.append("\'" + doc["url"].asString() + "\');");

            return ns_util::SQLUtil::MysqlQuery(mysql, sql);
        }
        bool Update(const string &url, const Json::Value &doc)
        {
            std::string sql;
            sql.append("update doc_info set ");

            // 拼接需要更新的字段
            if (!doc["title"].isNull())
            {
                sql.append("title =\'");
                sql.append(doc["title"].asString());
                sql.append("\', ");
            }
            if (!doc["content"].isNull())
            {
                sql.append("content =\'");
                sql.append(doc["content"].asString());
                sql.append("\', ");
            }
            if (!doc["url"].isNull())
            {
                sql.append("url =\'");
                sql.append(doc["url"].asString());
                sql.append("\', ");
            }

            // 去掉 SQL 语句末尾的逗号和空格
            sql.erase(sql.end() - 2, sql.end());

            // 拼接 WHERE 子句
            sql.append(" where url =\'");
            sql.append(url);
            sql.append("\';");

            return ns_util::SQLUtil::MysqlQuery(mysql, sql);
        }
        bool Delete(const string &url)
        {
            std::string sql;
            sql.append("delete from doc_info where url =\'");
            sql.append(url);
            sql.append("\';");
            return ns_util::SQLUtil::MysqlQuery(mysql, sql);
        }
        bool SelectAll(Json::Value &docs)
        {
            std::string sql = "select * from doc_info;";
            mtx.lock();
            if (!ns_util::SQLUtil::MysqlQuery(mysql, sql))
            {
                mtx.unlock();
                return false;
            }
            MYSQL_RES *res = mysql_store_result(mysql);
            if (res == nullptr)
            {
                LOG(FATAL, "mysql store result error: " + std::string(mysql_error(mysql)));
                mtx.unlock();
                return false;
            }
            mtx.unlock();
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                Json::Value doc;
                doc["doc_id"] = std::stoi(row[1]);
                doc["title"] = row[2];
                doc["content"] = row[3];
                doc["url"] = row[4];
                // doc["path"] = row[4];
                docs.append(doc);
            }
            mysql_free_result(res);
            return true;
        }
        bool SelectOne(const string &url, Json::Value &doc)
        {
            std::string sql = "select * from doc_info where url=\'";
            sql.append(url);
            sql.append("\';");
            mtx.lock();
            if (!ns_util::SQLUtil::MysqlQuery(mysql, sql))
            {
                mtx.unlock();
                return false;
            }
            MYSQL_RES *res = mysql_store_result(mysql);
            if (res == nullptr)
            {
                LOG(FATAL, "mysql store result error: " + std::string(mysql_error(mysql)));
                mtx.unlock();
                return false;
            }
            mtx.unlock();
            if (mysql_num_rows(res) != 1)
            {
                LOG(NOTICE, url + string(" does not exist!"));
                mysql_free_result(res);
                return false;
            }

            MYSQL_ROW row = mysql_fetch_row(res);
            doc["id"] = std::stoi(row[0]);
            doc["title"] = row[1];
            doc["content"] = row[2];
            doc["url"] = row[3];
            // doc["path"] = row[4];

            mysql_free_result(res);
            return true;
        }
        bool Clear()
        {
            std::string sql = "truncate table doc_info;";
            return ns_util::SQLUtil::MysqlQuery(mysql, sql);
        }
    };

    class TableUser : public TableBase
    {
        // user_info
        // + -- -- -- -- -- --+-- -- -- -- -- +-- -- +-- --+-- -- -- -- -- -- -- -- +-- -- -- -- -- -+
        // | Field            | Type          | Null | Key | Default                | Extra          |
        // +-- -- -- -- -- -- +-- -- -- -- -- +-- -- +-- --+-- -- -- -- -- -- -- -- +-- -- -- -- -- -+
        // | id               | int(11)       | NO   | PRI | NULL                   | auto_increment |
        // | account          | int(11)       | NO   |     | NULL                   |                |
        // | password         | varchar(256)  | NO   |     | 123456                 |                |
        // | name             | varchar(256)  | NO   |     | NULL                   |                |
        // | email            | varchar(256)  | NO   |     | NULL                   |                |
        // | avatar           | varchar(1024) | NO   |     | / assets / default.png |                |
        // | create_date      | datetime      | YES  |     | NULL                   |                |
        // | permission_level | tinyint(4)    | NO   |     | 0                      |                |
        // | is_vip           | tinyint(4)    | NO   |     | 0                      |                |
        // | is_delete        | tinyint(4)    | NO   |     | 0                      |                |
        // + -- -- -- -- -- --+-- -- -- -- -- +-- -- +-- --+-- -- -- -- -- -- -- -- +-- -- -- -- -- -+

    public:
        bool Insert(const Json::Value &user)
        {
            std::string sql;
            sql.append("insert into user_info(account, password, name, email, avatar, permission_level, is_vip, is_delete) values(");
            sql.append("\'" + user["account"].asString() + "\', ");
            sql.append("\'" + MD5(user["password"].asString()).toStr() + "\', ");
            sql.append("\'" + user["name"].asString() + "\', ");
            sql.append("\'" + user["email"].asString() + "\', ");
            sql.append("\'" + user["avatar"].asString() + "\', ");
            sql.append("\'" + user["permission_level"].asString() + "\', ");
            sql.append("\'" + user["is_vip"].asString() + "\', ");
            sql.append("\'" + user["is_delete"].asString() + "\');");

            return ns_util::SQLUtil::MysqlQuery(mysql, sql);
        }
        bool Update(const string &account, const Json::Value &user)
        {
            std::string sql;
            sql.append("update user_info set ");

            // 拼接需要更新的字段
            if (!user["password"].isNull())
            {
                sql.append("password =\'");
                sql.append(user["password"].asString());
                sql.append("\', ");
            }
            if (!user["name"].isNull())
            {
                sql.append("name =\'");
                sql.append(user["name"].asString());
                sql.append("\', ");
            }
            if (!user["email"].isNull())
            {
                sql.append("email =\'");
                sql.append(user["email"].asString());
                sql.append("\', ");
            }
            if (!user["avatar"].isNull())
            {
                sql.append("avatar =\'");
                sql.append(user["avatar"].asString());
                sql.append("\', ");
            }
            if (!user["permission_level"].isNull())
            {
                sql.append("permission_level =\'");
                sql.append(user["permission_level"].asString());
                sql.append("\', ");
            }
            if (!user["is_vip"].isNull())
            {
                sql.append("is_vip =\'");
                sql.append(user["is_vip"].asString());
                sql.append("\', ");
            }
            if (!user["is_delete"].isNull())
            {
                sql.append("is_delete =\'");
                sql.append(user["is_delete"].asString());
                sql.append("\', ");
            }

            // 去掉 SQL 语句末尾的逗号和空格
            sql.erase(sql.end() - 2, sql.end());

            // 拼接 WHERE 子句
            sql.append(" where account =\'");
            // sql.append(user["account"].asString());
            sql.append(account);
            sql.append("\';");

            return ns_util::SQLUtil::MysqlQuery(mysql, sql);
        }
        bool Delete(const string &account)
        {
            std::string sql;
            sql.append("update user_info set is_delete =\'1\' where account =\'");
            sql.append(account);
            sql.append("\';");
            return ns_util::SQLUtil::MysqlQuery(mysql, sql);
        }
        bool SelectAll(Json::Value &users)
        {
            std::string sql = "select * from user_info;";
            mtx.lock();
            if (!ns_util::SQLUtil::MysqlQuery(mysql, sql))
            {
                mtx.unlock();
                return false;
            }
            MYSQL_RES *res = mysql_store_result(mysql);
            if (res == nullptr)
            {
                LOG(FATAL, "mysql store result error: " + std::string(mysql_error(mysql)));
                mtx.unlock();
                return false;
            }
            mtx.unlock();
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                Json::Value user;
                user["id"] = std::stoi(row[0]);
                user["account"] = std::stoi(row[1]);
                user["password"] = row[2];
                user["name"] = row[3];
                user["email"] = row[4];
                user["avatar"] = row[5];
                user["create_date"] = row[6];
                user["permission_level"] = std::stoi(row[7]);
                user["is_vip"] = std::stoi(row[8]);
                user["is_delete"] = std::stoi(row[9]);
                users.append(user);
            }
            mysql_free_result(res);
            return true;
        }
        bool SelectOne(const string &account, Json::Value &user)
        {
            std::string sql = "select * from user_info where account=\'";
            sql.append(account);
            sql.append("\';");

            if (!ns_util::SQLUtil::MysqlQuery(mysql, sql))
            {
                mtx.unlock();
                return false;
            }
            MYSQL_RES *res = mysql_store_result(mysql);
            if (res == nullptr)
            {
                LOG(FATAL, "mysql store result error: " + std::string(mysql_error(mysql)));
                mtx.unlock();
                return false;
            }
            mtx.unlock();
            if (mysql_num_rows(res) != 1)
            {
                LOG(NOTICE, account + string(" does not exist!"));
                mysql_free_result(res);
                return false;
            }
            MYSQL_ROW row = mysql_fetch_row(res);
            user["id"] = std::stoi(row[0]);
            user["account"] = std::stoi(row[1]);
            // user["password"] = row[2];
            user["name"] = row[3];
            user["email"] = row[4];
            user["avatar"] = row[5];
            user["create_date"] = row[6];
            user["permission_level"] = std::stoi(row[7]);
            user["is_vip"] = std::stoi(row[8]);
            user["is_delete"] = std::stoi(row[9]);

            mysql_free_result(res);
            return true;
        }
        // 弃用
        bool Clear() override
        {
            return false;
        }
        // 登录验证
        bool ValidateLogin(const std::string &account, const std::string &password)
        {
            std::string sql = "select password,is_delete from user_info where account=\'";
            sql.append(account);
            sql.append("\';");
            mtx.lock();
            if (!ns_util::SQLUtil::MysqlQuery(mysql, sql))
            {
                mtx.unlock();
                return false;
            }

            // 保存查询结果
            MYSQL_RES *res = mysql_store_result(mysql);
            if (res == nullptr)
            {
                LOG(FATAL, "mysql store result error: " + std::string(mysql_error(mysql)));
                mtx.unlock();
                return false;
            }
            mtx.unlock();
            if (mysql_num_rows(res) != 1)
            {
                LOG(NOTICE, "account: " + account + " does not exist!");
                mysql_free_result(res);
                return false;
            }

            MYSQL_ROW row = mysql_fetch_row(res);
            if (row[1][0] == '1' || password != string(row[0]))
            {
                mysql_free_result(res);
                LOG(NOTICE, "account: " + account + " wrong password!");
                return false;
            }
            mysql_free_result(res);
            return true;
        }
        // vip更新
        bool ModifyVip(string &account, string &level)
        {
            std::string sql;
            sql.append("update user_info set is_vip =\'");
            sql.append(level);
            sql.append("\' where account =\'");
            sql.append(account);
            sql.append("\';");
            return ns_util::SQLUtil::MysqlQuery(mysql, sql);
        }
    };

    class TableInverted : public TableBase
    {
        // inverted_elem
        // +--------+--------------+------+-----+---------+-------+
        // | Field  | Type         | Null | Key | Default | Extra |
        // +--------+--------------+------+-----+---------+-------+
        // | doc_id | int(11)      | NO   | MUL | NULL    |       |
        // | word   | varchar(256) | NO   |     | NULL    |       |
        // | weight | int(11)      | NO   |     | NULL    |       |
        // +--------+--------------+------+-----+---------+-------+

    public:
        bool Insert(const Json::Value &elem)
        {
            std::string sql;
            sql.append("insert ignore into inverted_elem(doc_id, word, weight) values(");
            sql.append("\'" + elem["doc_id"].asString() + "\', ");
            sql.append("\'" + elem["word"].asString() + "\', ");
            sql.append("\'" + elem["weight"].asString() + "\');");

            return ns_util::SQLUtil::MysqlQuery(mysql, sql);
        }
        bool Update(const string &doc_id, const Json::Value &elem)
        {
            std::string sql;
            sql.append("update inverted_elem set ");

            // 拼接需要更新的字段
            if (!elem["title"].isNull())
            {
                sql.append("title =\'");
                sql.append(elem["title"].asString());
                sql.append("\', ");
            }
            if (!elem["content"].isNull())
            {
                sql.append("content =\'");
                sql.append(elem["content"].asString());
                sql.append("\', ");
            }
            if (!elem["url"].isNull())
            {
                sql.append("url =\'");
                sql.append(elem["url"].asString());
                sql.append("\', ");
            }

            // 去掉 SQL 语句末尾的逗号和空格
            sql.erase(sql.end() - 2, sql.end());

            // 拼接 WHERE 子句
            sql.append(" where doc_id =\'");
            sql.append(doc_id);
            sql.append("\';");

            return ns_util::SQLUtil::MysqlQuery(mysql, sql);
        }
        bool Delete(const string &doc_id)
        {
            std::string sql;
            sql.append("delete from inverted_elem where doc_id =\'");
            sql.append(doc_id);
            sql.append("\';");
            return ns_util::SQLUtil::MysqlQuery(mysql, sql);
        }
        bool SelectAll(Json::Value &elems)
        {
            std::string sql = "select * from inverted_elem;";
            mtx.lock();
            if (!ns_util::SQLUtil::MysqlQuery(mysql, sql))
            {
                mtx.unlock();
                return false;
            }
            MYSQL_RES *res = mysql_store_result(mysql);
            if (res == nullptr)
            {
                LOG(FATAL, "mysql store result error: " + std::string(mysql_error(mysql)));
                mtx.unlock();
                return false;
            }
            mtx.unlock();
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                Json::Value elem;
                elem["doc_id"] = std::stoi(row[0]);
                elem["word"] = row[1];
                elem["weight"] = std::stoi(row[2]);
                elems.append(elem);
            }
            mysql_free_result(res);
            return true;
        }
        bool SelectByDoc(const string &doc_id, Json::Value &elems)
        {
            std::string sql = "select * from inverted_elem where doc_id =\'";
            sql.append(doc_id);
            sql.append("\';");
            mtx.lock();
            if (!ns_util::SQLUtil::MysqlQuery(mysql, sql))
            {
                mtx.unlock();
                return false;
            }
            MYSQL_RES *res = mysql_store_result(mysql);
            if (res == nullptr)
            {
                LOG(FATAL, "mysql store result error: " + std::string(mysql_error(mysql)));
                mtx.unlock();
                return false;
            }
            mtx.unlock();
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                Json::Value elem;
                elem["doc_id"] = std::stoi(row[0]);
                elem["word"] = row[1];
                elem["weight"] = row[2];
                elems.append(elem);
            }
            mysql_free_result(res);
            return true;
        }
        bool SelectByWord(const string &word, Json::Value &elems)
        {
            std::string sql = "select * from inverted_elem where word =\'";
            sql.append(word);
            sql.append("\';");
            mtx.lock();
            if (!ns_util::SQLUtil::MysqlQuery(mysql, sql))
            {
                mtx.unlock();
                return false;
            }
            MYSQL_RES *res = mysql_store_result(mysql);
            if (res == nullptr)
            {
                LOG(FATAL, "mysql store result error: " + std::string(mysql_error(mysql)));
                mtx.unlock();
                return false;
            }
            mtx.unlock();
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                Json::Value elem;
                elem["doc_id"] = std::stoi(row[0]);
                elem["word"] = row[1];
                elem["weight"] = row[2];
                elems.append(elem);
            }
            mysql_free_result(res);
            return true;
        }
        // 弃用
        bool SelectOne(const string &doc_id, Json::Value &elems) override
        {
            return false;
        }
        bool Clear()
        {
            std::string sql = "delete from inverted_elem;";
            return ns_util::SQLUtil::MysqlQuery(mysql, sql);
        }
    };

}

#endif // MYSQL_OPERATIONS_HPP