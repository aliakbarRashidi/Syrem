#ifndef DATEPARSER_H
#define DATEPARSER_H

#include <QObject>
#include <QDateTime>
#include "schedule.h"

namespace ParserTypes {

class Expression : public QObject
{
	Q_OBJECT

public:
	enum Span {
		InvalidSpan,
		MinuteSpan,
		HourSpan,
		DaySpan,
		WeekSpan,
		MonthSpan,
		YearSpan
	};
	Q_ENUM(Span)

	Expression(QObject *parent = nullptr);
	virtual inline ~Expression() = default;

	virtual Schedule *createSchedule(const QDateTime &since, QObject *parent = nullptr) = 0;
};

// ------------- Basic Types -------------

class Datum : public QObject
{
	Q_OBJECT

	Q_PROPERTY(Scope scope MEMBER scope)
	Q_PROPERTY(int value MEMBER value)

public:
	enum Scope {
		InvalidScope,
		WeekDayScope,
		DayScope,
		MonthScope,
		MonthDayScope
	};
	Q_ENUM(Scope)

	Datum(QObject *parent = nullptr);

	QDate nextDate(QDate wDate, bool scopeReset) const;

	Scope scope;
	int value;
};

class Type : public QObject
{
	Q_OBJECT

	Q_PROPERTY(bool isDatum MEMBER isDatum)
	Q_PROPERTY(Datum* datum MEMBER datum)
	Q_PROPERTY(int count MEMBER count)
	Q_PROPERTY(Expression::Span span MEMBER span)

public:
	Type(QObject *parent = nullptr);

	QDate nextDate(QDate wDate) const;

	bool isDatum;
	Datum *datum;
	int count;
	Expression::Span span;
};

class TimePoint : public QObject
{
	Q_OBJECT

	Q_PROPERTY(Mode mode MEMBER mode)
	Q_PROPERTY(QDate date MEMBER date)
	Q_PROPERTY(Datum* datum MEMBER datum)

public:
	enum Mode {
		InvalidMode,
		DateMode,
		DatumMode,
		YearMode
	};
	Q_ENUM(Mode)

	TimePoint(QObject *parent = nullptr);

	bool isLess(const TimePoint *other) const;
	QDate nextDate(QDate wDate) const;

	Mode mode;
	QDate date;
	Datum *datum;
};

// ------------- Expressions -------------

class Conjunction : public Expression
{
	Q_OBJECT

	Q_PROPERTY(QList<Expression*> expressions MEMBER expressions)

public:
	Q_INVOKABLE Conjunction(QObject *parent = nullptr);
	Schedule *createSchedule(const QDateTime &since, QObject *parent = nullptr) override;

	QList<Expression*> expressions;
};

class TimeSpan : public Expression
{
	Q_OBJECT

	Q_PROPERTY(Span span MEMBER span)
	Q_PROPERTY(int count MEMBER count)
	Q_PROPERTY(Datum* datum MEMBER datum)
	Q_PROPERTY(QTime time MEMBER time)

public:
	Q_INVOKABLE TimeSpan(QObject *parent = nullptr);
	Schedule *createSchedule(const QDateTime &since, QObject *parent = nullptr) override;

	Span span;
	int count;
	Datum *datum;
	QTime time;
};

class Loop : public Expression
{
	Q_OBJECT

	Q_PROPERTY(Type* type MEMBER type)
	Q_PROPERTY(Datum* datum MEMBER datum)
	Q_PROPERTY(QTime time MEMBER time)
	Q_PROPERTY(TimePoint* from MEMBER from)
	Q_PROPERTY(TimePoint* until MEMBER until)

public:
	Q_INVOKABLE Loop(QObject *parent = nullptr);
	Schedule *createSchedule(const QDateTime &since, QObject *parent = nullptr) override;

	Type *type;
	Datum *datum;
	QTime time;
	TimePoint *from;
	TimePoint *until;
};

class Point : public Expression
{
	Q_OBJECT

	Q_PROPERTY(TimePoint* date MEMBER date)
	Q_PROPERTY(QTime time MEMBER time)

public:
	Q_INVOKABLE Point(QObject *parent = nullptr);
	Schedule *createSchedule(const QDateTime &since, QObject *parent = nullptr) override;

	TimePoint *date;
	QTime time;
};

}

class DateParser : public QObject
{
	Q_OBJECT

public:
	explicit DateParser(QObject *parent = nullptr);

	ParserTypes::Expression *parse(const QString &data);

private:
	static const QString timeRegex;

	ParserTypes::Expression *parseExpression(const QString &data, QObject *parent);
	ParserTypes::Conjunction *tryParseConjunction(const QString &data, QObject *parent);
	ParserTypes::TimeSpan *tryParseTimeSpan(const QString &data, QObject *parent);
	ParserTypes::Loop *tryParseLoop(const QString &data, QObject *parent);
	ParserTypes::Point *tryParsePoint(const QString &data, QObject *parent);

	ParserTypes::Datum *parseDatum(const QString &data, QObject *parent);
	ParserTypes::Type *parseType(const QString &data, QObject *parent);
	ParserTypes::TimePoint *parseTimePoint(const QString &data, QObject *parent);

	QDate parseMonthDay(const QString &data);
	QDate parseDate(const QString &data);
	QTime parseTime(const QString &data);
	ParserTypes::Expression::Span parseSpan(const QString &data);

	void validateDatumDatum(ParserTypes::Datum *datum, const ParserTypes::Datum *extraDatum);
	void validateSpanDatum(ParserTypes::Expression::Span span, const ParserTypes::Datum *datum, const QTime &time);

	static QStringList readWeekDays();
	static QStringList readMonths();
};

#endif // DATEPARSER_H