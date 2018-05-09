#include <iostream>
#include <string>
#include <memory>
#include <boost/program_options.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/range/iterator_range.hpp>

#include "dataprinter.h"

using namespace std;
using namespace boost;

static void show_usage(const std::string& app_name) {
    std::cout << "USAGE: "<<app_name<<" path_to_dir|path_to_file"<<std::endl;
    std::cout << std::endl;
    std::cout << "\t" << "-h [--help] \t Help message"<<std::endl;
    std::cout << std::endl;
    std::cout << "EXAMPLE: "<<app_name<<" 03_0001_00000001.msg"<<std::endl;

}

int main(int argc, char **argv) {

    std::string path;
    try {
        program_options::options_description desc("Allowed options");
        desc.add_options()
                ("help,h", "Help screen")
                ("path", program_options::value<std::string>(&path)->required(), "Path to file or directory");

        program_options::positional_options_description positionalOptions;
        positionalOptions.add("path", 1);

        program_options::variables_map vm;
        store(program_options::command_line_parser(argc, argv).options(desc).positional(positionalOptions).run(), vm);

        if (vm.count("help")) {
            show_usage(argv[0]);
            return 0;
        }

        notify(vm);
    } catch(std::exception& e) {
        std::cout<<e.what()<<std::endl;
    }

    if (path.empty()) {
        show_usage(argv[0]);
        return 0;
    }

    std::vector<filesystem::path> files;
    if (filesystem::exists(path)) {
        if (filesystem::is_directory(path)) {
            for (auto &file : make_iterator_range(filesystem::directory_iterator(path), {})) {
                if (filesystem::is_regular_file(file))
                    files.push_back(file);
            }
        } else {
            files.push_back(path);
        }
    } else {
        std::cerr << "Incorrect path, file not exists" <<std::endl;
        show_usage(argv[0]);
        return 0;
    }

    for (auto &it : files) {
        std::unique_ptr<DataPrinter> printer = DataPrinterFactory().getPrinter(it.string());
        if (!printer) {
            std::cout << "File " << it << " type not supported" << std::endl;
            continue;
        }
        try {
            std::cout<< "File " << it << std::endl;
            printer->printJson();
        } catch (std::exception& e) {
            std::cerr <<"Exception: "<<e.what()<<std::endl;
        }
    }

    return 0;
}
