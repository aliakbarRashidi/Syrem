#ifndef ANDROIDSCHEDULER_H
#define ANDROIDSCHEDULER_H

#include <QObject>
#include <QAndroidJniObject>
#include <reminder.h>

class AndroidScheduler : public QObject
{
	Q_OBJECT

public:
	explicit AndroidScheduler(QObject *parent = nullptr);

public slots:
	bool scheduleReminder(const Reminder &reminder);
	void cancleReminder(const QUuid &id);

private:
	QAndroidJniObject _jScheduler;
};

#endif // ANDROIDSCHEDULER_H