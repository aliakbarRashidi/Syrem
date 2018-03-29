#include <QAndroidService>
#include <QSet>
#include <QUuid>
#include "remindmeservice.h"

int main(int argc, char *argv[])
{
	QAndroidService a(argc, argv);
	//workarond
	qputenv("PLUGIN_KEYSTORES_PATH", QCoreApplication::applicationDirPath().toUtf8());
	qInfo() << "Overwriting keystore path to:" << qgetenv("PLUGIN_KEYSTORES_PATH");

	qRegisterMetaTypeStreamOperators<QSet<QUuid>>();

	RemindmeService service;
	if(!service.startService())
		return EXIT_FAILURE;

	return a.exec();
}
