#pragma once

#include <string>
#include <iostream>
#include <vector>
#include <fstream>
#include <mutex>
#include <boost/algorithm/string.hpp>
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
                std::cerr << "open file" << file_path << "error: " << std::endl;
            }

            std::string line;
            while (std::getline(in, line))
            {
                *out += line;
            }
            in.close();
            return true;
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
}