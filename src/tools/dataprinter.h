#ifndef DATAPRINTER_H
#define DATAPRINTER_H

#include <string>
#include <memory>
#include <map>

class DataPrinter {
public:
    DataPrinter();
    DataPrinter(const std::string& filepath);
    virtual ~DataPrinter();
    virtual void printJson() = 0;
    virtual void printString() = 0;
protected:
    std::string m_filepath;
};

class DataPrinterFactory {
private:
    enum FileType {
        MSG = 0,
        SRV,
        DAT,
        HDR,
        UNDO,
        HLOG,
        SIGNATURES_OK,
        VIP,
        INX,
        LOG,
        ARCH
    };

    const std::map<const std::string, FileType> fileTypeId = {
        { ".msg", FileType::MSG },
        { ".srv", FileType::SRV },
        { ".dat", FileType::DAT },
        { ".hdr", FileType::HDR },
        { ".und", FileType::UNDO },
        { ".hlg", FileType::HLOG },
        { ".ok",  FileType::SIGNATURES_OK },
        { ".vip", FileType::VIP },
        { ".inx", FileType::INX },
        { ".log", FileType::LOG },
        { ".arch", FileType::ARCH }
    };

public:
     std::unique_ptr<DataPrinter> getPrinter(const std::string& filepath);
};

#endif // DATAPRINTER_H
