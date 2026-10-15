#include "project.hpp"
#include <iostream>
int main(int argc,char** argv){
    try {
        const auto file=argc>1?std::filesystem::u8path(argv[1]):std::filesystem::path("example.pxproj");
        studio::save(studio::Project::example(),file);
        std::cout<<"Created project: "<<file.u8string()<<'\n';
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
