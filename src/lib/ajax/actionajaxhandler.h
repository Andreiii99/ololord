#ifndef ACTIONAJAXHANDLER_H
#define ACTIONAJAXHANDLER_H

namespace cppcms
{

namespace rpc
{

class json_rpc_server;

}

}

#include "abstractajaxhandler.h"

#include <QList>

#include <cppcms/json.h>

#include <map>
#include <string>

class OLOLORD_EXPORT ActionAjaxHandler : public AbstractAjaxHandler
{
public:
    explicit ActionAjaxHandler(cppcms::rpc::json_rpc_server &srv);
public:
    void banUser(const cppcms::json::object &params);
    void deleteFile(std::string boardName, std::string fileName, std::string password);
    void deletePost(std::string boardName, long long postNumber, std::string password);
    void editPost(const cppcms::json::object &params);
    void getBoards();
    void getCaptchaQuota(std::string boardName);
    void getFileExistence(std::string boardName, std::string hash);
    void getFileMetaData(std::string boardName, std::string fileName);
    void getNewPostCount(std::string boardName, long long lastPostNumber);
    void getNewPosts(std::string boardName, long long threadNumber, long long lastPostNumber);
    void getPost(std::string boardName, long long postNumber);
    void getThreadNumbers(std::string boardName);
    void getYandexCaptchaImage(std::string type);
    QList<Handler> handlers() const;
    void setThreadFixed(std::string boardName, long long threadNumber, bool fixed);
    void setThreadOpened(std::string boardName, long long threadNumber, bool opened);
    void setVoteOpened(long long postNumber, bool opened);
    void unvote(long long postNumber);
    void vote(long long postNumber, const cppcms::json::array &votes);
};

#endif // ACTIONAJAXHANDLER_H
