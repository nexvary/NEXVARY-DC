#pragma once
#include <QJsonObject>
#include <QString>
#include <QByteArray>
#include <atomic>
#include <functional>
namespace dc {
using Progress = std::function<void(qint64, qint64)>;
struct Context {
 std::atomic_bool *cancel = nullptr;
 Progress progress;
 bool cancelled() const { return cancel && cancel->load(); }
 void update(qint64 done, qint64 total) const { if(progress) progress(done,total); }
};
// File-only operations: block devices and Windows device namespaces are excluded.
bool isRegularSource(const QString &path);
QJsonObject scanImage(const QString &path, const Context &context = {});
QJsonObject copyImage(const QString &source, const QString &destination, const Context &context = {});
QByteArray testPattern(quint64 index, quint64 nonce, qsizetype size);
struct ProbeIO {
 std::function<bool(quint64, const QByteArray &)> write;
 std::function<QByteArray(quint64)> read;
};
QJsonObject verifyStorage(ProbeIO &io, quint64 blocks, qsizetype blockSize, quint64 nonce, const Context &context = {});
QJsonObject testDirectory(const QString &directory, int mebibytes, bool acknowledged, const Context &context = {});
}
