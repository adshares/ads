#include "dataprinter.h"

#include <boost/filesystem.hpp>

#include "msgprinter.h"
#include "srvprinter.h"
#include "msglistprinter.h"
#include "hdrprinter.h"
#include "undoprinter.h"
#include "hlogprinter.h"
#include "signaturesprinter.h"
#include "datprinter.h"
#include "vipprinter.h"
#include "inxprinter.h"
#include "logprinter.h"

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
        case FileType::DAT:
            if (boost::filesystem::basename(filepath).compare("msglist") == 0) {
                dataPrinter = std::make_unique<MsgListPrinter>(filepath);
            } else {
                dataPrinter = std::make_unique<DatPrinter>(filepath);
            }
            break;
        case FileType::HDR:
            dataPrinter = std::make_unique<HdrPrinter>(filepath);
            break;
        case FileType::UNDO:
            dataPrinter = std::make_unique<UndoPrinter>(filepath);
            break;
        case FileType::HLOG:
            dataPrinter = std::make_unique<HlogPrinter>(filepath);
            break;
        case FileType::SIGNATURES_OK:
            dataPrinter = std::make_unique<SignaturesPrinter>(filepath);
            break;
        case FileType::VIP:
            dataPrinter = std::make_unique<VipPrinter>(filepath);
            break;
        case FileType::INX:
            dataPrinter = std::make_unique<InxPrinter>(filepath);
            break;
        case FileType::LOG:
            dataPrinter = std::make_unique<LogPrinter>(filepath);
            break;
        default:
            break;
        }
    }
    return dataPrinter;
}
