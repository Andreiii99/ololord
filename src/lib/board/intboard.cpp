#include "intboard.h"

#include "translator.h"

#include <QLocale>
#include <QString>

intBoard::intBoard()
{
    //
}

QString intBoard::defaultUserName(const QLocale &l) const
{
    return TranslatorQt(l).translate("intBoard", "Vladimir Putin", "defaultUserName");
}

QString intBoard::name() const
{
    return "int";
}

QString intBoard::title(const QLocale &l) const
{
    TranslatorQt tq(l);
    return tq.translate("intBoard", "/int/ernational", "board title");
}

bool intBoard::showWhois() const
{
    return true;
}
