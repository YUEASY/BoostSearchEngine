#pragma once
#include "index.hpp"
#include "log.hpp"
// #include "util.hpp"//已包含在index.hpp内
#include <algorithm>
#include <jsoncpp/json/json.h>
// 搜素
namespace ns_searcher
{

    struct InvertedElemPrint
    {
        uint64_t doc_id;
        int weight;
        std::vector<std::string> words;
        InvertedElemPrint() : doc_id(0), weight(0) {}
    };

    class Searcher
    {
    private:
        ns_index::Index *index; // 供系统进行查找的索引
    public:
        Searcher() {}
        ~Searcher() {}

    public:
        void InitSearcher(const std::string &input)
        {
            // 1.获取或创建index对象
            index = ns_index::Index::GetInstance();
            // std::cout << "获取index单例成功. . . " << std::endl;
            LOG(NORMAL, "获取index单例成功. . . ");
            // 2.根据index对象建立索引
            if(!index->BuildIndex(input))
            {
                LOG(FATAL, "建立索引失败. . . ");
            }
            // std::cout << "建立正排和倒排索引成功. . . " << std::endl;
            LOG(NORMAL, "建立正排和倒排索引成功. . . ");
        }
        // query : 搜素关键字
        // json_string : 返回给客户端浏览器的搜素结果
        void Search(const std::string &query, std::string *json_string)
        {
            // 第一步：分词，对我们的query进行按照searcher的要求进行分词
            std::vector<std::string> words;
            ns_util::JiebaUtil::CutString(query, &words);
            // 第二步：触发，根据分词的结果进行index查找
            // ns_index::InvertedList inverted_list_all;

            std::vector<InvertedElemPrint> inverted_list_all;
            std::unordered_map<uint64_t, InvertedElemPrint> tokens_map;

            for (std::string word : words)
            {
                boost::to_lower(word);

                ns_index::InvertedList *inverted_list = index->GetInvertedList(word);
                if (inverted_list == nullptr)
                {
                    continue;
                }
                // inverted_list_all.insert(inverted_list_all.end(), inverted_list->begin(), inverted_list->end());
                for (const auto &elem : *inverted_list)
                {
                    auto &item = tokens_map[elem.doc_id];
                    // item一定是doc_id相同的print节点
                    item.doc_id = elem.doc_id;
                    item.weight += elem.doc_id;
                    item.words.push_back(std::move(elem.word));
                }
            }
            for (const auto &item : tokens_map)
            {
                inverted_list_all.push_back(std::move(item.second));
            }

            // 第三步：合并排序，汇总查找结果，按照相关性weight(降序)排序
            // std::sort(inverted_list_all.begin(), inverted_list_all.end(),
            //           [](const ns_index::InvertedElem &e1, const ns_index::InvertedElem &e2)
            //           { return e1.weight > e2.weight; });
            std::sort(inverted_list_all.begin(), inverted_list_all.end(),
                      [](const InvertedElemPrint &e1, const InvertedElemPrint &e2)
                      { return e1.weight > e2.weight; });

            // 第四部：构建，根据查找结果，构建json串 -- jsoncpp
            Json::Value root;
            for (auto &item : inverted_list_all)
            {
                ns_index::DocInfo *doc = index->GetForwardIndex(item.doc_id);
                if (doc == nullptr)
                {
                    continue;
                }
                Json::Value elem;
                elem["title"] = doc->title;
                elem["desc"] = GetDesc(doc->content, item.words[0]); // 对content进行取关键词上下文内容的操作
                elem["url"] = doc->url;
                // for debug
                elem["id"] = (int)item.doc_id;
                elem["weight"] = item.weight; // int自动转string

                root.append(elem);
            }
            // Json::StyledWriter writer;
            Json::FastWriter writer;
            *json_string = writer.write(root);
        }
        std::string GetDesc(const std::string &html_content, const std::string &word)
        {
            // 找到word在html_content首次出现的位置，截取相近的上下文
            const std::size_t prev_step = 128;
            const std::size_t next_step = 256;
            auto iter = std::search(html_content.begin(), html_content.end(), word.begin(), word.end(),
                                    [](int x, int y)
                                    { return std::tolower(x) == std::tolower(y); });
            if (iter == html_content.end())
            {
                return "None 1";
            }
            std::size_t pos = std::distance(html_content.begin(), iter);//计算两个迭代器表示的范围内包含元素的个数

            std::size_t start = 0;
            std::size_t end = html_content.size() - 1;
            start = (pos > start + prev_step) ? pos - prev_step : start;
            end = (pos + next_step < end) ? pos + next_step : end;

            if (end <= start)
            {
                return "None 2";
            }
            return html_content.substr(start, end - start) + ". . . . . .";
        }
    };
}