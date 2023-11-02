#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <memory>
#include <boost/filesystem.hpp>
#include <jsoncpp/json/json.h>
#include "util.hpp"
#include "mysql_operations.hpp"
#include <algorithm>
#include <codecvt>

// 文件/数据处理
const std::string url_head = "https://www.boost.org/doc/libs/1_83_0";
const std::string src_path = "data/input";
const std::string output = "data/raw_html/raw.txt";
// const std::string output = "mysql";

typedef struct DocInfo
{
    std::string title;   // 标题
    std::string content; // 内容
    std::string url;     // URL
} DocInfo_t;

bool EnumFile(const std::string &src_path, std::vector<std::string> *files_list);
bool ParseHtml(const std::vector<std::string> &files_list, std::vector<DocInfo_t> *results);
bool SaveHtml(const std::vector<DocInfo_t> &results, const std::string &output);

int main()
{
    std::vector<std::string> files_list;
    // 第一步: 递归式吧每个html文件名带路径保存到file_list
    if (!EnumFile(src_path, &files_list))
    {
        std::cerr << "enum file name error!" << std::endl;
        return 1;
    }
    // 第二步: 按照files_file读取每个文件内容并解析
    std::vector<DocInfo_t> results;
    if (!ParseHtml(files_list, &results))
    {
        std::cerr << "parse html error!" << std::endl;
        return 2;
    }
    // 第三步：把解析完的各个文件写入output,以\3作为分割符
    if (!SaveHtml(results, output))
    {
        std::cerr << "save html error" << std::endl;
        return 3;
    }
    return 0;
}

bool EnumFile(const std::string &src_path, std::vector<std::string> *files_list)
{
    namespace fs = boost::filesystem;
    fs::path root_path(src_path);

    if (!fs::exists(root_path))
    {
        std::cerr << src_path << " does not exist" << std::endl;
        return false;
    }

    // 定义一个空的迭代器用来判断何时结束
    fs::recursive_directory_iterator end;
    for (fs::recursive_directory_iterator iter(root_path); iter != end; iter++)
    {
        // 过滤合法的网页文件
        // 先判断文件是否是普通文件(html属于普通文件)
        if (!fs::is_regular_file(*iter))
        {
            continue;
        }
        // 再判断文件的路径名的后缀是否符合要求
        if (iter->path().extension() != ".html")
        {
            continue;
        }

        // debug
        // std::cout << "debug:" << iter->path().string() << std::endl;

        // 将html文件保存到files_list，方便后续进行文本分析
        files_list->push_back(iter->path().string());
    }
    return true;
}

static bool ParseTitle(const std::string &file, std::string *title)
{
    std::size_t begin = file.find("<title>");
    if (begin == std::string::npos)
    {
        return false;
    }
    std::size_t end = file.find("</title>");
    if (end == std::string::npos)
    {
        return false;
    }

    begin += std::string("<title>").size();
    if (begin > end)
    {
        return false;
    }

    *title = file.substr(begin, end - begin); // substr(起始，长度)

    return true;
}

static bool ParseContent(const std::string &file, std::string *content)
{
    // 去标签
    enum status
    {
        LABLE,
        CONTENT
    };

    enum status s = LABLE;
    for (char c : file)
    {
        switch (s)
        {
        case LABLE:
            if (c == '>')
                s = CONTENT;
            break;
        case CONTENT:
            if (c == '<')
                s = LABLE;
            else
            {
                //'\n'要作为html解析后文本的分割符，所以不保留原始文件的'\n'
                if (c == '\n')
                {
                    c = ' ';
                }
                content->push_back(c);
            }
            break;
        default:
            break;
        }
    }
    // ns_util::StringUtil::Escape(*content);
    //*content = ns_util::StringUtil::utf8ToBinaryString(*content);
    return true;
}

static bool ParseUrl(const std::string &file_path, std::string *url)
{
    // std::string url_head;
    std::string url_tail = file_path.substr(src_path.size());

    *url = url_head + url_tail;
    return true;
}

// only for debug
static void ShowDoc(const DocInfo_t &doc)
{
    std::cout << "tile: " << doc.title << std::endl;
    std::cout << "content: " << doc.content << std::endl;
    std::cout << "url: " << doc.url << std::endl;
}

bool ParseHtml(const std::vector<std::string> &files_list, std::vector<DocInfo_t> *results)
{
    for (const std::string &file : files_list)
    {
        // 1.读取文件：read()
        std::string result;
        if (!ns_util::FileUtil::ReadFile(file, &result))
        {
            continue;
        }
        DocInfo_t doc;
        // 2.解析文件内容，提取title
        if (!ParseTitle(result, &doc.title))
        {
            continue;
        }
        // 3.提取content
        if (!ParseContent(result, &doc.content))
        {
            continue;
        }
        // 4.解析文件路径，构建url
        if (!ParseUrl(file, &doc.url))
        {
            continue;
        }

        // 完成解析，当前文档的相关结果保存在doc里
        // results->push_back(doc); // 未优化：拷贝效率低
        results->push_back(std::move(doc)); // 已优化
        // debug
        // ShowDoc(doc);
        // break;
    }
    return true;
}

bool SaveHtml(const std::vector<DocInfo_t> &results, const std::string &output)
{
#define SEP '\3'

    std::unique_ptr<ns_operation::TableDoc> tb_doc(new ns_operation::TableDoc());

    // 按照二进制方式进行写入
    std::ofstream out(output, std::ios::out | std::ios::binary);
    if (!out.is_open())
    {
        std::cerr << "open " << output << " failed!" << std::endl;
        return false;
    }

    int i = 0;
    for (auto &item : results)
    {
        std::string out_string;
        out_string = item.title;
        out_string += SEP;
        out_string += item.content;
        out_string += SEP;
        out_string += item.url;
        out_string += '\n';

        out.write(out_string.c_str(), out_string.size());

        Json::Value tmp;
        tmp["doc_id"] = i;
        tmp["title"] = item.title;
        tmp["content"] = item.content;
        tmp["url"] = item.url;
        if (tb_doc->Insert(tmp) == false)
        {
            return false;
        }
        i++;
    }
    out.close();

    return true;
}
