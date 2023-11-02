#pragma once
#include "cpp-httplib/httplib.h"
#include "searcher.hpp"
#include "mysql_operations.hpp"
#include "log.hpp"

namespace ns_httpserver
{
    const std::string root_path = "./wwwroot";
    const std::string input = "data/raw_html/raw.txt";
    ns_searcher::Searcher search;
    ns_operation::TableUser *tb_user = nullptr;
    ns_operation::TableDoc *tb_doc = nullptr;
    class Server
    {
    private:
        int port;            // 服务器的监听端口
        httplib::Server svr; // 用于搭建http服务器
        
    private:
        // 对应的业务处理接口
        // 登录
        static void Login(const httplib::Request &req, httplib::Response &rsp)
        {


            // 获取请求中的账号和密码
            Json::Value login_info;
            // if (ns_util::JsonUtil::UnSerialize(req.body, login_info) == false)
            // {
            //     rsp.set_header("Content-Type", "application/json");
            //     rsp.body = R"({"result":false,"reason":"账号密码反序列化失败"})";
            //     rsp.status = 501;
            //     return;
            // }
            std::string account = req.get_param_value("account");
            std::string password = req.get_param_value("password");

            // 进行账号和密码验证
            if (!account.empty() && !password.empty())
            {
                if (tb_user->ValidateLogin(account, password) == true)
                {
                    rsp.set_header("Set-Cookie", "account=" + account + "; path=/");
                    rsp.set_header("Set-Cookie", "password=" + password + "; path=/");

                    rsp.body = R"({"result":true,"reason":"登录成功"})";
                    Json::Value user;
                    tb_user->SelectOne(account,user);
                    std::string json_string;
                    ns_util::JsonUtil::Serialize(user, json_string);
                    rsp.set_content(json_string, "application/json");
                    rsp.status = 200;
                    return;
                }
                else
                {
                    rsp.set_header("Content-Type", "application/json");
                    rsp.body = R"({"result":false,"reason":"登录失败，账号或密码输入错误"})";
                    rsp.status = 401;
                    return;
                }
            }
            else
            {
                rsp.set_header("Content-Type", "application/json");
                rsp.body = R"({"result":false,"reason":"登录失败,账号或密码为空"})";
                rsp.status = 400;
                return;
            }
        }

        // 注册
        static void Register(const httplib::Request &req, httplib::Response &rsp)
        {
            Json::Value user_info;
            if (ns_util::JsonUtil::UnSerialize(req.body, user_info) == false)
            {
                rsp.set_header("Content-Type", "application/json");
                rsp.body = R"({"result":false,"reason":"注册信息反序列化失败"})";
                rsp.status = 501;
                return;
            }
            if (tb_user->Insert(user_info) == false)
            {
                rsp.set_header("Content-Type", "application/json");
                rsp.body = R"({"result":false,"reason":"注册失败"})";
                rsp.status = 502;
                return;
            }
            rsp.set_header("Content-Type", "application/json");
            rsp.body = R"({"result ":true,"reason":"注册成功"})";
            rsp.status = 200;
            return;
        }

        // 搜索功能
        static void SearchFunction(const httplib::Request &req, httplib::Response &rsp)
        {
            if (!req.has_param("word"))
            {
                rsp.set_content("需要搜素关键字!", "text/plain; charset=utf-8");
                return;
            }
            std::string word = req.get_param_value("word");
            LOG(NORMAL, "正在搜素: " + word);
            std::string json_string;
            search.Search(word, &json_string);
            rsp.set_content(json_string, "application/json");
        }

        // 检查Cookie
        static void CheckCookie(const httplib::Request &req, httplib::Response &rsp)
        {
            std::string cookie = req.get_header_value("Cookie");
            std::string account, password;
            if (cookie.find("account=") != std::string::npos)
            {
                size_t start = cookie.find("account=") + 8; // cookie中存储的格式为 "account=xxx"
                size_t end = cookie.find(";", start);
                account = cookie.substr(start, end - start);
            }
            if (cookie.find("password=") != std::string::npos)
            {
                size_t start = cookie.find("password=") + 9; // cookie中存储的格式为 "password=xxx"
                size_t end = cookie.find(";", start);
                password = cookie.substr(start, end - start);
            }
            if (!account.empty() && !password.empty())
            {

                if(tb_user->ValidateLogin(account, password))
                {
                    rsp.set_header("Content-Type", "application/json");
                    rsp.body = R"({"result":true,"reason":"已登录"})";
                    rsp.status = 200;

                    Json::Value user;
                    tb_user->SelectOne(account, user);
                    std::string json_string;
                    ns_util::JsonUtil::Serialize(user, json_string);
                    rsp.set_content(json_string, "application/json");

                    return;
                }
            }
        }

    public:
        Server(int port) : port(port) {}

        bool RunModule()
        {
            search.InitSearcher(input);
            tb_user = new ns_operation::TableUser();
            tb_doc = new ns_operation::TableDoc();
            // 设置主页
            svr.set_base_dir(root_path.c_str());

            // 设置回调函数
            svr.Get("/s", SearchFunction);
            svr.Post("/login", Login);
            svr.Get("/l", CheckCookie);
            svr.Post("/register", Register);

            LOG(NORMAL, "服务器启动成功!");

            // 启动服务器
            svr.listen("0.0.0.0", port);
            return true;
        }
    
    };
}
