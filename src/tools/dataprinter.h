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
        MSGLIST,
        HDR
    };

    const std::map<const std::string, FileType> fileTypeId = {
        { ".msg", FileType::MSG },
        { ".srv", FileType::SRV },
        { ".dat", FileType::MSGLIST},
        { ".hdr", FileType::HDR}
    };

public:
     std::unique_ptr<DataPrinter> getPrinter(const std::string& filepath);
};

#endif // DATAPRINTER_H
