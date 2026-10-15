#pragma once
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif
inline std::vector<std::string> applicationArguments(int argc, char** argv) {
#ifdef _WIN32
    int count=0;auto** wide=CommandLineToArgvW(GetCommandLineW(),&count);
    if(!wide)throw std::runtime_error("Cannot read Unicode command line");
    std::vector<std::string> args;
    try {
        for(int i=0;i<count;++i){
            const int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide[i],-1,nullptr,0,nullptr,nullptr);
            if(n<=0)throw std::runtime_error("Invalid Unicode argument");
            std::string text(static_cast<std::size_t>(n),'\0');
            if(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide[i],-1,text.data(),n,nullptr,nullptr)!=n)throw std::runtime_error("Cannot convert Unicode argument");
            text.pop_back();args.push_back(std::move(text));
        }
    }catch(...){LocalFree(wide);throw;}
    LocalFree(wide);(void)argc;(void)argv;return args;
#else
    return {argv,argv+argc};
#endif
}
