#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <ctime>
#include <mutex>
#include "util.hpp"
#include "log.hpp"
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
                LOG(WARNING, "越界错误 doc_id out reange, error");
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
                    // std::cout << "已建立索引的文档: " << count << "/8141" << std::endl;
                    LOG(NORMAL, "当前已建立的索引文档: " + std::to_string(count) + "/8141");
                    timeStart = timeEnd;
                }
            }
            return true;
        }

    private:
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
}