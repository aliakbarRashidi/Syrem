TEMPLATE = app

QT += core gui widgets remoteobjects datasync
CONFIG += c++11

TARGET = remind-me
VERSION = $$RM_VERSION

RC_ICONS += ../icn/remindme.ico
QMAKE_TARGET_COMPANY = "Skycoder42"
QMAKE_TARGET_PRODUCT = "Remind-Me"
QMAKE_TARGET_DESCRIPTION = "Simple reminder app"
QMAKE_TARGET_COPYRIGHT = "Felix Barz"
QMAKE_TARGET_BUNDLE_PREFIX = de.skycoder42

ICON = ../icn/remindme.icns
QMAKE_TARGET_BUNDLE_PREFIX = de.skycoder42

DEFINES += "TARGET=\\\"$$TARGET\\\""
DEFINES += "VERSION=\\\"$$VERSION\\\""
DEFINES += "COMPANY=\"\\\"$$QMAKE_TARGET_COMPANY\\\"\""
DEFINES += "BUNDLE=\"\\\"$$QMAKE_TARGET_BUNDLE_PREFIX\\\"\""
DEFINES += "DISPLAY_NAME=\"\\\"$$QMAKE_TARGET_PRODUCT\\\"\""

HEADERS += mainwindow.h

SOURCES += main.cpp \
	mainwindow.cpp

FORMS += mainwindow.ui

TRANSLATIONS += remindme_widgets_de.ts \
	remindme_widgets_template.ts

# Link with core project
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../RemindMeCore/release/ -lRemindMeCore
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../RemindMeCore/debug/ -lRemindMeCore
else:unix: LIBS += -L$$OUT_PWD/../RemindMeCore/ -lRemindMeCore

INCLUDEPATH += $$PWD/../RemindMeCore
DEPENDPATH += $$PWD/../RemindMeCore

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../RemindMeCore/release/libRemindMeCore.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../RemindMeCore/debug/libRemindMeCore.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../RemindMeCore/release/RemindMeCore.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../RemindMeCore/debug/RemindMeCore.lib
else:unix: PRE_TARGETDEPS += $$OUT_PWD/../RemindMeCore/libRemindMeCore.a

!ReleaseBuild:!DebugBuild:!system(qpmx -d $$shell_quote($$_PRO_FILE_PWD_) --qmake-run init $$QPMX_EXTRA_OPTIONS $$shell_quote($$QMAKE_QMAKE) $$shell_quote($$OUT_PWD)): error(qpmx initialization failed. Check the compilation log for details.)
else: include($$OUT_PWD/qpmx_generated.pri)