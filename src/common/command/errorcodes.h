#ifndef ERRORCODES_H
#define ERRORCODES_H

#define ERROR_CODE_LENGTH 4
#define ERROR_TAG "error"


/** \brief Error code class provides function to return string message for certain enum error code. */
class ErrorCodes {
public:
    /** \brief Error codes used in get accounts (currently) command. */
    enum Code {
        eNone = 0,
        eBadPath,
        eBadNode,
        eBadUser,
        eBankNotFound,
        eUserNotFound,
        eBankIncorrect,
        eUndoNotFound,
        eGetUserFail,
        eGetGlobalUserFail,
        eLowBalance,
        eReadOnlyMode,
        eBadMsgId,
        eCreateAccountBadTiming,
        eCreateAccountFail,
        eMessageSubmitFail,
        eWrongSignature,
        eDuplicatedTarget,
        eAmountBelowZero,
        eUserBadTarget,
        eTimeInFuture,
        eBroadcastNotReady,
        eNoBroadcastFile,
        eIncorrectTransaction,
        eMatchSecretKeyNotFound,
        eSetKeyRemoteBankFail,
        eConnectServerError
    };

private:
    const std::map<ErrorCodes::Code, const char*> errorCodeMsg = {
        { Code::eNone, "No error" },
        { Code::eBadPath, "Bad path" },
        { Code::eBadNode, "Bad node" },
        { Code::eBadUser, "Bad user" },
        { Code::eBankNotFound, "Can't open bank file" },
        { Code::eUserNotFound, "Read user failed" },
        { Code::eBankIncorrect, "Incorrect bank" },
        { Code::eUndoNotFound, "Can't open undo file" },
        { Code::eGetUserFail, "Failed to get user info" },
        { Code::eGetGlobalUserFail, "Failed to get global user info" },
        { Code::eLowBalance, "Too low balance" },
        { Code::eReadOnlyMode, "Reject transaction in readonly mode" },
        { Code::eBadMsgId, "Bad message id (msid)" },
        { Code::eCreateAccountBadTiming, "Bad timing for remote account request, try again later."},
        { Code::eCreateAccountFail, "Failed to create account" },
        { Code::eMessageSubmitFail, "Failed message submission" },
        { Code::eWrongSignature, "Wrong signature" },
        { Code::eDuplicatedTarget, "Duplicated target" },
        { Code::eAmountBelowZero, "Amount below zero" },
        { Code::eUserBadTarget, "Bad target user" },
        { Code::eTimeInFuture, "Can't perform operation, inserted time value is in feature" },
        { Code::eBroadcastNotReady, "Broadcast not ready, try again later" },
        { Code::eNoBroadcastFile, "No broadcast file to send" },
        { Code::eIncorrectTransaction, "Incorrect transaction type" },
        { Code::eMatchSecretKeyNotFound, "Matching secret key not found" },
        { Code::eSetKeyRemoteBankFail, "Setting key for remote bank failed" },
        { Code::eConnectServerError, "Can't connect to server" }
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
