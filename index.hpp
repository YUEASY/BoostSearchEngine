#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <ctime>
#include <mutex>
#include <jsoncpp/json/json.h>
#include "util.hpp"
#include "log.hpp"
#include "mysql_operations.hpp"
// 索引
namespace ns_index
{
    struct DocInfo
    {
        std::string title;   // 标题
        std::string content; // 内容
        std::string url;     // URL
        uint64_t doc_id;     // 文档id
    };

    struct InvertedElem
    {
        uint64_t doc_id;
        std::string word;
        int weight; // 权重值
    };

    // 倒排拉链
    typedef std::vector<InvertedElem> InvertedList;

    class Index
    {
    private:
        // 正排索引使用数组，数组下标为文档id
        std::vector<DocInfo> forward_index;
        // 倒排索引为一个关键字对应多个InvertedElem
        std::unordered_map<std::string, InvertedList> inverted_index;

    private: // 单例模型
        Index() {}
        Index(const Index &) = delete;
        Index &operator=(const Index &) = delete;

        static Index *instance;
        static std::mutex mtx;
        static ns_operation::TableInverted *tb_inv;
        static ns_operation::TableDoc *tb_doc;

    public:
        ~Index() {}

    public:
        static Index *GetInstance()
        {
            if (nullptr == instance)
            {
                mtx.lock();
                if (nullptr == instance)
                {
                    instance = new Index();
                    tb_inv = new ns_operation::TableInverted();
                    tb_doc = new ns_operation::TableDoc();
                }
                mtx.unlock();
            }
            return instance;
        }
        // 根据id找到文档内容
        DocInfo *GetForwardIndex(uint64_t doc_id)
        {
            if (doc_id >= forward_index.size()) // 是否越界
            {
                // std::cerr << "doc_id out reange, error" << std::endl;
                LOG(WARNING, "越界错误 doc_id = " + std::to_string(doc_id) + "[max:"+ std::to_string(forward_index.size()) +"]"+" out reange, error");
                return nullptr;
            }
            return &forward_index[doc_id];
        }
        // 根据关键字word，得到倒排拉链
        InvertedList *GetInvertedList(const std::string &word)
        {
            auto iter = inverted_index.find(word);
            if (iter == inverted_index.end())
            {
                // std::cerr << word << "have no InvertedList" << std::endl;
                LOG(NOTICE, "无" + word + "相关倒排拉链 have no InvertedList");
                return nullptr;
            }
            return &(iter->second);
        }
        // 根据去标签、格式化之后的文档，构建正排和倒排索引
        // data/raw_html/raw.txt
        bool BuildIndex(const std::string &input) // 接收parser处理完的数据
        {
            std::ifstream in(input, std::ios::binary | std::ios::in);
            if (!in.is_open())
            {
                // std::cerr << "sorry, " << input << " open error" << std::endl;
                LOG(FATAL, "sorry, " + input + " open error");
                return false;
            }
            std::string line;
            int count = 0;
            clock_t timeStart = clock();
            clock_t timeEnd;
            while (std::getline(in, line))
            {
                DocInfo *doc = BuildForwardIndex(line);
                if (doc == nullptr)
                {
                    // std::cerr << "build error: " << line  << std::endl;
                    LOG(WARNING, "build error: " + line);
                    continue;
                }

                BuildInvertedIndex(*doc);
                count++;
                timeEnd = clock();
                if ((timeEnd - timeStart) / CLOCKS_PER_SEC >= 1)
                {
                    LOG(NORMAL, "当前已建立的索引文档: " + std::to_string(count));
                    timeStart = timeEnd;
                }
            }
            return true;
        }
        
        bool LoadInvertedIndex()    // 根据数据库中正排索引建立倒排索引
        {
            Json::Value docs;
            if (tb_doc->SelectAll(docs) == false)
            {
                return false;
            }
            for (auto &doc : docs)
            {
                DocInfo docif;
                docif.doc_id = doc["doc_id"].asInt();
                docif.url = doc["url"].asString();
                docif.title = doc["title"].asString();
                docif.content = doc["content"].asString();
                BuildInvertedIndex(docif);
                forward_index.push_back(std::move(docif));
            }
            return true;
        }

        bool LoadIndex() // 从数据库读取索引
        {
            Json::Value tmps;
            if (tb_inv->SelectAll(tmps) == false)
            {
                return false;
            }

            for (auto &tmp : tmps)
            {
                InvertedElem item;
                item.doc_id = tmp["doc_id"].asInt();
                item.word = tmp["word"].asString();
                item.weight = tmp["weight"].asInt();
                inverted_index[item.word].push_back(std::move(item));
            }
            
            Json::Value docs;
            if(tb_doc->SelectAll(docs) == false)
            {
                return false;
            }
            for(auto &doc : docs)
            {
                DocInfo docif;
                docif.doc_id = doc["doc_id"].asInt();
                docif.url = doc["url"].asString();
                docif.title = doc["title"].asString();
                docif.content = doc["content"].asString();
                forward_index.push_back(std::move(docif));
            }

            return true;
        }

        bool SaveInvertedIndex()   
        {
            if (tb_inv->Clear() == false)
            {
                return false;
            }
            for (auto &item_list : inverted_index)
            {
                for (auto &item : item_list.second)
                {
                    Json::Value tmp;
                    tmp["doc_id"] = std::to_string(item.doc_id);
                    tmp["word"] = item.word;
                    tmp["weight"] = item.weight;
                    if (tb_inv->Insert(tmp) == false)
                    {
                        return false;
                    }
                }
            }
            return true;
        }

    private:
        // 构建正排索引
        DocInfo *BuildForwardIndex(const std::string &line)
        {
            // 1. 解析line，字符串切分 [title\3 content\3 url]
            std::string sep = "\3";
            std::vector<std::string> results;
            ns_util::StringUtil::Split(line, &results, sep);
            if (results.size() != 3)
            {
                return nullptr;
            }
            // 2. 字符串进行填充到DocInfo中
            DocInfo doc;
            doc.title = results[0];
            doc.content = results[1];
            doc.url = results[2];
            doc.doc_id = forward_index.size();

            // 3. 插入到正排索引的vector
            forward_index.push_back(std::move(doc));

            return &forward_index.back();
        }
        // 构建倒排索引
        bool BuildInvertedIndex(const DocInfo &doc)
        {
            struct word_cnt
            {
                int title_cnt;
                int content_cnt;
                word_cnt() : title_cnt(0), content_cnt(0) {}
            };
            std::unordered_map<std::string, word_cnt> word_map; // 用来暂存词频的映射表

            std::vector<std::string> title_words;
            ns_util::JiebaUtil::CutString(doc.title, &title_words);

            for (std::string s : title_words)
            {
                boost::to_lower(s);
                word_map[s].title_cnt++;
            }

            std::vector<std::string> content_words;
            ns_util::JiebaUtil::CutString(doc.content, &content_words);

            for (std::string s : content_words)
            {
                boost::to_lower(s);
                word_map[s].content_cnt++;
            }

#define X 10
#define Y 1
            for (auto &word_pair : word_map)
            {
                InvertedElem item;
                item.doc_id = doc.doc_id;
                item.word = word_pair.first;
                item.weight = (X - Y) * word_pair.second.title_cnt + Y * word_pair.second.content_cnt;
                InvertedList &inverted_list = inverted_index[word_pair.first];
                inverted_list.push_back(std::move(item));
            }

            return true;
        }
    };
    std::mutex Index::mtx;
    Index *Index::instance = nullptr;
    ns_operation::TableInverted *Index::tb_inv = nullptr;
    ns_operation::TableDoc *Index::tb_doc = nullptr;
}