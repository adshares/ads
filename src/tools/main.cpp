#include <iostream>
#include <string>
#include <memory>

#include "dataprinter.h"

using namespace std;

static void show_usage(const std::string& app_name) {
    std::cout << "Usage: "<<app_name<<" <path_to_file>"<<std::endl;
    std::cout << "Example: "<<app_name<<" 03_0001_00000001.msg\n"<<std::endl;
    std::cout << "Example: "<<app_name<<" msglist.dat\n"<<std::endl;
    std::cout << "Example: "<<app_name<<" servers.srv\n"<<std::endl;
}

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

    std::unique_ptr<DataPrinter> printer = DataPrinterFactory().getPrinter(argv[1]);
    if (!printer) {
        std::cerr << "File extension not supported" <<std::endl;
        return 0;
    }

    try {
        printer->printJson();
    } catch (std::exception& e) {
        std::cerr <<"Exception: "<<e.what()<<std::endl;
    }

    return 0;
}
