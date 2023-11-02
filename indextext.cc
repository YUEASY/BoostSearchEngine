#include "index.hpp"

const std::string input = "data/raw_html/raw.txt";

int main()
{
    ns_index::Index *index = ns_index::Index::GetInstance();
    index->BuildIndex(input);
    index->SaveInvertedIndex();
    // if (index->LoadIndex() == true)
    //     std::cout << "读取完成" << std::endl;
    // else
    //     std::cout << "读取失败" << std::endl;
    return 0;
}