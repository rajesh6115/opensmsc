#include "app_info.hpp"
#include <iostream>

namespace smpp {

std::string AppInfo::info_string() {
    std::string info;
    info += PROJECT_NAME;
    info += " ";
    info += FULL_VERSION_STRING;
    info += "\n";
    info += "Description: ";
    info += PROJECT_DESCRIPTION;
    info += "\n";
    info += "Homepage: ";
    info += PROJECT_HOMEPAGE_URL;
    info += "\n";
    info += "Compiler: ";
    info += COMPILER;
    info += " (C++";
    info += CXX_STANDARD;
    info += ")\n";
    info += "Build Type: ";
    info += BUILD_TYPE;
    return info;
}

void AppInfo::print_info() {
    std::cout << info_string() << std::endl;
}

}  // namespace smpp
