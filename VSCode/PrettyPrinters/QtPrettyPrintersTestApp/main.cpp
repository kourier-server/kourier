#include <QDebug>
#include <QMap>
#include <QString>
#include <QByteArray>
#include <QByteArrayView>
#include <QString>
#include <QStringView>
#include <QLatin1StringView>
#include <QUtf8StringView>
#include <QTime>
#include <QDate>
#include <QVector>
#include <QList>
#include <QPair>
#include <QQueue>
#include <QStack>
#include <QByteArrayList>
#include <QStringList>
#include <QHash>
#include <QSet>
#include <QMultiHash>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QVariantHash>
#include <QUrl>

using namespace Qt::StringLiterals;

int main(int argc, char ** argv)
{
    QByteArray ba("QByteArray contents.");
    QString str = u"QString contents."_s;
    QByteArrayList baList({"First", "Second", "Third"});
    QStringList strList({u"First"_s, u"Second"_s, u"Third"_s});   
    QVector<QByteArray> baVector({"First", "Second", "Third"});
    QList<int> list({1,2,3});
    const auto *pList = &list;
    const auto &refList = list;
    QPair<QByteArray, int> pair({"text", 2});
    QMap<QString, int> map({{u"Hello World!"_s, 1}, {u"Oi Mundo!"_s, 2}});
    QQueue<int> queue({1,2,3});
    QStack<int> stack({1,2,3});
    QHash<QString, int> hash({{u"Hello World!"_s, 1}, {u"Oi Mundo!"_s, 2}});
    QSet<int> set({1,2,3});
    QMultiHash<QByteArray, int> multiHash({{"First", 1}, {"First", 2}, {"First", 3}, {"Second", 108}, {"Third", -1}});
    QVariant intVariant(int(3));
    QVariantList variantList({
        QVariant(int(-15)),
        QVariant(QByteArray("My ByteArray Contents")),
        QVariant(u"My QString contents"_s)});
    QVariantMap variantMap({
        {u"First"_s, QVariant(int(-15))},
        {u"Second"_s, QVariant(QByteArray("My ByteArray Contents"))},
        {u"Third"_s, QVariant(u"My QString contents"_s)}});
    QVariantHash variantHash({
        {u"First"_s, QVariant(int(-15))},
        {u"Second"_s, QVariant(QByteArray("My ByteArray Contents"))},
        {u"Third"_s, QVariant(u"My QString contents"_s)}});
    QUrl url("https://user:password@example.com:443/a/path?a_query=3#a_fragment");
    QByteArrayView baView = "Contents of QByteArrayView";
    QStringView strView = u"Contents of QStringView"_s;
    QStringView emptyView;
    QLatin1StringView latin1StrView = "Contents of QLatin1StringView"_L1;
    QUtf8StringView utf8StrView = u8"Contents of QUtf8StringView";
    QTime time(9, 35, 22, 18);
    QDate date(2026, 11, 6);
    qInfo() << "Hello World!" << Qt::endl;
    return 0;
}
