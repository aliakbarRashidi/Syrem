#ifndef KDENOTIFIER_H
#define KDENOTIFIER_H

#include <QObject>
#include <QtMvvmCore/Injection>
#include <KNotification>
#include "inotifier.h"
#include <eventexpressionparser.h>
#include <syncedsettings.h>

class KdeNotifier : public QObject, public INotifier
{
	Q_OBJECT
	Q_INTERFACES(INotifier)

	QTMVVM_INJECT_PROP(SyncedSettings*, settings, _settings)
	QTMVVM_INJECT_PROP(EventExpressionParser*, parser, _parser)

public:
	Q_INVOKABLE explicit KdeNotifier(QObject *parent = nullptr);

public slots:
	void showNotification(const Reminder &reminder) override;
	void removeNotification(QUuid id) override;
	void showErrorMessage(const QString &error) override;
	void cancelAll() override;

signals:
	void messageCompleted(QUuid id, quint32 versionCode) final;
	void messageDelayed(QUuid id, quint32 versionCode, const QDateTime &nextTrigger) final;
	void messageActivated(QUuid id) final;
	void messageOpenUrls(QUuid id) final;

private slots:
	void qtmvvm_init();

private:
	SyncedSettings *_settings;
	EventExpressionParser *_parser;
	QHash<QUuid, KNotification*> _notifications;

	bool removeNot(QUuid id, bool close = false);
};

#endif // KDENOTIFIER_H
