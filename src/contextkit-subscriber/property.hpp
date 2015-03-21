#ifndef _STATEFS_CKIT_PROPERTY_HPP_
#define _STATEFS_CKIT_PROPERTY_HPP_

#include "actor.hpp"

//#include <cor/mt.hpp>
#include <qtaround/mt.hpp>
#include <qtaround/debug.hpp>
#include <functional>
#include <future>

#include <QObject>
#include <QString>
#include <QVariant>
#include <QFile>
#include <QScopedPointer>
#include <QSharedPointer>
#include <QByteArray>
#include <QMutex>
#include <QSet>
#include <QMap>
#include <QSocketNotifier>
#include <QPointer>

class ContextPropertyInfo;
class QSocketNotifier;
class QTimer;


class ContextPropertyPrivate;

namespace ckit {

class File
{
public:
    enum class Status {
        Opened = 0,
        NoFile,
        NoAccess
    };

    enum Type { User = 0, System };

    typedef std::unique_ptr<QFile> file_ptr;
    typedef std::array<file_ptr, System + 1> files_type;

    File(QString const &);
    virtual ~File() {}

    QString fileName() const
    {
        return file_ ? file_->fileName() : "?";
    }

    bool seek(qint64 pos)
    {
        return file_ ? file_->seek(pos) : false;
    }

    qint64 size() const
    {
        return file_ ? file_->size() : 0;
    }

    int handle() const
    {
        return file_ ? file_->handle() : -1;
    }

    virtual void close();
    qint64 read(QByteArray &, size_t, size_t offset = 0);
    QString key() const { return key_; }

protected:
    bool tryOpen(QIODevice::OpenMode);

    file_ptr file_;

private:
    files_type openNew(QIODevice::OpenMode);
    QString nameFor(Type) const;
    QString getError(file_ptr const&) const;

    Type type_;
    QString key_;
    size_t failures_count_;
};

class FileReader : public File
{
public:
    FileReader(QString const &key) : File(key) {}

    bool tryOpen()
    {
        return File::tryOpen(QIODevice::ReadOnly | QIODevice::Unbuffered);
    }

    template <typename T>
    void connect(T *parent, void (T::*slot)(int))
    {
        auto h = handle();
        if (h >= 0) {
            notifier_.reset(new QSocketNotifier
                            (handle(), QSocketNotifier::Read));
            parent->connect(notifier_.data(), &QSocketNotifier::activated
                            , parent, slot);
        } else {
            qtaround::debug::warning("Can't connect, invalid handle", key());
        }
    }

    virtual void close();
    mutable QScopedPointer<QSocketNotifier> notifier_;
};

class FileWriter : public File
{
public:
    FileWriter(QString const &key) : File(key) {}

    bool tryOpen()
    {
        return File::tryOpen(QIODevice::WriteOnly | QIODevice::Unbuffered);
    }
    bool write(QByteArray const &);
};

class Adapter;

class Property : public QObject
{
    Q_OBJECT;
public:
    Property(QString const &key, QObject *parent);
    virtual ~Property();

    QVariant subscribe();
    void unsubscribe();

    bool update();

signals:
    void changed(QVariant) const;

private slots:
    void handleActivated(int);
    void trySubscribe();

private:
    bool tryOpen();
    void resubscribe();
    QVariant subscribe_();

    FileReader file_;
    QByteArray buffer_;
    mutable int reopen_interval_;
    mutable QTimer *reopen_timer_;
    bool is_subscribed_;
    QVariant cache_;
};

class SubscribeRequest;
class UnsubscribeRequest;
class WriteRequest;
class RefreshRequest;

class PropertyMonitor : public QObject
{
    Q_OBJECT;
public:
    virtual bool event(QEvent *);

    typedef qtaround::mt::ActorHandle monitor_ptr;
    static monitor_ptr instance();
private:
    void subscribe(SubscribeRequest*);
    void unsubscribe(UnsubscribeRequest*);
    Property *add(const QString &);
    void write(WriteRequest *);
    void refresh(RefreshRequest*);

    QMap<QString, QSet<QSharedPointer<Adapter> > > targets_;
    QMap<QString, Property*> properties_;

    static std::once_flag once_;
    static monitor_ptr instance_;
};

class Adapter : public QObject
{
    Q_OBJECT;
public:
    Adapter(ContextPropertyPrivate *target)
        : target_(target)
    {}


public slots:
    void onChanged(QVariant);
private:
    friend class ::ContextPropertyPrivate;
    void detach();
    ContextPropertyPrivate *target_;
};

}

class ContextPropertyPrivate : public QObject
{
    Q_OBJECT;

public:
    explicit ContextPropertyPrivate(const QString &key, QObject *parent = 0);

    virtual ~ContextPropertyPrivate();

    QString key() const;
    QVariant value(const QVariant &def) const;
    QVariant value() const;

    const ContextPropertyInfo* info() const;

    void subscribe () const;
    void unsubscribe () const;

    void waitForSubscription() const;
    void waitForSubscription(bool block) const;

    static void ignoreCommander();
    static void setTypeCheck(bool typeCheck);

    void refresh() const;

signals:
    void valueChanged() const;

private:

    friend class ckit::Adapter;
    void onChanged(QVariant) const;

    enum State {
        Initial,
        Unsubscribing,
        Subscribing,
        Subscribed
    };

    bool update(QVariant const&) const;
    bool waitForUnsubscription() const;
    static ckit::PropertyMonitor::monitor_ptr actor();
    QString key_;
    mutable State state_;
    mutable bool is_cached_;
    mutable QVariant cache_;

    mutable std::future<QVariant> on_subscribed_;
    mutable std::future<void> on_unsubscribed_;
    mutable QSharedPointer<ckit::Adapter> adapter_;
};

#endif // _STATEFS_CKIT_PROPERTY_HPP_
