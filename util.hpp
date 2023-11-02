#pragma once

#include <string>
#include <iostream>
#include <vector>
#include <fstream>
#include <bitset>
#include <sstream>
#include <mutex>
#include <regex>
#include <mysql/mysql.h>
#include <jsoncpp/json/json.h>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/replace.hpp>
#include "cppjieba/include/cppjieba/Jieba.hpp"
#include "log.hpp"

// 操作合集
namespace ns_util
{

    class FileUtil
    {
    public:
        static bool ReadFile(const std::string &file_path, std::string *out)
        {
            std::ifstream in(file_path.c_str(), std::ios::in);
            if (!in.is_open())
            {
                LOG(WARNING, "open file " + file_path + "error");
                return false;
            }

            std::string line;
            while (std::getline(in, line))
            {
                removeInvalidCharacters(line);
                *out += line;
            }
            in.close();
            return true;
        }

        static bool isUTF8ContinuationByte(char ch)
        {
            // 判断是否是UTF-8的继续字节（高位是10）
            return (ch & 0xC0) == 0x80;
        }

        static bool isUTF8StartByte(char ch)
        {
            // 判断是否是UTF-8的起始字节
            return (ch & 0x80) == 0;
        }

        static void removeInvalidCharacters(std::string &str)
        {
            std::string validChars;
            for (std::size_t i = 0; i < str.size(); ++i)
            {
                if (str[i] == '\\' || str[i] == '\'' || str[i] == '\"')
                {
                    validChars += '\\';
                }
                if (isUTF8StartByte(str[i]))
                {
                    // 当找到UTF-8的起始字节时，尝试提取整个UTF-8字符
                    int charLength = 1;
                    if ((str[i] & 0xE0) == 0xC0)
                    {
                        charLength = 2;
                    }
                    else if ((str[i] & 0xF0) == 0xE0)
                    {
                        charLength = 3;
                    }
                    else if ((str[i] & 0xF8) == 0xF0)
                    {
                        charLength = 4;
                    }

                    if (i + charLength <= str.size())
                    {
                        // 检查后续字节是否合法
                        bool validChar = true;
                        for (int j = 1; j < charLength; ++j)
                        {
                            if (!isUTF8ContinuationByte(str[i + j]))
                            {
                                validChar = false;
                                break;
                            }
                        }

                        if (validChar)
                        {
                            // 将合法的UTF-8字符添加到结果中
                            validChars += str.substr(i, charLength);
                            i += charLength - 1; // 跳过已处理的字节
                        }
                    }
                }
            }
            str = validChars;
        }
    };

    class StringUtil
    {
    public:
        static void Split(const std::string &target, std::vector<std::string> *out, const std::string &sep)
        {
            // boost split
            boost::split(*out, target, boost::is_any_of(sep), boost::token_compress_on);
        }
     
        
    };

    const char *const DICT_PATH = "./dict/jieba.dict.utf8";
    const char *const HMM_PATH = "./dict/hmm_model.utf8";
    const char *const USER_DICT_PATH = "./dict/user.dict.utf8";
    const char *const IDF_PATH = "./dict/idf.utf8";
    const char *const STOP_WORD_PATH = "./dict/stop_words.utf8";

    class JiebaUtil
    {
    private:
        // static cppjieba::Jieba jieba;//设置为静态，类外初始化
        cppjieba::Jieba jieba;
        std::unordered_map<std::string, bool> stop_words;

    private:
        JiebaUtil() : jieba(DICT_PATH, HMM_PATH, USER_DICT_PATH, IDF_PATH, STOP_WORD_PATH) {}
        JiebaUtil(const JiebaUtil &) = delete;
        static JiebaUtil *instance;

    public:
        static JiebaUtil *get_instance()
        {
            static std::mutex mtx;
            if (nullptr == instance)
            {
                mtx.lock();
                if (nullptr == instance)
                {
                    instance = new JiebaUtil();
                    instance->InitJiebaUtil();
                }
                mtx.unlock();
            }
            return instance;
        }
        void InitJiebaUtil()
        {
            std::ifstream in(STOP_WORD_PATH);
            if (!in.is_open())
            {
                LOG(FATAL, "Load stop words file error");
                return;
            }
            std::string line;
            while (std::getline(in, line))
            {
                stop_words.insert({line, true});
            }

            in.close();
        }
        void CutStringHelper(const std::string &src, std::vector<std::string> *out)
        {
            jieba.CutForSearch(src, *out);
            for (auto iter = out->begin(); iter != out->end();)
            {
                auto it = stop_words.find(*iter);
                if (it != stop_words.end()) // 判断是否为暂停词
                {
                    iter = out->erase(iter);
                }
                else
                {
                    iter++;
                }
            }
        }

    public:
        static void CutString(const std::string &src, std::vector<std::string> *out)
        {
            ns_util::JiebaUtil::get_instance()->CutStringHelper(src, out);
            // jieba.CutForSearch(src, *out);
        }
    };
    // cppjieba::Jieba JiebaUtil::jieba(DICT_PATH, HMM_PATH, USER_DICT_PATH, IDF_PATH, STOP_WORD_PATH);
    JiebaUtil *JiebaUtil::instance = nullptr;

    class SQLUtil
    {
    public:
        static MYSQL *MysqlInit(const char *db, const char *host = "127.0.0.1", const char *port = "3306", const char *user = "root", const char *passwd = "", const char *unix_socket = nullptr, unsigned long client_flag = 0)
        {
            MYSQL *mysql = mysql_init(NULL);
            if (mysql == nullptr)
            {
                LOG(FATAL, "init mysql instance failed!");
                return nullptr;
            }
            if (mysql_real_connect(mysql, host, user, passwd, db, std::stoi(port), unix_socket, client_flag) == nullptr)
            {
                LOG(FATAL, "connect mysql sever failed!");
                mysql_close(mysql);
                return nullptr;
            }
            mysql_set_character_set(mysql, "utf8");
            return mysql;
        }

        static void MysqlDestroy(MYSQL *mysql)
        {
            if (mysql != nullptr)
            {
                mysql_close(mysql);
            }
        }

        static bool MysqlQuery(MYSQL *mysql, const std::string &sql)
        {
            if (mysql_query(mysql, sql.c_str()) != 0)
            {
                LOG(FATAL, "mysql query error:" + sql);
                LOG(FATAL, mysql_error(mysql));
                return false;
            }
            return true;
        }
    };

    class JsonUtil
    {
    public:
        static bool Serialize(const Json::Value &root, std::string &str)
        {
            Json::StreamWriterBuilder swb;
            std::unique_ptr<Json::StreamWriter> sw(swb.newStreamWriter());

            std::stringstream ss;
            if (sw->write(root, &ss) != 0)
            {
                LOG(FATAL, "Serialize failed!");
                return false;
            }
            str = ss.str();
            return true;
        }

        static bool UnSerialize(const std::string &str, Json::Value &root)
        {
            Json::CharReaderBuilder crb;
            std::unique_ptr<Json::CharReader> cr(crb.newCharReader());

            std::string errs;
            if (cr->parse(str.c_str(), str.c_str() + str.size(), &root, &errs) == false)
            {
                LOG(FATAL, "UnSerialize failed!" + errs);
                return false;
            }
            return true;
        }
    };

}