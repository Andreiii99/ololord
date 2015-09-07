#include "cache.h"

#include "controller/baseboard.h"
#include "settingslocker.h"
#include "translator.h"

#include <BeQt>
#include <BTranslator>

#include <QByteArray>
#include <QCache>
#include <QDateTime>
#include <QLocale>
#include <QMap>
#include <QReadLocker>
#include <QReadWriteLock>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QWriteLocker>

namespace Cache
{

static QCache<QString, QString> boards;
static QReadWriteLock boardsLock(QReadWriteLock::Recursive);
static QCache<QString, QString> theCustomContent;
static QReadWriteLock customContentLock(QReadWriteLock::Recursive);
static QCache<QString, CustomLinkInfoList> theCustomLinks;
static QReadWriteLock customLinksLock(QReadWriteLock::Recursive);
static QCache<QString, File> dynamicFiles;
static QReadWriteLock dynamicFilesLock(QReadWriteLock::Recursive);
static QCache<QString, Tools::FriendList> theFriendList;
static QReadWriteLock friendListLock(QReadWriteLock::Recursive);
static QCache<QString, IpBanInfoList> theIpBanInfoList;
static QReadWriteLock ipBanInfoListLock(QReadWriteLock::Recursive);
static QCache<QString, QStringList> theNews;
static QReadWriteLock newsLock(QReadWriteLock::Recursive);
static QCache<QString, Content::Post> thePosts;
static QReadWriteLock postsLock(QReadWriteLock::Recursive);
static QCache<QString, QStringList> theRules;
static QReadWriteLock rulesLock(QReadWriteLock::Recursive);
static QCache<QString, File> staticFiles;
static QReadWriteLock staticFilesLock(QReadWriteLock::Recursive);
static QCache<QString, QString> threads;
static QReadWriteLock threadsLock(QReadWriteLock::Recursive);
static QCache<QString, BTranslator> translators;
static QReadWriteLock translatorsLock(QReadWriteLock::Recursive);

template <typename T>
void initCache(T &cache, const QString &name, int defaultSize)
{
    if (name.isEmpty())
        return;
    if (defaultSize < 0)
        defaultSize = 100 * BeQt::Megabyte;
    int sz = SettingsLocker()->value("Cache/" + name + "/max_size", defaultSize).toInt();
    cache.setMaxCost((sz >= 0) ? sz : 0);
}

ClearCacheFunctionMap availableClearCacheFunctions()
{
    init_once(ClearCacheFunctionMap, map, ClearCacheFunctionMap()) {
        map.insert("boards", &clearBoards);
        map.insert("custom_content", &clearCustomContent);
        map.insert("custom_links", &clearCustomLinks);
        map.insert("dynamic_files", &clearDynamicFilesCache);
        map.insert("friend_list", &clearFriendListCache);
        map.insert("ip_ban_info_list", &clearIpBanInfoListCache);
        map.insert("news", &clearNewsCache);
        map.insert("posts", &clearPostsCache);
        map.insert("rules", &clearRulesCache);
        map.insert("static_files", &clearStaticFilesCache);
        map.insert("threads", &clearThreads);
        map.insert("translators", &clearTranslatorsCache);
    }
    return map;
}

SetMaxCacheSizeFunctionMap availableSetMaxCacheSizeFunctions()
{
    init_once(SetMaxCacheSizeFunctionMap, map, SetMaxCacheSizeFunctionMap()) {
        map.insert("boards", &setBoardsMaxCacheSize);
        map.insert("custom_content", &setCustomContentMaxCacheSize);
        map.insert("custom_links", &setCustomLinksMaxCacheSize);
        map.insert("dynamic_files", &setDynamicFilesMaxCacheSize);
        map.insert("friend_list", &setFriendListMaxCacheSize);
        map.insert("ip_ban_info_list", &setIpBanInfoListMaxCacheSize);
        map.insert("news", &setNewsMaxCacheSize);
        map.insert("posts", &setPostsMaxCacheSize);
        map.insert("rules", &setRulesMaxCacheSize);
        map.insert("static_files", &setStaticFilesMaxCacheSize);
        map.insert("threads", &setThreadsMaxCacheSize);
        map.insert("translators", &setTranslatorsMaxCacheSize);
    }
    return map;
}

QStringList availableCacheNames()
{
    init_once(QStringList, names, QStringList()) {
        names << "boards";
        names << "custom_content";
        names << "custom_links";
        names << "home_page";
        names << "dynamic_files";
        names << "friend_list";
        names << "ip_ban_info_list";
        names << "news";
        names << "posts";
        names << "rules";
        names << "static_files";
        names << "threads";
        names << "translators";
    }
    return names;
}

QString *board(const QString &boardName, const QLocale &l, unsigned int page)
{
    if (boardName.isEmpty())
        return 0;
    QReadLocker locker(&boardsLock);
    return boards.object(boardName + "/" + l.name() + "/" + QString::number(page));
}

bool cacheBoard(const QString &boardName, const QLocale &l, unsigned int page, QString *content)
{
    if (boardName.isEmpty() || !content)
        return false;
    QWriteLocker locker(&boardsLock);
    do_once(init)
        initCache(boards, "boards", defaultBoardsCacheSize);
    int sz = content->length() * 2;
    if (boards.maxCost() < sz)
        return false;
    boards.insert(boardName + "/" + l.name() + "/" + QString::number(page), content, sz);
    return true;
}

bool cacheCustomContent(const QString &prefix, const QLocale &l, QString *content)
{
    if (prefix.isEmpty() || !content)
        return false;
    QWriteLocker locker(&customContentLock);
    do_once(init)
        initCache(theCustomContent, "custom_content", defaultCustomContentCacheSize);
    int sz = content->length() * 2;
    if (theCustomContent.maxCost() < sz)
        return false;
    theCustomContent.insert(prefix + "/" + l.name(), content, sz);
    return true;
}

bool cacheCustomLinks(const QLocale &l, CustomLinkInfoList *list)
{
    if (!list)
        return false;
    QWriteLocker locker(&customLinksLock);
    do_once(init)
        initCache(theCustomLinks, "custom_links", defaultCustomLinksCacheSize);
    int sz = list->size();
    if (theCustomLinks.maxCost() < sz)
        return false;
    theCustomLinks.insert(l.name(), list, sz);
    return true;
}

File *cacheDynamicFile(const QString &path, const QByteArray &file)
{
    if (path.isEmpty())
        return 0;
    QWriteLocker locker(&dynamicFilesLock);
    do_once(init)
        initCache(dynamicFiles, "dynamic_files", defaultDynamicFilesCacheSize);
    if (dynamicFiles.maxCost() < (file.size() + 8))
        return 0;
    File *f = new File;
    f->data = file;
    f->msecsSinceEpoch = (QDateTime::currentMSecsSinceEpoch() / 1000) * 1000;
    dynamicFiles.insert(path, f, file.size() + 8);
    return f;
}

bool cacheFriendList(Tools::FriendList *list)
{
    if (!list)
        return false;
    QWriteLocker locker(&friendListLock);
    do_once(init)
        initCache(theFriendList, "friend_list", defaultFriendListCacheSize);
    int sz = 0;
    foreach (const Tools::Friend &f, *list)
        sz += f.name.length() * 2 + f.title.length() * 2 + f.url.length() * 2;
    if (theFriendList.maxCost() < sz)
        return false;
    theFriendList.insert("x", list, sz);
    return true;
}

bool cacheIpBanInfoList(IpBanInfoList *list)
{
    if (!list)
        return false;
    QWriteLocker locker(&ipBanInfoListLock);
    do_once(init)
        initCache(theIpBanInfoList, "ip_ban_info_list", defaultIpBanInfoListCacheSize);
    int sz = list->size() * 2 * sizeof(int);
    if (theIpBanInfoList.maxCost() < sz)
        return false;
    theIpBanInfoList.insert("x", list, sz);
    return true;
}

bool cacheNews(const QLocale &locale, QStringList *news)
{
    if (!news)
        return false;
    QWriteLocker locker(&newsLock);
    do_once(init)
        initCache(theNews, "news", defaultNewsCacheSize);
    int sz = 0;
    foreach (const QString &r, *news)
        sz = r.length() * 2;
    if (theNews.maxCost() < sz)
        return false;
    theNews.insert(locale.name(), news, sz);
    return true;
}

bool cachePost(const QString &boardName, quint64 postNumber, Content::Post *post)
{
    if (boardName.isEmpty() || !postNumber || !post)
        return false;
    QWriteLocker locker(&postsLock);
    do_once(init)
        initCache(thePosts, "posts", defaultPostsCacheSize);
    if (thePosts.maxCost() < 1)
        return false;
    thePosts.insert(boardName + "/" + QString::number(postNumber), post, 1);
    return true;
}

bool cacheRules(const QString &prefix, const QLocale &locale, QStringList *rules)
{
    if (prefix.isEmpty() || !rules)
        return false;
    QWriteLocker locker(&rulesLock);
    do_once(init)
        initCache(theRules, "rules", defaultRulesCacheSize);
    int sz = 0;
    foreach (const QString &r, *rules)
        sz = r.length() * 2;
    if (theRules.maxCost() < sz)
        return false;
    theRules.insert(prefix + "/" + locale.name(), rules, sz);
    return true;
}

File *cacheStaticFile(const QString &path, const QByteArray &file)
{
    if (path.isEmpty())
        return 0;
    QWriteLocker locker(&staticFilesLock);
    do_once(init)
        initCache(staticFiles, "static_files", defaultStaticFilesCacheSize);
    if (staticFiles.maxCost() < (file.size() + 8))
        return 0;
    File *f = new File;
    f->data = file;
    f->msecsSinceEpoch = (QDateTime::currentMSecsSinceEpoch() / 1000) * 1000;
    staticFiles.insert(path, f, file.size() + 8);
    return f;
}

bool cacheThread(const QString &boardName, const QLocale &l, quint64 number, QString *content)
{
    if (boardName.isEmpty() || !number || !content)
        return false;
    QWriteLocker locker(&threadsLock);
    do_once(init)
        initCache(boards, "threads", defaultThreadsCacheSize);
    int sz = content->length() * 2;
    if (threads.maxCost() < sz)
        return false;
    threads.insert(boardName + "/" + l.name() + "/" + QString::number(number), content, sz);
    return true;
}

bool cacheTranslator(const QString &name, const QLocale &locale, BTranslator *t)
{
    if (name.isEmpty() || !t)
        return false;
    QWriteLocker locker(&translatorsLock);
    do_once(init)
        initCache(translators, "translators", defaultTranslationsCacheSize);
    if (translators.maxCost() < 1)
        return false;
    translators.insert(name + "_" + locale.name(), t, 1);
    return true;
}

void clearBoards()
{
    QWriteLocker locker(&boardsLock);
    boards.clear();
}

bool clearCache(const QString &name, QString *err, const QLocale &l)
{
    ClearCacheFunction f = availableClearCacheFunctions().value(name);
    TranslatorQt tq(l);
    if (!f)
        return bRet(err, tq.translate("clearCache", "No such cache", "error"), false);
    f();
    return bRet(err, QString(), true);
}

void clearCustomContent()
{
    QWriteLocker locker(&customContentLock);
    theCustomContent.clear();
}

void clearCustomLinks()
{
    QWriteLocker locker(&customLinksLock);
    theCustomLinks.clear();
}

void clearDynamicFilesCache()
{
    QWriteLocker locker(&dynamicFilesLock);
    dynamicFiles.clear();
}

void clearFriendListCache()
{
    QWriteLocker locker(&friendListLock);
    theFriendList.clear();
}

void clearIpBanInfoListCache()
{
    QWriteLocker locker(&ipBanInfoListLock);
    theIpBanInfoList.clear();
}

void clearNewsCache()
{
    QWriteLocker locker(&newsLock);
    theNews.clear();
}

void clearPostsCache()
{
    QWriteLocker locker(&postsLock);
    thePosts.clear();
}

void clearRulesCache()
{
    QWriteLocker locker(&rulesLock);
    theRules.clear();
}

void clearStaticFilesCache()
{
    QWriteLocker locker(&staticFilesLock);
    staticFiles.clear();
}

void clearThreads()
{
    QWriteLocker locker(&threadsLock);
    threads.clear();
}

void clearTranslatorsCache()
{
    QWriteLocker locker(&translatorsLock);
    translators.clear();
}

QString *customContent(const QString &prefix, const QLocale &l)
{
    if (prefix.isEmpty())
        return 0;
    QReadLocker locker(&customContentLock);
    return theCustomContent.object(prefix + "/" + l.name());
}

CustomLinkInfoList *customLinks(const QLocale &l)
{
    QReadLocker locker(&customLinksLock);
    return theCustomLinks.object(l.name());
}

int defaultCacheSize(const QString &name)
{
    typedef QMap<QString, int> IntMap;
    init_once(IntMap, map, IntMap()) {
        map.insert("custom_content", defaultCustomContentCacheSize);
        map.insert("custom_links", defaultCustomLinksCacheSize);
        map.insert("dynamic_files", defaultDynamicFilesCacheSize);
        map.insert("friend_list", defaultFriendListCacheSize);
        map.insert("ip_ban_info_list", defaultIpBanInfoListCacheSize);
        map.insert("news", defaultNewsCacheSize);
        map.insert("rules", defaultRulesCacheSize);
        map.insert("static_files", defaultStaticFilesCacheSize);
        map.insert("translators", defaultTranslationsCacheSize);
    }
    return map.value(name);
}

File *dynamicFile(const QString &path)
{
    if (path.isEmpty())
        return 0;
    QReadLocker locker(&dynamicFilesLock);
    return dynamicFiles.object(path);
}

Tools::FriendList *friendList()
{
    QReadLocker locker(&friendListLock);
    return theFriendList.object("x");
}

IpBanInfoList *ipBanInfoList()
{
    QReadLocker locker(&ipBanInfoListLock);
    return theIpBanInfoList.object("x");
}

QStringList *news(const QLocale &locale)
{
    QReadLocker locker(&newsLock);
    return theNews.object(locale.name());
}

Content::Post *post(const QString &boardName, quint64 postNumber)
{
    if (boardName.isEmpty() || !postNumber)
        return 0;
    QReadLocker locker(&postsLock);
    return thePosts.object(boardName + "/" + QString::number(postNumber));
}

void removePost(const QString &boardName, quint64 postNumber)
{
    if (boardName.isEmpty() || !postNumber)
        return;
    QWriteLocker locker(&postsLock);
    thePosts.remove(boardName + "/" + QString::number(postNumber));
}

QStringList *rules(const QLocale &locale, const QString &prefix)
{
    if (prefix.isEmpty())
        return 0;
    QReadLocker locker(&rulesLock);
    return theRules.object(prefix + "/" + locale.name());
}

void setBoardsMaxCacheSize(int size)
{
    if (size < 0)
        return;
    QWriteLocker locker(&boardsLock);
    boards.setMaxCost(size);
}

void setCustomContentMaxCacheSize(int size)
{
    if (size < 0)
        return;
    QWriteLocker locker(&customContentLock);
    theCustomContent.setMaxCost(size);
}

void setCustomLinksMaxCacheSize(int size)
{
    if (size < 0)
        return;
    QWriteLocker locker(&customLinksLock);
    theCustomLinks.setMaxCost(size);
}

void setDynamicFilesMaxCacheSize(int size)
{
    if (size < 0)
        return;
    QWriteLocker locker(&dynamicFilesLock);
    dynamicFiles.setMaxCost(size);
}

void setFriendListMaxCacheSize(int size)
{
    if (size < 0)
        return;
    QWriteLocker locker(&friendListLock);
    theFriendList.setMaxCost(size);
}

void setIpBanInfoListMaxCacheSize(int size)
{
    if (size < 0)
        return;
    QWriteLocker locker(&ipBanInfoListLock);
    theIpBanInfoList.setMaxCost(size);
}

bool setMaxCacheSize(const QString &name, int size, QString *err, const QLocale &l)
{
    TranslatorQt tq(l);
    if (size < 0)
        return bRet(err, tq.translate("setMaxCacheSize", "Invalid cache size", "error"), false);
    SetMaxCacheSizeFunction f = availableSetMaxCacheSizeFunctions().value(name);
    if (!f)
        return bRet(err, tq.translate("setMaxCacheSize", "No such cache", "error"), false);
    f(size);
    return bRet(err, QString(), true);
}

void setNewsMaxCacheSize(int size)
{
    if (size < 0)
        return;
    QWriteLocker locker(&newsLock);
    theNews.setMaxCost(size);
}


void setPostsMaxCacheSize(int size)
{
    if (size < 0)
        return;
    QWriteLocker locker(&postsLock);
    thePosts.setMaxCost(size);
}

void setRulesMaxCacheSize(int size)
{
    if (size < 0)
        return;
    QWriteLocker locker(&rulesLock);
    theRules.setMaxCost(size);
}

void setStaticFilesMaxCacheSize(int size)
{
    if (size < 0)
        return;
    QWriteLocker locker(&staticFilesLock);
    staticFiles.setMaxCost(size);
}

void setThreadsMaxCacheSize(int size)
{
    if (size < 0)
        return;
    QWriteLocker locker(&threadsLock);
    threads.setMaxCost(size);
}

void setTranslatorsMaxCacheSize(int size)
{
    if (size < 0)
        return;
    QWriteLocker locker(&translatorsLock);
    translators.setMaxCost(size);
}

File *staticFile(const QString &path)
{
    if (path.isEmpty())
        return 0;
    QReadLocker locker(&staticFilesLock);
    return staticFiles.object(path);
}

QString *thread(const QString &boardName, const QLocale &l, quint64 number)
{
    if (boardName.isEmpty() || !number)
        return 0;
    QReadLocker locker(&threadsLock);
    return threads.object(boardName + "/" + l.name() + "/" + QString::number(number));
}

BTranslator *translator(const QString &name, const QLocale &locale)
{
    if (name.isEmpty())
        return 0;
    QReadLocker locker(&translatorsLock);
    return translators.object(name + "_" + locale.name());
}

}
