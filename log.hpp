#pragma once
// 日志
#include <iostream>
#include <string>
#include <ctime>

#define NORMAL 1
#define NOTICE 2
#define WARNING 3
#define DEBUG 4
#define FATAL 5

#define LOG(LEVEL, MESSAGE) log(#LEVEL, MESSAGE, __FILE__, __LINE__)
// 符号 # 称为字符串化运算符,它会把宏调用时的实参转换为字符串
//__FTLE__  获取文件名称
//__LINE__  获取行号
void log(std::string level, std::string message, std::string file, int line)
{
    std::cout << "[" << level << "]"
              << "[" << time(nullptr) << "]"
              << "[" << message << "]"
              << "[" << file << " : " << line << "]" << std::endl;
}
