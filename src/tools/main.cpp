#include <iostream>
#include <string>

#include "jsonprinter.h"

using namespace std;

static void show_usage(const std::string& app_name) {
    std::cerr << "Usage: "<<app_name<<" <path_to_msg_file>"<<std::endl;
    std::cerr << "Example: "<<app_name<<" 03_0001_00000001.msg\n"<<std::endl;
}

int loadFile();

int printJson();

int main(int argc, char **argv) {
    if (argc != 2) {
        show_usage(argv[0]);
        return 0;
    }

    std::string arg = argv[1];
    if ((arg == "-h") || (arg == "--help")) {
        show_usage(argv[0]);
        return 0;
    }

    try {
        JsonPrinter printer(argv[1]);
        printer.printJson();
    } catch (std::exception& e) {
        std::cerr <<e.what()<<std::endl;
    }

    return 0;
}
