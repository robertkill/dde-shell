// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "notifyaccessor.h"

#include <QQmlEngine>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QDBusInterface>
#include <QDBusPendingReply>
#include <QProcess>
#include <QDBusReply>
#include <QQueue>

#include <DConfig>

#include <pluginloader.h>
#include <applet.h>
#include <containment.h>

#include "dataaccessor.h"

DS_USE_NAMESPACE
DCORE_USE_NAMESPACE

namespace notifycenter {
namespace {
Q_LOGGING_CATEGORY(notifyLog, "notify")
}

static const QString DDENotifyDBusServer = "org.deepin.dde.Notification1";
static const QString DDENotifyDBusInterface = "org.deepin.dde.Notification1";
static const QString DDENotifyDBusPath = "/org/deepin/dde/Notification1";
static const uint ShowNotificationTop = 7;
static const QString InvalidApp {"DS-Invalid-Apps"};
static const QStringList InvalidPinnedApps {InvalidApp};

static QDBusInterface notifyCenterInterface()
{
    return QDBusInterface(DDENotifyDBusServer, DDENotifyDBusPath, DDENotifyDBusInterface);
}

static QDBusInterface controlCenterInterface()
{
    return QDBusInterface("org.deepin.dde.ControlCenter1",
                          "/org/deepin/dde/ControlCenter1",
                          "org.deepin.dde.ControlCenter1");
}

class EventFilter : public QObject
{
    // QObject interface
public:
    virtual bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut
            || event->type() == QEvent::KeyPress  || event->type() == QEvent::KeyRelease) {
            qDebug(notifyLog) << "Focus and Key event" << event->type() << watched << qApp->focusObject();
        }
        return false;
    }
};

static QList<DApplet *> appletList(const QString &pluginId)
{
    QList<DApplet *> ret;
    auto rootApplet = DPluginLoader::instance()->rootApplet();
    auto root = qobject_cast<DContainment *>(rootApplet);

    QQueue<DContainment *> containments;
    containments.enqueue(root);
    while (!containments.isEmpty()) {
        DContainment *containment = containments.dequeue();
        for (const auto applet : containment->applets()) {
            if (auto item = qobject_cast<DContainment *>(applet)) {
                containments.enqueue(item);
            }
            if (applet->pluginId() == pluginId)
                ret << applet;
        }
    }
    return ret;
}

NotifyAccessor::NotifyAccessor(QObject *parent)
    : m_pinnedApps(InvalidPinnedApps)
{
    auto applets = appletList("org.deepin.ds.notificationserver");
    bool valid = false;
    if (!applets.isEmpty()) {
        if (auto server = applets.first()) {
            valid = QObject::connect(server,
                                     SIGNAL(notificationStateChanged(qint64, int)),
                                     this,
                                     SLOT(onReceivedRecordStateChanged(qint64, int)),
                                     Qt::QueuedConnection);
        }
    } else {
        // old interface by dbus
        auto connection = QDBusConnection::sessionBus();
        valid = connection.connect(DDENotifyDBusServer, DDENotifyDBusPath, DDENotifyDBusInterface,
                                        "RecordAdded", this, SLOT(onReceivedRecord(const QString &)));
    }
    if (!valid) {
        qWarning() << "NotifyConnection is invalid, and can't receive RecordAdded signal.";
    }

    if (!qEnvironmentVariableIsEmpty("DS_NOTIFICATION_DEBUG")) {
        const int value = qEnvironmentVariableIntValue("DS_NOTIFICATION_DEBUG");
        m_debugging = value;
    }
    if (m_debugging) {
        qApp->installEventFilter(new EventFilter());
    }
}

NotifyAccessor *NotifyAccessor::instance()
{
    static NotifyAccessor *instance = nullptr;

    if (!instance) {
        instance = new NotifyAccessor(qGuiApp);
        instance->setDataAccessor(new DataAccessor());
    }
    return instance;
}

NotifyAccessor *NotifyAccessor::create(QQmlEngine *, QJSEngine *)
{
    auto helper = NotifyAccessor::instance();
    QQmlEngine::setObjectOwnership(helper, QQmlEngine::CppOwnership);
    return helper;
}

void NotifyAccessor::setDataAccessor(DataAccessor *accessor)
{
    if (m_accessor) {
        delete m_accessor;
    }
    m_accessor = accessor;
}

NotifyEntity NotifyAccessor::fetchEntity(qint64 id) const
{
    qDebug(notifyLog) << "Fetch entity" << id;
    auto ret = m_accessor->fetchEntity(id);
    return ret;
}

int NotifyAccessor::fetchEntityCount(const QString &appName) const
{
    qDebug(notifyLog) << "Fetch entity count for the app" << appName;
    auto ret = m_accessor->fetchEntityCount(appName, NotifyEntity::Processed);
    return ret;
}

NotifyEntity NotifyAccessor::fetchLastEntity(const QString &appName) const
{
    qDebug(notifyLog) << "Fetch last entity for the app" << appName;
    auto ret = m_accessor->fetchLastEntity(appName, NotifyEntity::Processed);
    return ret;
}

QList<NotifyEntity> NotifyAccessor::fetchEntities(const QString &appName, int maxCount)
{
    qDebug(notifyLog) << "Fetch entities for the app" << appName;
    auto ret = m_accessor->fetchEntities(appName, NotifyEntity::Processed, maxCount);
    return ret;
}

QStringList NotifyAccessor::fetchApps(int maxCount) const
{
    qDebug(notifyLog) << "Fetch apps count" << maxCount;
    auto ret = m_accessor->fetchApps(maxCount);
    return ret;
}

void NotifyAccessor::removeEntity(qint64 id)
{
    qDebug(notifyLog) << "Remove notify" << id;

    m_accessor->removeEntity(id);
}

void NotifyAccessor::removeEntityByApp(const QString &appName)
{
    qDebug(notifyLog) << "Remove notifies for the application" << appName;

    m_accessor->removeEntityByApp(appName);
}

void NotifyAccessor::clear()
{
    qDebug(notifyLog) << "Remove all notify";

    m_accessor->clear();
}

// don't need to emit ActionInvoked of protocol.
void NotifyAccessor::invokeAction(const NotifyEntity &entity, const QString &actionId)
{
    qDebug(notifyLog) << "Invoke action for the notify" << entity.id() << actionId;

    QMap<QString, QVariant> hints = entity.hints();
    if (hints.isEmpty())
        return;
    QMap<QString, QVariant>::const_iterator i = hints.constBegin();
    while (i != hints.constEnd()) {
        QStringList args = i.value().toString().split(",");
        if (!args.isEmpty()) {
            QString cmd = args.first();
            args.removeFirst();
            if (i.key() == "x-deepin-action-" + actionId) {
                qDebug(notifyLog) << "Invoke action" << cmd;
                QProcess::startDetached(cmd, args);
            }
        }
        ++i;
    }
}

void NotifyAccessor::pinApplication(const QString &appId, bool pin)
{
    qDebug(notifyLog) << "Pin the application" << appId << pin;

    if (!pin) {
        m_pinnedApps.removeOne(appId);
    } else {
        if (!m_pinnedApps.contains(appId))
            m_pinnedApps.append(appId);
    }
    QScopedPointer<DConfig> config(DConfig::create("org.deepin.dde.shell", "org.deepin.dde.shell.notification"));
    config->setValue("pinnedApps", m_pinnedApps);
}

bool NotifyAccessor::applicationPin(const QString &appId) const
{
    if (m_pinnedApps.contains(appId))
        return true;

    if (m_pinnedApps.contains(InvalidApp)) {
        QScopedPointer<DConfig> config(DConfig::create("org.deepin.dde.shell", "org.deepin.dde.shell.notification"));
        const_cast<NotifyAccessor*>(this)->m_pinnedApps = config->value("pinnedApps").toStringList();
    }

    return m_pinnedApps.contains(appId);
}

void NotifyAccessor::openNotificationSetting()
{
    qDebug(notifyLog) << "Open notification setting";
    QDBusReply<void> reply = controlCenterInterface().call("ShowPage",
                                                           "notification");
    if (reply.error().isValid()) {
        qWarning(notifyLog) << "Failed to Open notifycation setting" << reply.error().message();
        return;
    }
}

void NotifyAccessor::addNotify(const QString &appName, const QString &content)
{
    qDebug(notifyLog) << "Add notify" << appName;
    static int id = 10000;
    NotifyEntity entity(id++, appName);
    entity.setBody(content);
    m_accessor->addEntity(entity);

    if (auto entity = fetchLastEntity(appName); entity.isValid()) {
        entityReceived(entity.id());
    }
}

void NotifyAccessor::fetchDataInfo()
{
    QStringList info;
    auto entityCount = fetchEntityCount();
    auto apps = fetchApps();
    info.append(QString("notifyCount: %1, appCount: %2").arg(entityCount).arg(apps.size()));
    for (auto item : apps) {
        info.append(QString("%1 -> %2").arg(item).arg(fetchEntityCount(item)));
    }
    QString ret = info.join("\n");
    m_dataInfo = ret;
    dataInfoChanged();
    appsChanged();
}

void NotifyAccessor::onReceivedRecordStateChanged(qint64 id, int processedType)
{
    if (processedType == NotifyEntity::Processed) {
        onReceivedRecord(id);
    }
}

void NotifyAccessor::onReceivedRecord(qint64 id)
{
    emit entityReceived(id);
}

QString NotifyAccessor::dataInfo() const
{
    return m_dataInfo;
}

QStringList NotifyAccessor::apps() const
{
    return m_apps;
}

bool NotifyAccessor::debugging() const
{
    return m_debugging;
}

}