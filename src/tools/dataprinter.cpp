#include "dataprinter.h"

#include <boost/filesystem.hpp>

#include "msgprinter.h"
#include "srvprinter.h"
#include "msglistprinter.h"
#include "hdrprinter.h"

DataPrinter::DataPrinter() : m_filepath("") {
}

DataPrinter::DataPrinter(const std::string &filepath) : m_filepath(filepath) {
}

DataPrinter::~DataPrinter() {

}

std::unique_ptr<DataPrinter> DataPrinterFactory::getPrinter(const std::string &filepath) {
    std::unique_ptr<DataPrinter> dataPrinter;
    std::string extension = boost::filesystem::extension(filepath);
    const auto &it = fileTypeId.find(extension);
    if (it != fileTypeId.end()) {
        switch (it->second) {
        case FileType::MSG:
            dataPrinter = std::make_unique<MsgPrinter>(filepath);
            break;
        case FileType::SRV:
            dataPrinter = std::make_unique<SrvPrinter>(filepath);
            break;
        case FileType::MSGLIST:
            dataPrinter = std::make_unique<MsgListPrinter>(filepath);
            break;
        case FileType::HDR:
            dataPrinter = std::make_unique<HdrPrinter>(filepath);
            break;
        default:
            break;
        }
    }
    return dataPrinter;
}
