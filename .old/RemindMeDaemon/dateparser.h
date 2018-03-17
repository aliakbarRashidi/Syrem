#ifndef DATEPARSER_H
#define DATEPARSER_H

#include <QObject>
#include <QDateTime>
#include <QSharedPointer>
#include "schedule.h"

namespace ParserTypes {

class TimePoint;

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

	virtual Schedule *createSchedule(const QDateTime &since, QTime defaultTime, QObject *parent = nullptr) = 0;

protected:
	static QDateTime calcTpoint(const QDateTime &since, const TimePoint *tPoint, const QTime &time, bool notToday = true);
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

	Q_INVOKABLE Datum(QObject *parent = nullptr);

	QDate nextDate(QDate wDate, bool scopeReset = false, bool notToday = false) const;

	Scope scope;
	int value;

	static int toMonthDay(int day, int month);
	static QPair<int, int> fromMonthDay(int monthDay);
};

typedef QList<QPair<int, Expression::Span>> Sequence;
QDateTime nextSequenceDate(const Sequence &sequence, const QDateTime &since, bool *timeChange = nullptr);

class Type : public QObject
{
	Q_OBJECT

	Q_PROPERTY(bool isDatum MEMBER isDatum)
	Q_PROPERTY(ParserTypes::Datum* datum MEMBER datum)
	Q_PROPERTY(QList<QPair<int,ParserTypes::Expression::Span>> sequence MEMBER sequence) //dont use typedef to not confuse json serializer

public:
	Q_INVOKABLE Type(QObject *parent = nullptr);

	QDateTime nextDateTime(const QDateTime &since) const;

	bool isDatum;
	Datum *datum;
	Sequence sequence;
};

class TimePoint : public QObject
{
	Q_OBJECT

	Q_PROPERTY(Mode mode MEMBER mode)
	Q_PROPERTY(QDate date MEMBER date)
	Q_PROPERTY(ParserTypes::Datum* datum MEMBER datum)

public:
	enum Mode {
		InvalidMode,
		DateMode,
		DatumMode,
		YearMode
	};
	Q_ENUM(Mode)

	Q_INVOKABLE TimePoint(QObject *parent = nullptr);

	bool isLess(const TimePoint *other) const;
	QDate nextDate(QDate wDate, bool notToday = true) const;

	Mode mode;
	QDate date;
	Datum *datum;
};

// ------------- Expressions -------------

class Conjunction : public Expression
{
	Q_OBJECT

public:
	Conjunction(QObject *parent = nullptr);
	Schedule *createSchedule(const QDateTime &since, QTime defaultTime, QObject *parent = nullptr) override;

	QList<Expression*> expressions;
};

class TimeSpan : public Expression
{
	Q_OBJECT

public:
	TimeSpan(QObject *parent = nullptr);
	Schedule *createSchedule(const QDateTime &since, QTime defaultTime, QObject *parent = nullptr) override;

	Sequence sequence;
	Datum *datum;
	QTime time;
};

class Loop : public Expression
{
	Q_OBJECT

public:
	Loop(QObject *parent = nullptr);
	Schedule *createSchedule(const QDateTime &since, QTime defaultTime, QObject *parent = nullptr) override;

	Type *type;
	Datum *datum;
	QTime time;
	TimePoint *from;
	QTime fromTime;
	TimePoint *until;
	QTime untilTime;
};

class Point : public Expression
{
	Q_OBJECT

public:
	Point(QObject *parent = nullptr);
	Schedule *createSchedule(const QDateTime &since, QTime defaultTime, QObject *parent = nullptr) override;

	TimePoint *date;
	QTime time;
};

}

class DateParser : public QObject
{
	Q_OBJECT

public:
	enum WordKey {
		TimeRegexKey,
		TimeKey,
		DateKey,
		MonthDayKey,
		TodayKey,
		TomorrowKey,

		SpanMinuteKey,
		SpanHourKey,
		SpanDayKey,
		SpanWeekKey,
		SpanMonthKey,
		SpanYearKey,
		AllSpans,

		DatumKey,
		SequenceKey,

		ConjunctionKey,
		TimeSpanKey,
		LoopKey,
		FromKey,
		UntilKey,
		PointKey
	};
	Q_ENUM(WordKey)

	explicit DateParser(QObject *parent = nullptr);

	QSharedPointer<ParserTypes::Expression> parse(const QString &data);

	QDateTime snoozeParse(const QString &expression);

	QString lastError() const;

private:
	QString _lastError;

	static QString word(WordKey key);
	static QString timeRegex();
	static QString sequenceRegex();

	ParserTypes::Expression *parseExpression(const QString &data, QObject *parent);
	ParserTypes::Conjunction *tryParseConjunction(const QString &data, QObject *parent);
	ParserTypes::TimeSpan *tryParseTimeSpan(const QString &data, QObject *parent);
	ParserTypes::Loop *tryParseLoop(const QString &data, QObject *parent);
	ParserTypes::Point *tryParsePoint(const QString &data, QObject *parent);

	ParserTypes::Datum *parseDatum(const QString &data, QObject *parent);
	ParserTypes::Type *parseType(const QString &data, QObject *parent);
	ParserTypes::TimePoint *parseTimePoint(const QString &data, QObject *parent);
	QPair<ParserTypes::TimePoint*, QTime> parseExtendedTimePoint(const QString &data, QObject *parent);

	QDate parseMonthDay(const QString &data, bool noThrow = false);
	QDate parseDate(const QString &data, bool noThrow = false);
	QTime parseTime(const QString &data);
	ParserTypes::Expression::Span parseSpan(const QString &data);
	ParserTypes::Sequence parseSequence(const QString &data);

	void validateDatumDatum(ParserTypes::Datum *datum, const ParserTypes::Datum *extraDatum);
	void validateSequenceDatum(const ParserTypes::Sequence &sequence, const ParserTypes::Datum *datum, const QTime &time);
	void validateSpanDatum(ParserTypes::Expression::Span span, const ParserTypes::Datum *datum, const QTime &time);

	static QMap<QString, int> readWeekDays();
	static QMap<QString, int> readMonths();
};

Q_DECLARE_METATYPE(ParserTypes::Datum*)
Q_DECLARE_METATYPE(ParserTypes::Type*)
Q_DECLARE_METATYPE(ParserTypes::TimePoint*)

#endif // DATEPARSER_H