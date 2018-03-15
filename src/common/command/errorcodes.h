#ifndef ERRORCODES_H
#define ERRORCODES_H

/** \brief Error code class provides function to return string message for certain enum error code. */
class ErrorCodes {
public:
    /** \brief Error codes used in get accounts (currently) command. */
    enum Code {
        eNone = 0,
        eBadPath = 1,
        eBadNode = 2,
        eBankNotFound = 3,
        eUndoNotFound = 4
    };

private:
    const std::map<ErrorCodes::Code, const char*> errorCodeMsg = {
        { Code::eNone, "No error" },
        { Code::eBadPath, "Bad path" },
        { Code::eBadNode, "Bad node" },
        { Code::eBankNotFound, "Can't open bank file" },
        { Code::eUndoNotFound, "Can't open undo file" }
    };

public:
    const char* getErrorMsg(ErrorCodes::Code errorCode) {
        const auto &it = errorCodeMsg.find(errorCode);
        if (it != errorCodeMsg.end()) {
            return it->second;
        }
        return "";
    }
    const char* getErrorMsg(int errorCode) {
        const auto &it = errorCodeMsg.find((ErrorCodes::Code)errorCode);
        if (it != errorCodeMsg.end()) {
            return it->second;
        }
        return "";
    }
};


#endif // ERRORCODES_H
