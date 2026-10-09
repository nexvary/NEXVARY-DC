#pragma once
#include <QObject>
#include <QVariantList>
#include <QFutureWatcher>
#include <QJsonObject>
#include <QSqlDatabase>
#include <atomic>
#include <functional>
#include <QTimer>
class Controller : public QObject {
 Q_OBJECT
 Q_PROPERTY(QVariantList volumes READ volumes NOTIFY volumesChanged)
 Q_PROPERTY(QVariantList disks READ disks NOTIFY disksChanged)
 Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
 Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
 Q_PROPERTY(double progress READ progress NOTIFY stateChanged)
 Q_PROPERTY(QString report READ report NOTIFY reportChanged)
 Q_PROPERTY(QVariantMap result READ result NOTIFY reportChanged)
 Q_PROPERTY(bool administrator READ administrator CONSTANT)
 Q_PROPERTY(bool windows READ windows CONSTANT)
 Q_PROPERTY(bool interruptible READ interruptible NOTIFY stateChanged)
 Q_PROPERTY(QString storageNotice READ storageNotice CONSTANT)
 Q_PROPERTY(QVariantMap health READ health NOTIFY healthChanged)
 Q_PROPERTY(QVariantList healthSamples READ healthSamples NOTIFY healthChanged)
 Q_PROPERTY(QVariantList crystalDisks READ crystalDisks NOTIFY healthChanged)
 Q_PROPERTY(QVariantMap smartPreferences READ smartPreferences NOTIFY healthChanged)
 Q_PROPERTY(bool monitoring READ monitoring NOTIFY healthChanged)
public:
 explicit Controller(QObject *parent=nullptr);
 ~Controller() override;
 QVariantList volumes() const {return m_volumes;}
 QVariantList disks() const {return m_disks;}
 QVariantList history() const {return m_history;}
 bool busy() const {return m_busy;}
 double progress() const {return m_progress;}
 QString report() const {return m_report;}
 QVariantMap result() const {return m_result;}
 QString storageNotice() const {return m_storageNotice;}
 bool administrator() const;
 bool windows() const;
 bool interruptible() const {return !m_busy || !m_mutating;}
 QVariantMap health() const {return m_health;}
 QVariantList healthSamples() const {return m_healthSamples;}
 QVariantList crystalDisks() const {return m_crystalDisks;}
 QVariantMap smartPreferences() const {return m_smartPreferences;}
 bool monitoring() const {return m_monitor.isActive();}
 Q_INVOKABLE bool relaunchAdministrator();
 Q_INVOKABLE QVariantMap prepareStorage(const QString &device,const QString &action,int partition,const QString &filesystem,const QString &style,int sizeMiB,const QString &source);
 Q_INVOKABLE void executeStorage(const QString &token,const QString &confirmation,bool acknowledged);
 Q_INVOKABLE void refresh();
 Q_INVOKABLE void inspectHealth(const QString &device);
 Q_INVOKABLE void readCrystalHealth();
 Q_INVOKABLE void selectCrystalDisk(int index);
 Q_INVOKABLE void openCrystalPanel(bool arabic,bool acknowledged);
 Q_INVOKABLE void setSmartPreferences(const QVariantMap &preferences);
 Q_INVOKABLE void setMonitoring(bool enabled);
 Q_INVOKABLE void scanSurface(const QString &device,bool acknowledged);
 Q_INVOKABLE void rescueDisk(const QString &device,const QString &destination,bool acknowledged,bool resume=false,int sectorBytes=512,int retries=1);
 Q_INVOKABLE void retryRescueDisk(const QString &device,const QString &previous,const QString &destination,bool acknowledged,bool resume=false,int sectorBytes=512,int retries=1);
 Q_INVOKABLE void recoverImage(const QString &source,const QString &directory,int mode=0,int maxFiles=100000);
 Q_INVOKABLE void prepareBootRepair(const QString &root,const QString &esp,const QString &disk,const QString &mode,const QString &backup);
 Q_INVOKABLE void executeBootRepair(const QString &confirmation);
 Q_INVOKABLE void prepareFirmwareBoot(const QString &id=QString());
 Q_INVOKABLE void scanImage(const QString &path);
 Q_INVOKABLE void copyImage(const QString &source,const QString &destination);
 Q_INVOKABLE void testCapacity(const QString &directory,int mib,bool acknowledged);
 Q_INVOKABLE void cancel();
 Q_INVOKABLE bool exportReport(const QString &destination);
 Q_INVOKABLE QString fileUrl(const QString &path) const;
 Q_INVOKABLE QString localPath(const QString &url) const;
signals:
 void volumesChanged();void disksChanged();void historyChanged();void stateChanged();void reportChanged();
 void healthChanged();void healthAlert(const QString &message);
private:
 using Work=std::function<QJsonObject()>;
 void start(const QString &name,Work work);
 void setResult(const QJsonObject &result);
 void loadHistory();
 void recordHealth(const QJsonObject &result);
 QVariantList m_volumes,m_disks,m_history;
 QVariantMap m_result;
 QFutureWatcher<QJsonObject> m_watcher;
 QSqlDatabase m_db;
 std::atomic_bool m_cancel{false};
 QJsonObject m_pending;
 QString m_pendingToken;
 qint64 m_pendingExpires=0;
 bool m_mutating=false;
 bool m_busy=false;
 double m_progress=0;
 QString m_report,m_operation,m_storageNotice;
 QVariantMap m_health,m_smartPreferences;
 QVariantList m_healthSamples,m_crystalDisks;
 QTimer m_monitor;
 QString m_healthDevice,m_monitorSerial,m_lastHealthAlert,m_settingsPath;
};
