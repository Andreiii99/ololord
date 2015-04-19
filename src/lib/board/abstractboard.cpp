#include "abstractboard.h"

#include "board/cgboard.h"
#include "board/configurableboard.h"
#include "board/echoboard.h"
#include "board/mlpboard.h"
#include "board/prboard.h"
#include "board/rpgboard.h"
#include "cache.h"
#include "controller/baseboard.h"
#include "controller/board.h"
#include "controller/controller.h"
#include "controller/rules.h"
#include "controller/thread.h"
#include "database.h"
#include "plugin/global/boardfactoryplugininterface.h"
#include "settingslocker.h"
#include "stored/postcounter.h"
#include "stored/postcounter-odb.hxx"
#include "stored/thread.h"
#include "stored/thread-odb.hxx"
#include "tools.h"
#include "transaction.h"
#include "translator.h"

#include <BCoreApplication>
#include <BDirTools>
#include <BeQt>
#include <BPluginInterface>
#include <BPluginWrapper>
#include <BSettingsNode>
#include <BTerminal>
#include <BTranslation>

#include <QBuffer>
#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QMutexLocker>
#include <QScopedPointer>
#include <QSet>
#include <QSettings>
#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <QtAlgorithms>
#include <QVariant>
#include <QVariantMap>

#include <cppcms/application.h>
#include <cppcms/http_request.h>
#include <cppcms/json.h>

#include <odb/database.hxx>
#include <odb/connection.hxx>
#include <odb/qt/lazy-ptr.hxx>
#include <odb/query.hxx>
#include <odb/result.hxx>
#include <odb/transaction.hxx>

#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>

#include <ostream>
#include <string>
#include <vector>

#include <fstream>

static void scaleThumbnail(QImage &img, AbstractBoard::FileTransaction &ft)
{
    ft.setMainFileSize(img.height(), img.width());
    if (img.height() > 200 || img.width() > 200)
        img = img.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    ft.setThumbFileSize(img.height(), img.width());
}

static bool threadLessThan(const Thread &t1, const Thread &t2)
{
    if (t1.fixed() == t2.fixed())
        return t1.dateTime().toUTC() > t2.dateTime().toUTC();
    else if (t1.fixed())
        return true;
    else
        return false;
}

AbstractBoard::FileTransaction::FileTransaction(AbstractBoard *board) :
    Board(board)
{
    commited = false;
}

AbstractBoard::FileTransaction::~FileTransaction()
{
    if (commited)
        return;
    if (!Board)
        return;
    QString path = Tools::storagePath() + "/img/" + Board->name();
    foreach (const FileInfo &fi, minfos) {
        if (!fi.name.isEmpty())
            QFile::remove(path + "/" + fi.name);
        if (!fi.thumbName.isEmpty())
            QFile::remove(path + "/" + fi.thumbName);
    }
}

void AbstractBoard::FileTransaction::commit()
{
    commited = true;
}

QList<AbstractBoard::FileInfo> AbstractBoard::FileTransaction::fileInfos() const
{
    return minfos;
}

void AbstractBoard::FileTransaction::addInfo(const QString &mainFileName, const QByteArray &hash,
                                             const QString &mimeType, int size)
{
    FileInfo fi;
    if (!mainFileName.isEmpty())
        fi.name = QFileInfo(mainFileName).fileName();
    fi.hash = hash;
    fi.mimeType = mimeType;
    fi.size = size;
    minfos << fi;
}

void AbstractBoard::FileTransaction::setMainFile(const QString &fn, const QByteArray &hash, const QString &mimeType,
                                                 int size)
{
    if (fn.isEmpty())
        return;
    if (minfos.isEmpty())
        return;
    FileInfo &fi = minfos.last();
    fi.name = QFileInfo(fn).fileName();
    fi.hash = hash;
    fi.mimeType = mimeType;
    fi.size = size;
}

void AbstractBoard::FileTransaction::setMainFileSize(int height, int width)
{
    if (minfos.isEmpty())
        return;
    FileInfo &fi = minfos.last();
    fi.height = height;
    fi.width = width;
}

void AbstractBoard::FileTransaction::setThumbFile(const QString &fn)
{
    if (fn.isEmpty())
        return;
    if (minfos.isEmpty())
        return;
    minfos.last().thumbName = Tools::isSpecialThumbName(fn) ? fn : QFileInfo(fn).fileName();
}

void AbstractBoard::FileTransaction::setThumbFileSize(int height, int width)
{
    if (minfos.isEmpty())
        return;
    FileInfo &fi = minfos.last();
    fi.thumbHeight = height;
    fi.thumbWidth = width;
}

QMap<QString, AbstractBoard *> AbstractBoard::boards;
bool AbstractBoard::boardsInitialized = false;
QMutex AbstractBoard::boardsMutex(QMutex::Recursive);
const QString AbstractBoard::defaultFileTypes = "audio/mpeg,audio/ogg,audio/wav,image/gif,image/jpeg,image/png,"
                                                "video/mp4,video/ogg,video/webm";

AbstractBoard::AbstractBoard() :
    captchaQuotaMutex(QMutex::Recursive)
{
    //
}

AbstractBoard::~AbstractBoard()
{
    //
}

AbstractBoard *AbstractBoard::board(const QString &name)
{
    initBoards();
    boardsMutex.lock();
    AbstractBoard *b = boards.value(name);
    boardsMutex.unlock();
    return (b && b->isEnabled()) ? b : 0;
}

AbstractBoard::BoardInfoList AbstractBoard::boardInfos(const QLocale &l, bool includeHidden)
{
    initBoards();
    BoardInfoList list;
    boardsMutex.lock();
    foreach (const QString &key, boards.keys()) {
        AbstractBoard *board = boards.value(key);
        if (!board || !board->isEnabled() || (!includeHidden && board->isHidden()))
            continue;
        BoardInfo info;
        info.name = Tools::toStd(board->name());
        info.title = Tools::toStd(boards.value(key)->title(l));
        list.push_back(info);
    }
    boardsMutex.unlock();
    return list;
}

QStringList AbstractBoard::boardNames(bool includeHidden)
{
    initBoards();
    boardsMutex.lock();
    QStringList list = boards.keys();
    boardsMutex.unlock();
    foreach (int i, bRangeR(list.size() - 1, 0)) {
        AbstractBoard *board = boards.value(list.at(i));
        if (!board || !board->isEnabled() || (!includeHidden && board->isHidden()))
            list.removeAt(i);
    }
    return list;
}

void AbstractBoard::reloadBoards()
{
    initBoards(true);
}

void AbstractBoard::restoreCaptchaQuota(const QByteArray &data)
{
    QVariantMap m = BeQt::deserialize(data).toMap();
    QMutexLocker locker(&boardsMutex);
    foreach (const QString &boardName, m.keys()) {
        AbstractBoard *board = boards.value(boardName);
        if (!board)
            continue;
        QVariantMap mm = m.value(boardName).toMap();
        QMutexLocker lockerQuota(&board->captchaQuotaMutex);
        board->captchaQuotaMap.clear();
        foreach (const QString &ip, mm.keys()) {
            bool ok = false;
            unsigned int q = mm.value(ip).toUInt(&ok);
            if (!ok || !q)
                continue;
            board->captchaQuotaMap.insert(ip, q);
        }
    }
}

QByteArray AbstractBoard::saveCaptchaQuota()
{
    QVariantMap m;
    QMutexLocker locker(&boardsMutex);
    foreach (AbstractBoard *board, boards.values()) {
        QVariantMap mm;
        QMutexLocker lockerQuota(&board->captchaQuotaMutex);
        foreach (const QString &ip, board->captchaQuotaMap.keys())
            mm.insert(ip, board->captchaQuotaMap.value(ip));
        m.insert(board->name(), mm);
    }
    return BeQt::serialize(m);
}

unsigned int AbstractBoard::archiveLimit() const
{
    SettingsLocker s;
    return s->value("Board/" + name() + "/archive_limit", s->value("Board/archive_limit", 0)).toUInt();
}

QString AbstractBoard::bannerFileName() const
{
    QString path = BDirTools::findResource("static/img/banner", BDirTools::AllResources);
    if (path.isEmpty())
        return "";
    QStringList fns = QDir(path).entryList(QStringList() << (name() + "*.*"), QDir::Files);
    if (fns.isEmpty())
        return "";
    qsrand((uint) QDateTime::currentMSecsSinceEpoch());
    return fns.at(qrand() % fns.size());
}

bool AbstractBoard::beforeStoringEditedPost(const cppcms::http::request &, cppcms::json::value &, Post &, Thread &,
                                            QString *)
{
    return true;
}

bool AbstractBoard::beforeStoringNewPost(const cppcms::http::request &, Post *, const Tools::PostParameters &, bool,
                                         QString *, QString *)
{
    return true;
}

unsigned int AbstractBoard::bumpLimit() const
{
    SettingsLocker s;
    return s->value("Board/" + name() + "/bump_limit", s->value("Board/bump_limit", 500)).toUInt();
}

unsigned int AbstractBoard::captchaQuota() const
{
    SettingsLocker s;
    return s->value("Board/" + name() + "/captcha_quota", s->value("Board/captcha_quota", 0)).toUInt();
}

unsigned int AbstractBoard::captchaQuota(const QString &ip) const
{
    if (ip.isEmpty())
        return 0;
    QMutexLocker locker(&captchaQuotaMutex);
    return captchaQuotaMap.value(ip);
}

unsigned int AbstractBoard::captchaQuota(const cppcms::http::request &req) const
{
    return captchaQuota(Tools::userIp(req));
}

void AbstractBoard::captchaSolved(const QString &ip)
{
    if (ip.isEmpty())
        return;
    QMutexLocker locker(&captchaQuotaMutex);
    captchaQuotaMap[ip] = captchaQuota();
}

void AbstractBoard::captchaUsed(const QString &ip)
{
    if (ip.isEmpty())
        return;
    QMutexLocker locker(&captchaQuotaMutex);
    unsigned int &q = captchaQuotaMap[ip];
    if (!q)
        captchaQuotaMap.remove(ip);
    else
        --q;
    if (!q)
        captchaQuotaMap.remove(ip);
}

void AbstractBoard::createPost(cppcms::application &app)
{
    cppcms::http::request &req = app.request();
    QString logTarget = name();
    if (!Controller::testBan(app, Controller::WriteAction, name()))
        return Tools::log(app, "create_post", "fail:ban", logTarget);
    Tools::PostParameters params = Tools::postParameters(req);
    Tools::FileList files = Tools::postFiles(req);
    QString err;
    if (!Controller::testParams(this, app, params, files, true, &err))
        return Tools::log(app, "create_post", "fail:" + err, logTarget);
    TranslatorQt tq(req);
    if (!postingEnabled()) {
        QString err = tq.translate("AbstractBoard", "Posting disabled", "error");
        Controller::renderError(app, err,
                                tq.translate("AbstractBoard", "Posting is disabled for this board","description"));
        Tools::log(app, "create_post", "fail:" + err, logTarget);
        return;
    }
    QString desc;
    Database::CreatePostParameters p(req, params, files, tq.locale());
    p.bumpLimit = bumpLimit();
    p.postLimit = postLimit();
    p.error = &err;
    p.description = &desc;
    quint64 postNumber = 0L;
    if (!Database::createPost(p, &postNumber)) {
        Controller::renderError(app, err, desc);
        Tools::log(app, "create_post", "fail:" + err, logTarget);
        return;
    }
    Controller::renderSuccessfulPost(app, postNumber, p.referencedPosts);
    Tools::log(app, "create_post", "success", logTarget);
}

void AbstractBoard::createThread(cppcms::application &app)
{
    cppcms::http::request &req = app.request();
    QString logTarget = name();
    if (!Controller::testBan(app, Controller::WriteAction, name()))
        return Tools::log(app, "create_thread", "fail:ban", logTarget);
    Tools::PostParameters params = Tools::postParameters(req);
    Tools::FileList files = Tools::postFiles(req);
    QString err;
    if (!Controller::testParams(this, app, params, files, false, &err))
        return Tools::log(app, "create_thread", "fail:" + err, logTarget);
    TranslatorQt tq(req);
    if (!postingEnabled()) {
        QString err = tq.translate("AbstractBoard", "Posting disabled", "error");
        Controller::renderError(app, err,
                                tq.translate("AbstractBoard", "Posting is disabled for this board", "description"));
        Tools::log(app, "create_thread", "fail:" + err, logTarget);
        return;
    }
    QString desc;
    Database::CreateThreadParameters p(req, params, files, tq.locale());
    p.archiveLimit = archiveLimit();
    p.threadLimit = threadLimit();
    p.error = &err;
    p.description = &desc;
    quint64 threadNumber = Database::createThread(p);
    if (!threadNumber) {
        Controller::renderError(app, err, desc);
        Tools::log(app, "create_thread", "fail:" + err, logTarget);
        return;
    }
    Controller::renderSuccessfulThread(app, threadNumber);
    Tools::log(app, "create_thread", "success", logTarget);
}

QString AbstractBoard::defaultUserName(const QLocale &l) const
{
    return TranslatorQt(l).translate("AbstractBoard", "Anonymous", "defaultUserName");
}

bool AbstractBoard::draftsEnabled() const
{
    SettingsLocker s;
    return s->value("Board/" + name() + "/drafts_enabled", s->value("Board/drafts_enabled", true)).toBool();
}

void AbstractBoard::handleBoard(cppcms::application &app, unsigned int page)
{
    QString logTarget = name() + "/" + QString::number(page);
    if (!Controller::testBan(app, Controller::ReadAction, name()))
        return Tools::log(app, "board", "fail:ban", logTarget);
    TranslatorQt tq(app.request());
    TranslatorStd ts(app.request());
    QString viewName;
    QScopedPointer<Content::Board> cc(createBoardController(app.request(), viewName));
    if (cc.isNull()) {
        QString err = tq.translate("AbstractBoard", "Internal logic error", "description");
        Controller::renderError(app, tq.translate("AbstractBoard", "Internal error", "error"), err);
        Tools::log(app, "board", "fail:" + err, logTarget);
        return;
    }
    if (viewName.isEmpty())
        viewName = "board";
    Content::Board &c = *cc;
    unsigned int pageCount = 0;
    bool postingEn = postingEnabled();
    try {
        Transaction t;
        if (!t) {
            QString err = tq.translate("AbstractBoard", "Internal database error", "description");
            Controller::renderError(app, tq.translate("AbstractBoard", "Internal error", "error"), err);
            Tools::log(app, "board", "fail:" + err, logTarget);
            return;
        }
        odb::query<Thread> q = odb::query<Thread>::board == name() && odb::query<Thread>::archived == false;
        QByteArray hashpass = Tools::hashpass(app.request());
        bool modOnBoard = Database::moderOnBoard(app.request(), name());
        QList<Thread> list = Database::query<Thread, Thread>(q);
        int lvl = Database::registeredUserLevel(app.request());
        foreach (int i, bRangeR(list.size() - 1, 0)) {
            Post opPost = *list.at(i).posts().first().load();
            if (opPost.draft() && opPost.hashpass() != hashpass
                    && (!modOnBoard || Database::registeredUserLevel(opPost.hashpass()) >= lvl)) {
                list.removeAt(i);
            }
        }
        qSort(list.begin(), list.end(), &threadLessThan);
        pageCount = (list.size() / threadsPerPage()) + ((list.size() % threadsPerPage()) ? 1 : 0);
        if (!pageCount)
            pageCount = 1;
        if (page >= pageCount) {
            Controller::renderNotFound(app);
            Tools::log(app, "board", "fail:not_found", logTarget);
            return;
        }
        foreach (const Thread &tt, list.mid(page * threadsPerPage(), threadsPerPage())) {
            Content::Board::Thread thread;
            const Thread::Posts &posts = tt.posts();
            thread.bumpLimit = bumpLimit();
            thread.closed = !tt.postingEnabled();
            thread.fixed = tt.fixed();
            thread.postLimit = postLimit();
            thread.postCount = posts.size();
            thread.postingEnabled = postingEn && tt.postingEnabled();
            bool ok = false;
            QString err;
            thread.opPost = toController(*posts.first().load(), app.request(), &ok, &err);
            if (!ok) {
                Controller::renderError(app, tq.translate("AbstractBoard", "Internal error", "error"), err);
                Tools::log(app, "board", "fail:" + err, logTarget);
                return;
            }
            unsigned int maxPosts = Tools::maxInfo(Tools::MaxLastPosts, name());
            foreach (int i, bRangeR(posts.size() - 1, 1)) {
                Post post = *posts.at(i).load();
                if (post.draft() && hashpass != post.hashpass()
                        && (!modOnBoard || Database::registeredUserLevel(post.hashpass()) >= lvl)) {
                    continue;
                }
                thread.lastPosts.push_front(toController(post, app.request(), &ok, &err));
                if (!ok) {
                    Controller::renderError(app, tq.translate("AbstractBoard", "Internal error", "error"), err);
                    Tools::log(app, "board", "fail:" + err, logTarget);
                    return;
                }
                if (thread.lastPosts.size() >= maxPosts)
                    break;
            }
            thread.hidden = (Tools::cookieValue(app.request(), "postHidden" + name()
                                                + QString::number(tt.number())) == "true");
            c.threads.push_back(thread);
        }
        t.commit();
    }  catch (const odb::exception &e) {
        QString err = Tools::fromStd(e.what());
        Controller::renderError(app, tq.translate("AbstractBoard", "Internal error", "error"), err);
        Tools::log(app, "board", "fail:" + err, logTarget);
        return;
    }
    Controller::initBaseBoard(c, app.request(), this, postingEn, title(ts.locale()));
    c.boardRulesLinkText = ts.translate("AbstractBoard", "Borad rules", "boardRulesLinkText");
    c.currentPage = page;
    c.omittedPostsText = ts.translate("AbstractBoard", "Posts omitted:", "omittedPostsText");
    foreach (int i, bRangeD(0, pageCount - 1))
        c.pages.push_back(i);
    c.toNextPageText = ts.translate("AbstractBoard", "Next page", "toNextPageText");
    c.toPreviousPageText = ts.translate("AbstractBoard", "Previous page", "toPreviousPageText");
    beforeRenderBoard(app.request(), cc.data());
    app.render(Tools::toStd(viewName), c);
    Tools::log(app, "board", "success", logTarget);
}

void AbstractBoard::handleRules(cppcms::application &app)
{
    QString logTarget = name();
    Content::Rules c;
    TranslatorQt tq(app.request());
    TranslatorStd ts(app.request());
    QString pageTitle = title(tq.locale()) + " - " + tq.translate("AbstractBoard", "rules", "pageTitle");
    Controller::initBase(c, app.request(), pageTitle);
    c.currentBoard.name = Tools::toStd(name());
    c.currentBoard.title = Tools::toStd(title(tq.locale()));
    c.noRulesText = ts.translate("AbstractBoard", "There are no specific rules for this board.", "noRulesText");;
    foreach (const QString &r, rules(ts.locale()))
        c.rules.push_back(Tools::toStd(r));
    app.render("rules", c);
    Tools::log(app, "board_rules", "success", logTarget);
}

void AbstractBoard::handleThread(cppcms::application &app, quint64 threadNumber)
{
    QString logTarget = name() + "/" + QString::number(threadNumber);
    if (!Controller::testBan(app, Controller::ReadAction, name()))
        return Tools::log(app, "thread", "fail:ban", logTarget);
    TranslatorQt tq(app.request());
    TranslatorStd ts(app.request());
    QString viewName;
    QScopedPointer<Content::Thread> cc(createThreadController(app.request(), viewName));
    if (cc.isNull()) {
        QString err = tq.translate("AbstractBoard", "Internal logic error", "description");
        Controller::renderError(app, tq.translate("AbstractBoard", "Internal error", "error"), err);
        Tools::log(app, "thread", "fail:" + err, logTarget);
        return;
    }
    if (viewName.isEmpty())
        viewName = "thread";
    Content::Thread &c = *cc;
    bool postingEn = postingEnabled();
    QString pageTitle;
    try {
        Transaction t;
        if (!t) {
            QString err = tq.translate("AbstractBoard", "Internal database error", "description");
            Controller::renderError(app, tq.translate("AbstractBoard", "Internal error", "error"), err);
            Tools::log(app, "thread", "fail:" + err, logTarget);
            return;
        }
        odb::query<Thread> q = odb::query<Thread>::board == name() && odb::query<Thread>::number == threadNumber;
        q = q && odb::query<Thread>::archived == false;
        Database::Result<Thread> thread = Database::queryOne<Thread, Thread>(q);
        QByteArray hashpass = Tools::hashpass(app.request());
        bool modOnBoard = Database::moderOnBoard(app.request(), name());
        if (thread.error) {
            QString err = tq.translate("AbstractBoard", "Internal database error", "description");
            Controller::renderError(app, tq.translate("AbstractBoard", "Internal error", "error"), err);
            Tools::log(app, "thread", "fail:" + err, logTarget);
            return;
        }
        if (!thread) {
            Controller::renderNotFound(app);
            Tools::log(app, "thread", "fail:not_found", logTarget);
            return;
        }
        const Thread::Posts &posts = thread->posts();
        if (posts.isEmpty()) {
            QString err = tq.translate("AbstractBoard", "Internal database error", "description");
            Controller::renderError(app, tq.translate("AbstractBoard", "Internal error", "error"), err);
            Tools::log(app, "thread", "fail:" + err, logTarget);
            return;
        }
        c.closed = !thread->postingEnabled();
        c.fixed = thread->fixed();
        c.id = thread->id();
        c.number = thread->number();
        Post opPost = *posts.first().load();
        int lvl = Database::registeredUserLevel(app.request());
        if (opPost.draft() && hashpass != opPost.hashpass()
                && (!modOnBoard || Database::registeredUserLevel(opPost.hashpass()) >= lvl)) {
            Controller::renderNotFound(app);
            Tools::log(app, "thread", "fail:not_found", logTarget);
            return;
        }
        pageTitle = opPost.subject();
        if (pageTitle.isEmpty()) {
            pageTitle = opPost.text().replace(QRegExp("\\r?\\n+"), " ").replace(QRegExp("<[^<>]+>"), "");
            pageTitle.replace("&amp;", "&");
            pageTitle.replace("&lt;", "<");
            pageTitle.replace("&gt;", ">");
            pageTitle.replace("&nbsp;", " ");
            pageTitle.replace("&quot;", "\"");
            if (pageTitle.length() > 50)
                pageTitle = pageTitle.left(47) + "...";
        }
        postingEn = postingEn && thread->postingEnabled();
        bool ok = false;
        QString err;
        c.opPost = toController(*posts.first().load(), app.request(), &ok, &err);
        if (!ok)
            return Controller::renderError(app, tq.translate("AbstractBoard", "Internal error", "error"), err);
        foreach (int j, bRangeD(1, posts.size() - 1)) {
            Post post = *posts.at(j).load();
            if (post.draft() && hashpass != post.hashpass()
                    && (!modOnBoard || Database::registeredUserLevel(post.hashpass()) >= lvl)) {
                continue;
            }
            c.posts.push_back(toController(post, app.request(), &ok, &err));
            if (!ok) {
                Controller::renderError(app, tq.translate("AbstractBoard", "Internal error", "error"), err);
                Tools::log(app, "thread", "fail:" + err, logTarget);
                return;
            }
        }
        t.commit();
    }  catch (const odb::exception &e) {
        QString err = Tools::fromStd(e.what());
        Controller::renderError(app, tq.translate("AbstractBoard", "Internal error", "error"), err);
        Tools::log(app, "thread", "fail:" + err, logTarget);
        return;
    }
    Controller::initBaseBoard(c, app.request(), this, postingEn, pageTitle, threadNumber);
    c.autoUpdateEnabled = !Tools::cookieValue(app.request(), "auto_update").compare("true", Qt::CaseInsensitive);
    c.autoUpdateText = ts.translate("AbstractBoard", "Auto update", "autoUpdateText");
    c.backText = ts.translate("AbstractBoard", "Back", "backText");
    c.bumpLimit = bumpLimit();
    c.newPostsText = ts.translate("AbstractBoard", "New posts:", "newPostsText");
    c.noNewPostsText = ts.translate("AbstractBoard", "No new posts", "noNewPostsText");
    c.postLimit = postLimit();
    c.hidden = (Tools::cookieValue(app.request(), "postHidden" + name() + QString::number(threadNumber)) == "true");
    c.updateThreadText = ts.translate("AbstractBoard", "Update thread", "updateThreadText");
    beforeRenderThread(app.request(), cc.data());
    app.render(Tools::toStd(viewName), c);
    Tools::log(app, "thread", "success", logTarget);
}

bool AbstractBoard::isCaptchaValid(const cppcms::http::request &req, const Tools::PostParameters &params,
                                   QString &error) const
{
    QString captcha = params.value("g-recaptcha-response");
    TranslatorQt tq(req);
    if (captcha.isEmpty())
        return bRet(&error, tq.translate("AbstractBoard", "Captcha is empty", "error"), false);
    try {
        curlpp::Cleanup curlppCleanup;
        Q_UNUSED(curlppCleanup)
        QString url = "https://www.google.com/recaptcha/api/siteverify?secret=%1&response=%2&remoteip=%3";
        url = url.arg(SettingsLocker()->value("Site/captcha_private_key").toString()).arg(captcha);
        url = url.arg(Tools::userIp(req));
        curlpp::Easy request;
        request.setOpt(curlpp::options::Url(Tools::toStd(url)));
        std::ostringstream os;
        os << request;
        QString result = Tools::fromStd(os.str());
        result.remove(QRegExp(".*\"success\":\\s*\"?"));
        result.remove(QRegExp("\"?\\,?\\s+.+"));
        if (result.compare("true", Qt::CaseInsensitive))
            return bRet(&error, tq.translate("AbstractBoard", "Captcha is incorrect", "error"), false);
        return true;
    } catch (curlpp::RuntimeError &e) {
        return bRet(&error, Tools::fromStd(e.what()), false);
    } catch(curlpp::LogicError &e) {
        return bRet(&error, Tools::fromStd(e.what()), false);
    }
}

bool AbstractBoard::isEnabled() const
{
    return SettingsLocker()->value("Board/" + name() + "/enabled", true).toBool();
}

bool AbstractBoard::isFileTypeSupported(const QString &mimeType) const
{
    if (mimeType.isEmpty())
        return false;
    QStringList fileTypes = supportedFileTypes().split(',');
    foreach (const QString &ft, fileTypes) {
        if (QRegExp(ft, Qt::CaseInsensitive, QRegExp::Wildcard).exactMatch(mimeType))
            return true;
    }
    return false;
}

bool AbstractBoard::isFileTypeSupported(const QByteArray &data) const
{
    return isFileTypeSupported(Tools::mimeType(data));
}

bool AbstractBoard::isHidden() const
{
    return SettingsLocker()->value("Board/" + name() + "/hidden", false).toBool();
}

QStringList AbstractBoard::postformRules(const QLocale &l) const
{
    return Tools::rules("rules/postform", l) + Tools::rules("rules/postform/" + name(), l);
}

bool AbstractBoard::postingEnabled() const
{
    SettingsLocker s;
    return s->value("Board/" + name() + "/posting_enabled", s->value("Board/posting_enabled", true)).toBool();
}

unsigned int AbstractBoard::postLimit() const
{
    SettingsLocker s;
    return s->value("Board/" + name() + "/post_limit", s->value("Board/post_limit", 1000)).toUInt();
}

QStringList AbstractBoard::rules(const QLocale &l) const
{
    return Tools::rules("rules/board", l) + Tools::rules("rules/board/" + name(), l);
}

bool AbstractBoard::saveFile(const Tools::File &f, FileTransaction &ft)
{
#if defined(Q_OS_WIN)
    static const QString FfmpegDefault = "ffmpeg.exe";
#elif defined(Q_OS_UNIX)
    static const QString FfmpegDefault = "ffmpeg";
#endif
    typedef QMap<QString, QString> StringMap;
    init_once(StringMap, suffixes, StringMap()) {
        suffixes.insert("audio/mpeg", "mpeg");
        suffixes.insert("audio/ogg", "ogg");
        suffixes.insert("audio/wav", "wav");
        suffixes.insert("image/gif", "gif");
        suffixes.insert("image/jpeg", "jpeg");
        suffixes.insert("image/png", "png");
        suffixes.insert("video/mp4", "mp4");
        suffixes.insert("video/ogg", "ogg");
        suffixes.insert("video/webm", "webm");
    }
    bool ok = false;
    QString mimeType = Tools::mimeType(f.data, &ok);
    if (!ok)
        return false;
    if (!isFileTypeSupported(mimeType))
        return false;
    QString storagePath = Tools::storagePath();
    if (storagePath.isEmpty())
        return false;
    QString path = storagePath + "/img/" + name();
    if (!BDirTools::mkpath(path))
        return false;
    QString dt = QString::number(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch());
    QString suffix = QFileInfo(f.fileName).suffix();
    if (suffix.isEmpty())
        suffix = suffixes.value(mimeType);
    QString sfn = path + "/" + dt + "." + suffix;
    ft.addInfo(sfn, QCryptographicHash::hash(f.data, QCryptographicHash::Sha1), mimeType, f.data.size());
    if (!BDirTools::writeFile(sfn, f.data))
        return false;
    QImage img;
    if (Tools::isAudioType(mimeType)) {
        ft.setMainFileSize(0, 0);
        ft.setThumbFile(mimeType);
        ft.setThumbFileSize(200, 200);
    } else if (Tools::isVideoType(mimeType)) {
        QString ffmpeg = SettingsLocker()->value("System/ffmpeg_command", FfmpegDefault).toString();
        QStringList args = QStringList() << "-i" << QDir::toNativeSeparators(sfn) << "-vframes" << "1"
                                         << (dt + "s.png");
        if (!BeQt::execProcess(path, ffmpeg, args, BeQt::Second, 5 * BeQt::Second)) {
            ft.setThumbFile(path + "/" + dt + "s.png");
            if (!img.load(path + "/" + dt + "s.png"))
                return false;
            scaleThumbnail(img, ft);
            if (!img.save(path + "/" + dt + "s.png", "png"))
                return false;
        } else {
            ft.setThumbFile(mimeType);
            ft.setThumbFileSize(200, 200);
        }
    } else {
        QByteArray data = f.data;
        QBuffer buff(&data);
        buff.open(QIODevice::ReadOnly);
        if (!img.load(&buff, suffix.toLower().toLatin1().data()))
            return false;
        scaleThumbnail(img, ft);
        if (!mimeType.compare("image/gif", Qt::CaseInsensitive))
            suffix = "png";
        ft.setThumbFile(path + "/" + dt + "s." + suffix);
        if (!img.save(path + "/" + dt + "s." + suffix, suffix.toLower().toLatin1().data()))
            return false;
    }
    return true;
}

bool AbstractBoard::showWhois() const
{
    return false;
}

QString AbstractBoard::supportedFileTypes() const
{
    SettingsLocker s;
    return s->value("Board/" + name() + "/supported_file_types",
                    s->value("Board/supported_file_types", defaultFileTypes)).toString();
}

bool AbstractBoard::testParams(const Tools::PostParameters &params, const Tools::FileList &files, bool post,
                               const QLocale &l, QString *error) const
{
    TranslatorQt tq(l);
    if (params.value("email").length() > int(Tools::maxInfo(Tools::MaxEmailFieldLength, name())))
        return bRet(error, tq.translate("AbstractBoard", "E-mail is too long", "description"), false);
    else if (params.value("name").length() > int(Tools::maxInfo(Tools::MaxNameFieldLength, name())))
        return bRet(error, tq.translate("AbstractBoard", "Name is too long", "description"), false);
    else if (params.value("subject").length() > int(Tools::maxInfo(Tools::MaxSubjectFieldLength, name())))
        return bRet(error, tq.translate("AbstractBoard", "Subject is too long", "description"), false);
    else if (params.value("text").length() > int(Tools::maxInfo(Tools::MaxTextFieldLength, name())))
        return bRet(error, tq.translate("AbstractBoard", "Comment is too long", "description"), false);
    else if (params.value("password").length() > int(Tools::maxInfo(Tools::MaxPasswordFieldLength, name())))
        return bRet(error, tq.translate("AbstractBoard", "Password is too long", "description"), false);
    QStringList fileHashes = params.value("fileHashes").split(',', QString::SkipEmptyParts);
    int fileCount = files.size() + fileHashes.size();
    int maxFileSize = Tools::maxInfo(Tools::MaxFileSize, name());
    int maxFileCount = int(Tools::maxInfo(Tools::MaxFileCount, name()));
    if (!post && maxFileCount && !fileCount) {
        return bRet(error, tq.translate("AbstractBoard", "Attempt to create a thread without attaching a file",
                                        "error"), false);
    }
    if (params.value("text").isEmpty() && !fileCount)
        return bRet(error, tq.translate("AbstractBoard", "Both file and comment are missing", "error"), false);
    if (maxFileCount && (fileCount > maxFileCount)) {
        return bRet(error, tq.translate("AbstractBoard", "Too many files", "error"), false);
    } else {
        foreach (const Tools::File &f, files) {
            if (f.data.size() > maxFileSize)
                return bRet(error, tq.translate("AbstractBoard", "File is too big", "error"), false);
            if (!isFileTypeSupported(f.data))
                return bRet(error, tq.translate("AbstractBoard", "File type is not supported", "error"), false);
        }
    }
    return bRet(error, QString(), true);
}

unsigned int AbstractBoard::threadLimit() const
{
    SettingsLocker s;
    return s->value("Board/" + name() + "/thread_limit", s->value("Board/thread_limit", 200)).toUInt();
}

unsigned int AbstractBoard::threadsPerPage() const
{
    SettingsLocker s;
    return s->value("Board/" + name() + "/threads_per_page", s->value("Board/threads_per_page", 20)).toUInt();
}

Content::Post AbstractBoard::toController(const Post &post, const cppcms::http::request &req, bool *ok,
                                          QString *error) const
{
    static const QString DateTimeFormat = "dd/MM/yyyy ddd hh:mm:ss";
    TranslatorQt tq(req);
    QString storagePath = Tools::storagePath();
    if (storagePath.isEmpty()) {
        return bRet(ok, false, error, tq.translate("AbstractBoard", "Internal file system error", "error"),
                    Content::Post());
    }
    Content::Post *p = Cache::post(name(), post.number());
    bool inCache = p;
    if (!p) {
        p = new Content::Post;
        p->bannedFor = post.bannedFor();
        p->email = Tools::toStd(post.email());
        p->number = post.number();
        p->rawSubject = Tools::toStd(post.subject());
        p->subject = p->rawSubject;
        p->subjectIsRaw = false;
        p->rawName = Tools::toStd(post.name());
        p->draft = post.draft();
        p->userData = post.userData();
        quint64 threadNumber = 0;
        try {
            Transaction t;
            if (!t) {
                return bRet(ok, false, error, tq.translate("AbstractBoard", "Internal database error", "error"),
                            Content::Post());
            }
            typedef QLazyWeakPointer< ::FileInfo > FileInfoWP;
            foreach (FileInfoWP fi, post.fileInfos()) {
                Content::File f;
                QSharedPointer< ::FileInfo > fis = fi.load();
                f.type = Tools::toStd(fis->mimeType());
                f.sourceName = Tools::toStd(fis->name());
                QString sz = QString::number(fis->size() / BeQt::Kilobyte) + "KB";
                f.sizeX = fis->width();
                f.sizeY = fis->height();
                f.thumbSizeX = fis->thumbWidth();
                f.thumbSizeY = fis->thumbHeight();
                if (f.sizeX >= 0 && f.sizeY >= 0)
                    sz += ", " + QString::number(f.sizeX) + "x" + QString::number(f.sizeY);
                f.thumbName = Tools::toStd(fis->thumbName());
                f.size = Tools::toStd(sz);
                p->files.push_back(f);
            }
            QSharedPointer<Thread> thread = post.thread().load();
            threadNumber = thread->number();
            bool op = (post.number() == threadNumber);
            p->fixed = op && thread->fixed();
            p->closed = op && !thread->postingEnabled();
            typedef QLazySharedPointer<PostReference> PostReferenceSP;
            foreach (PostReferenceSP reference, post.referencedBy()) {
                QSharedPointer<Post> rp = reference.load()->sourcePost().load();
                Content::Post::Ref ref;
                ref.boardName = Tools::toStd(rp->board());
                ref.postNumber = rp->number();
                ref.threadNumber = rp->thread().load()->number();
                p->referencedBy.push_back(ref);
            }
            foreach (PostReferenceSP reference, post.refersTo()) {
                QSharedPointer<Post> rp = reference.load()->targetPost().load();
                Content::Post::Ref ref;
                ref.boardName = Tools::toStd(rp->board());
                ref.postNumber = rp->number();
                ref.threadNumber = rp->thread().load()->number();
                p->refersTo.push_back(ref);
            }
            t.commit();
        } catch (const odb::exception &e) {
            return bRet(ok, false, error, Tools::fromStd(e.what()), Content::Post());
        }
        p->text = Tools::toStd(post.text());
        p->rawHtml = post.rawHtml();
        p->rawPostText = Tools::toStd(post.rawText());
        p->threadNumber = threadNumber;
        p->showRegistered = false;
        p->showTripcode = post.showTripcode();
        if (showWhois()) {
            QString countryCode = Tools::countryCode(post.posterIp());
            p->flagName = Tools::toStd(Tools::flagName(countryCode));
            if (!p->flagName.empty()) {
                p->countryName = Tools::toStd(Tools::countryName(countryCode));
                if (SettingsLocker()->value("Board/guess_city_name", true).toBool())
                    p->cityName = Tools::toStd(Tools::cityName(post.posterIp()));
            } else {
                p->flagName = "default.png";
                p->countryName = "Unknown country";
            }
        }
    }
    Content::Post pp = *p;
    if (!inCache && !Cache::cachePost(name(), post.number(), p))
        delete p;
    TranslatorStd ts(req);
    QLocale l = tq.locale();
    for (std::list<Content::File>::iterator i = pp.files.begin(); i != pp.files.end(); ++i)
        i->size = Tools::toStd(Tools::fromStd(i->size).replace("KB", tq.translate("AbstractBoard", "KB", "fileSize")));
    if (showWhois() && "Unknown country" == pp.countryName)
        pp.countryName = ts.translate("AbstractBoard", "Unknown country", "countryName");
    int regLvl = Database::registeredUserLevel(req);
    if (regLvl >= RegisteredUser::ModerLevel)
        pp.ip = Tools::toStd(post.posterIp());
    pp.ownPost = (post.posterIp() == Tools::userIp(req));
    pp.dateTime = Tools::toStd(l.toString(Tools::dateTime(post.dateTime(), req), DateTimeFormat));
    if (!post.modificationDateTime().isNull()) {
        pp.modificationDateTime = Tools::toStd(l.toString(Tools::dateTime(post.modificationDateTime(), req),
                                                          DateTimeFormat));
    }
    pp.hidden = (Tools::cookieValue(req, "postHidden" + name() + QString::number(post.number())) == "true");
    pp.name = Tools::toStd(Controller::toHtml(post.name()));
    if (pp.name.empty())
        pp.name = "<span class=\"userName\">" + Tools::toStd(Controller::toHtml(defaultUserName(l))) + "</span>";
    else
        pp.name = "<span class=\"userName\">" + pp.name + "</span>";
    pp.nameRaw = Tools::toStd(post.name());
    if (pp.nameRaw.empty())
        pp.nameRaw = Tools::toStd(defaultUserName(l));
    QByteArray hashpass = post.hashpass();
    if (!hashpass.isEmpty()) {
        int lvl = Database::registeredUserLevel(hashpass);
        QString name;
        pp.showRegistered = lvl >= RegisteredUser::UserLevel;
        if (!post.name().isEmpty()) {
            if (lvl >= RegisteredUser::AdminLevel)
                name = post.name();
            else if (lvl >= RegisteredUser::ModerLevel)
                name = "<span class=\"moderName\">" + Controller::toHtml(post.name()) + "</span>";
            else if (lvl >= RegisteredUser::UserLevel)
                name = "<span class=\"userName\">" + Controller::toHtml(post.name()) + "</span>";
        }
        pp.name = Tools::toStd(name);
        if (pp.name.empty())
            pp.name = "<span class=\"userName\">" + Tools::toStd(Controller::toHtml(defaultUserName(l))) + "</span>";
        QString s;
        hashpass += SettingsLocker()->value("Site/tripcode_salt").toString().toUtf8();
        QByteArray tripcode = QCryptographicHash::hash(hashpass, QCryptographicHash::Md5);
        foreach (int i, bRangeD(0, tripcode.size() - 1)) {
            QChar c(tripcode.at(i));
            if (c.isLetterOrNumber() || c.isPunct())
                s += c;
            else
                s += QString::number(uchar(tripcode.at(i)), 16);
        }
        pp.tripcode = Tools::toStd(s);
    }
    return bRet(ok, true, error, QString(), pp);
}

cppcms::json::object AbstractBoard::toJson(const Content::Post &post, const cppcms::http::request &/*req*/) const
{
    cppcms::json::object o;
    o["bannedFor"] = post.bannedFor;
    o["cityName"] = post.cityName;
    o["closed"] = post.closed;
    o["countryName"] = post.countryName;
    o["dateTime"] = post.dateTime;
    o["modificationDateTime"] = post.modificationDateTime;
    o["email"] = post.email;
    cppcms::json::array files;
    for (std::list<Content::File>::const_iterator i = post.files.begin(); i != post.files.end(); ++i) {
        const Content::File &file = *i;
        cppcms::json::object f;
        f["type"] = file.type;
        f["size"] = file.size;
        f["thumbSizeX"] = file.thumbSizeX;
        f["thumbSizeY"] = file.thumbSizeY;
        f["sizeX"] = file.sizeX;
        f["sizeY"] = file.sizeY;
        f["sourceName"] = file.sourceName;
        f["thumbName"] = file.thumbName;
        files.push_back(f);
    }
    o["files"] = files;
    o["fixed"] = post.fixed;
    o["flagName"] = post.flagName;
    o["hidden"] = post.hidden;
    o["ip"] = post.ip;
    o["name"] = post.name;
    o["nameRaw"] = post.nameRaw;
    o["number"] = post.number;
    o["showRegistered"] = post.showRegistered;
    o["showTripcode"] = post.showTripcode;
    o["threadNumber"] = post.threadNumber;
    o["subject"] = post.subject;
    o["subjectIsRaw"] = post.subjectIsRaw;
    o["draft"] = post.draft;
    o["rawName"] = post.rawName;
    o["rawSubject"] = post.rawSubject;
    o["text"] = post.text;
    o["rawPostText"] = post.rawPostText;
    o["tripcode"] = post.tripcode;
    o["ownPost"] = post.ownPost;
    cppcms::json::array refs;
    typedef Content::Post::Ref Ref;
    for (std::list<Ref>::const_iterator i = post.referencedBy.begin(); i != post.referencedBy.end(); ++i) {
        cppcms::json::object ref;
        ref["boardName"] = i->boardName;
        ref["postNumber"] = i->postNumber;
        ref["threadNumber"] = i->threadNumber;
        refs.push_back(ref);
    }
    o["referencedBy"] = refs;
    refs.clear();
    for (std::list<Ref>::const_iterator i = post.refersTo.begin(); i != post.refersTo.end(); ++i) {
        cppcms::json::object ref;
        ref["boardName"] = i->boardName;
        ref["postNumber"] = i->postNumber;
        ref["threadNumber"] = i->threadNumber;
        refs.push_back(ref);
    }
    o["refersTo"] = refs;
    return o;
}

void AbstractBoard::beforeRenderBoard(const cppcms::http::request &/*req*/, Content::Board */*c*/)
{
    //
}

void AbstractBoard::beforeRenderThread(const cppcms::http::request &/*req*/, Content::Thread */*c*/)
{
    //
}

Content::Board *AbstractBoard::createBoardController(const cppcms::http::request &/*req*/, QString &/*viewName*/)
{
    return new Content::Board;
}

Content::Thread *AbstractBoard::createThreadController(const cppcms::http::request &/*req*/, QString &/*viewName*/)
{
    return new Content::Thread;
}

void AbstractBoard::cleanupBoards()
{
    QMutexLocker locker(&boardsMutex);
    foreach (AbstractBoard *b, boards)
        delete b;
    boards.clear();
}

void AbstractBoard::initBoards(bool reinit)
{
    QMutexLocker locker(&boardsMutex);
    if (boardsInitialized && !reinit)
        return;
    if (reinit) {
        QSet<QString> boardNames = QSet<QString>::fromList(boards.keys());
        BSettingsNode *n = BTerminal::rootSettingsNode()->find("Board");
        foreach (BSettingsNode *nn, n->childNodes()) {
            if (!boardNames.contains(nn->key()))
                continue;
            n->removeChild(nn);
            delete nn;
        }
        foreach (AbstractBoard *b, boards.values())
            delete b;
        boards.clear();
    }
    ConfigurableBoard *cb = new ConfigurableBoard("a", BTranslation::translate("AbstractBoard", "/a/nime", "title"),
        BTranslation::translate("AbstractBoard", "Kamina", "defaultUserName"));
    boards.insert(cb->name(), cb);
    cb = new ConfigurableBoard("b", BTranslation::translate("AbstractBoard", "/b/rotherhood", "title"));
    boards.insert(cb->name(), cb);
    AbstractBoard *b = new cgBoard;
    boards.insert(b->name(), b);
    b = new echoBoard;
    boards.insert(b->name(), b);
    cb = new ConfigurableBoard("h", BTranslation::translate("AbstractBoard", "/h/entai", "title"));
    boards.insert(cb->name(), cb);
    cb = new ConfigurableBoard("int", BTranslation::translate("AbstractBoard", "/int/ernational", "title"),
                               BTranslation::translate("AbstractBoard", "Vladimir Putin", "defaultUserName"));
    cb->setShowWhois(true);
    boards.insert(cb->name(), cb);
    b = new mlpBoard;
    boards.insert(b->name(), b);
    b = new prBoard;
    boards.insert(b->name(), b);
    cb = new ConfigurableBoard("rf", BTranslation::translate("AbstractBoard", "Refuge", "title"),
                               BTranslation::translate("AbstractBoard", "Whiner", "defaultUserName"));
    boards.insert(cb->name(), cb);
    cb = new ConfigurableBoard("soc", BTranslation::translate("AbstractBoard", "Social life", "title"),
                               BTranslation::translate("AbstractBoard", "Life of the party", "defaultUserName"));
    boards.insert(cb->name(), cb);
    cb = new ConfigurableBoard("3dpd", BTranslation::translate("AbstractBoard", "3D pron", "title"));
    boards.insert(cb->name(), cb);
    cb = new ConfigurableBoard("vg", BTranslation::translate("AbstractBoard", "Video games", "title"),
                               BTranslation::translate("AbstractBoard", "PC Nobleman", "defaultUserName"));
    boards.insert(cb->name(), cb);
    b = new rpgBoard;
    boards.insert(b->name(), b);
    foreach (BPluginWrapper *pw, BCoreApplication::pluginWrappers("board-factory")) {
        pw->unload();
        BCoreApplication::removePlugin(pw);
    }
    BCoreApplication::loadPlugins(QStringList() << "board-factory");
    foreach (BPluginWrapper *pw, BCoreApplication::pluginWrappers("board-factory")) {
        BoardFactoryPluginInterface *i = qobject_cast<BoardFactoryPluginInterface *>(pw->instance());
        if (!i)
            continue;
        foreach (AbstractBoard *b, i->createBoards()) {
            if (!b)
                continue;
            QString nm = b->name();
            if (nm.isEmpty()) {
                delete b;
                continue;
            }
            if (boards.contains(nm))
                delete boards.take(nm);
            boards.insert(nm, b);
        }
    }
    BSettingsNode *n = BTerminal::rootSettingsNode()->find("Board");
    foreach (const QString &boardName, boards.keys()) {
        BSettingsNode *nn = new BSettingsNode(boardName, n);
        BSettingsNode *nnn = new BSettingsNode(QVariant::Bool, "captcha_enabled", nn);
        nnn->setDescription(BTranslation::translate("AbstractBoard",
                                                    "Determines if captcha is enabled on this board.\n"
                                                    "The default is true."));
        nnn = new BSettingsNode(QVariant::UInt, "threads_per_page", nn);
        nnn->setDescription(BTranslation::translate("AbstractBoard", "Number of threads per one page on this board.\n"
                                                    "The default is 20."));
        nnn = new BSettingsNode(QVariant::Bool, "posting_enabled", nn);
        nnn->setDescription(BTranslation::translate("AbstractBoard",
                                                    "Determines if posting is enabled on this board.\n"
                                                    "The default is true."));
        nnn = new BSettingsNode(QVariant::Bool, "drafts_enabled", nn);
        nnn->setDescription(BTranslation::translate("AbstractBoard",
                                                    "Determines if drafts are enabled on this board.\n"
                                                    "The default is true."));
        nnn = new BSettingsNode(QVariant::UInt, "bump_limit", nn);
        nnn->setDescription(BTranslation::translate("AbstractBoard", "Maximum bump count on this board.\n"
                                                    "When a thread has reached it's bump limit, "
                                                    "it will not be raised anymore.\n"
                                                    "The default is 500."));
        nnn = new BSettingsNode(QVariant::UInt, "post_limit", nn);
        nnn->setDescription(BTranslation::translate("AbstractBoard", "Maximum post count per thread on this board.\n"
                                                    "The default is 1000."));
        nnn = new BSettingsNode(QVariant::UInt, "thread_limit", nn);
        nnn->setDescription(BTranslation::translate("AbstractBoard", "Maximum thread count for this board.\n"
                                                    "When the limit is reached, the most old threads get deleted.\n"
                                                    "The default is 200."));
        nnn = new BSettingsNode(QVariant::UInt, "max_last_posts", nn);
        nnn->setDescription(BTranslation::translate("AbstractBoard", "Maximum last posts displayed for each thread at "
                                                    "this board.\nThe default is 3."));
        nnn = new BSettingsNode(QVariant::Bool, "hidden", nn);
        nnn->setDescription(BTranslation::translate("AbstractBoard", "Determines if this board is hidden.\n"
                                                    "A hidden board will not appear in navigation bars.\n"
                                                    "The default is false."));
        nnn = new BSettingsNode(QVariant::Bool, "enabled", nn);
        nnn->setDescription(BTranslation::translate("AbstractBoard", "Determines if this board is enabled.\n"
                                                    "A disabled board will not be accessible by any means.\n"
                                                    "The default is true."));
        nnn = new BSettingsNode(QVariant::UInt, "max_email_length", nn);
        nnn->setDescription(BTranslation::translate("AbstractBoard",
                                                    "Maximum length of the e-mail field for this board.\n"
                                                    "The default is 150."));
        nnn = new BSettingsNode(QVariant::UInt, "max_name_length", nn);
        nnn->setDescription(BTranslation::translate("AbstractBoard",
                                                    "Maximum length of the name field for this board.\n"
                                                    "The default is 50."));
        nnn = new BSettingsNode(QVariant::UInt, "max_subject_length", nn);
        nnn->setDescription(BTranslation::translate("AbstractBoard",
                                                    "Maximum length of the subject field for this board.\n"
                                                    "The default is 150."));
        nnn = new BSettingsNode(QVariant::UInt, "max_text_length", nn);
        nnn->setDescription(BTranslation::translate("AbstractBoard",
                                                    "Maximum length of the text field for this board.\n"
                                                    "The default is 15000."));
        nnn = new BSettingsNode(QVariant::UInt, "max_password_length", nn);
        nnn->setDescription(BTranslation::translate("AbstractBoard",
                                                    "Maximum length of the password field for this board.\n"
                                                    "The default is 150."));
        nnn = new BSettingsNode(QVariant::UInt, "max_file_size", nn);
        nnn->setDescription(BTranslation::translate("AbstractBoard",
                                                    "Maximum attached file size (in bytes) for this board.\n"
                                                    "The default is 10485760 (10 MB)."));
        nnn = new BSettingsNode(QVariant::UInt, "max_file_count", nn);
        nnn->setDescription(BTranslation::translate("AbstractBoard",
                                                    "Maximum attached file count for this board.\n"
                                                    "The default is 1."));
        nnn = new BSettingsNode(QVariant::UInt, "archive_limit", nn);
        nnn->setDescription(BTranslation::translate("AbstractBoard", "Maximum archived thread count for this board.\n"
                                                    "The default is 0 (do not archive)."));
        nnn = new BSettingsNode(QVariant::UInt, "captcha_quota", nn);
        nnn->setDescription(BTranslation::translate("AbstractBoard", "Maximum count of extra posts a user may make "
                                                    "before solving captcha again on this board.\n"
                                                    "The default is 0 (solve captcha every time)."));
        nnn = new BSettingsNode(QVariant::String, "supported_file_types", nn);
        BTranslation t = BTranslation::translate("AbstractBoard", "MIME types of files allowed for attaching on "
                                                 "this board.\n"
                                                 "Must be separated by commas. Wildcard matching is used.\n"
                                                 "The default is %1.");
        t.setArgument(defaultFileTypes);
        nnn->setDescription(t);
    }
    if (!reinit)
        qAddPostRoutine(&cleanupBoards);
    boardsInitialized = true;
}
