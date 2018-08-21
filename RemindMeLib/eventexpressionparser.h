#ifndef EVENTEXPRESSIONPARSER_H
#define EVENTEXPRESSIONPARSER_H

#include <QObject>
#include <QReadWriteLock>
#include <QUuid>
#include <QVector>
#include <QtMvvmCore/Injection>

#include "remindmelib_global.h"
#include <syncedsettings.h>

class Schedule;
class EventExpressionParser;

namespace Expressions {

enum TypeFlag {
	InvalidType = 0x00,

	Timepoint = 0x01,
	Timespan = 0x02,

	// No flag means relative, unlooped
	FlagAbsolute = 0x10,
	FlagLooped = 0x20,

	AbsoluteTimepoint = Timepoint | FlagAbsolute,
	LoopedTimePoint = Timepoint | FlagLooped,
	LoopedTimeSpan = Timespan | FlagLooped

	// TODO ... and when in loops, use the timepoints as "from+until" restriction
	// TODO add from/until for limitation of spans
	// TODO add ISO(/RFC) date support
};
Q_DECLARE_FLAGS(Type, TypeFlag)

enum ScopeFlag {
	InvalidScope = 0x00,

	Year = 0x01,
	Month = 0x02,
	Week = 0x04,
	Day = 0x08,
	WeekDay = Day,
	MonthDay = Week | Day,
	Hour = 0x10,
	Minute = 0x20,
};
Q_DECLARE_FLAGS(Scope, ScopeFlag)

enum WordKey {
	TimePrefix,
	TimeSuffix,
	TimePattern,

	DatePrefix,
	DateSuffix,
	DatePattern,

	InvTimeExprPattern,
	InvTimeHourPattern,
	InvTimeMinutePattern,
	InvTimeKeyword,

	MonthDayPrefix,
	MonthDaySuffix,
	MonthDayLoopPrefix,
	MonthDayLoopSuffix,
	MonthDayIndicator,

	WeekDayPrefix,
	WeekDaySuffix,
	WeekDayLoopPrefix,
	WeekDayLoopSuffix,

	MonthPrefix,
	MonthSuffix,
	MonthLoopPrefix,
	MonthLoopSuffix,

	YearPrefix,
	YearSuffix,

	SpanPrefix,
	SpanSuffix,
	SpanLoopPrefix,
	SpanConjuction,
	SpanKeyMinute,
	SpanKeyHour,
	SpanKeyDay,
	SpanKeyWeek,
	SpanKeyMonth,
	SpanKeyYear,

	KeywordDayspan,

	ExpressionSeperator
};

} // break namespace to declare flag operators

Q_DECLARE_OPERATORS_FOR_FLAGS(Expressions::Type)
Q_DECLARE_OPERATORS_FOR_FLAGS(Expressions::Scope)

namespace Expressions { //.. and continue it

class REMINDMELIBSHARED_EXPORT SubTerm
{
	Q_DISABLE_COPY(SubTerm)
public:
	inline SubTerm(Type t = InvalidType, Scope s = InvalidScope, bool c = false) :
		type{t},
		scope{s},
		certain{c}
	{}
	virtual ~SubTerm();

	Type type;
	Scope scope;
	bool certain;

	virtual void apply(QDateTime &datetime, bool applyRelative) const = 0;
};

// sub-expession terms that use Qt date/time formats

class REMINDMELIBSHARED_EXPORT TimeTerm : public SubTerm
{
public:
	TimeTerm(QTime time, bool certain);
	static std::pair<QSharedPointer<TimeTerm>, int> parse(const QStringRef &expression);
	void apply(QDateTime &datetime, bool keepOffset) const override;

private:
	QTime _time;

	static QString toRegex(QString pattern);
};

class REMINDMELIBSHARED_EXPORT DateTerm : public SubTerm
{
public:
	DateTerm(QDate date, bool hasYear, bool certain);
	static std::pair<QSharedPointer<DateTerm>, int> parse(const QStringRef &expression);
	void apply(QDateTime &datetime, bool applyRelative) const override;

private:
	QDate _date;

	static QString toRegex(QString pattern, bool &hasYear);
};

// time expression for worded times like "10 past 11"
class REMINDMELIBSHARED_EXPORT InvertedTimeTerm : public SubTerm
{
public:
	InvertedTimeTerm(QTime time);
	static std::pair<QSharedPointer<InvertedTimeTerm>, int> parse(const QStringRef &expression);
	void apply(QDateTime &datetime, bool applyRelative) const override;

private:
	QTime _time;

	static QString hourToRegex(QString pattern);
	static QString minToRegex(QString pattern);
};

class REMINDMELIBSHARED_EXPORT MonthDayTerm : public SubTerm
{
public:
	MonthDayTerm(int day, bool looped, bool certain);
	static std::pair<QSharedPointer<MonthDayTerm>, int> parse(const QStringRef &expression);
	void apply(QDateTime &datetime, bool applyRelative) const override;

private:
	int _day;
};

class REMINDMELIBSHARED_EXPORT WeekDayTerm : public SubTerm
{
public:
	WeekDayTerm(int weekDay, bool looped, bool certain);
	static std::pair<QSharedPointer<WeekDayTerm>, int> parse(const QStringRef &expression);
	void apply(QDateTime &datetime, bool applyRelative) const override;

private:
	int _weekDay;
};

class REMINDMELIBSHARED_EXPORT MonthTerm : public SubTerm
{
public:
	MonthTerm(int month, bool looped, bool certain);
	static std::pair<QSharedPointer<MonthTerm>, int> parse(const QStringRef &expression);
	void apply(QDateTime &datetime, bool applyRelative) const override;

private:
	int _month;
};

class REMINDMELIBSHARED_EXPORT YearTerm : public SubTerm
{
public:
	YearTerm(int year, bool certain);
	static std::pair<QSharedPointer<YearTerm>, int> parse(const QStringRef &expression);
	void apply(QDateTime &datetime, bool applyRelative) const override;

private:
	int _year;
};

class REMINDMELIBSHARED_EXPORT SequenceTerm : public SubTerm
{
public:
	using Sequence = QMap<ScopeFlag, int>;
	SequenceTerm(Sequence &&sequence, bool looped, bool certain);
	static std::pair<QSharedPointer<SequenceTerm>, int> parse(const QStringRef &expression);
	void apply(QDateTime &datetime, bool applyRelative) const override;

private:
	Sequence _sequence;
};

class REMINDMELIBSHARED_EXPORT KeywordTerm : public SubTerm
{
public:
	KeywordTerm(int days);
	static std::pair<QSharedPointer<KeywordTerm>, int> parse(const QStringRef &expression);
	void apply(QDateTime &datetime, bool applyRelative) const override;

private:
	int _days;
};

class REMINDMELIBSHARED_EXPORT Term : public QList<QSharedPointer<const SubTerm>>
{
public:
	Term() = default;
	Term(const Term &other) = default;
	Term(Term &&other) = default;
	Term& operator=(const Term &other) = default;
	Term& operator=(Term &&other) = default;

	Term(std::initializer_list<QSharedPointer<const SubTerm>> args);

	Scope scope() const;
	bool isLooped() const;
	bool isAbsolute() const;

	QDateTime apply(QDateTime datetime) const;

private:
	friend class ::EventExpressionParser;
	void finalize();

	Scope _scope = InvalidScope;
	bool _looped = false;
	bool _absolute = false;
};

using TermSelection = QList<Term>;
using MultiTerm = QVector<TermSelection>;

// general helper method
QString trWord(WordKey key, bool escape = true);
QStringList trList(WordKey key, bool escape = true, bool sort = true);

QString dateTimeFormatToRegex(QString pattern, const std::function<void(QString&)> &replacer);

}

class REMINDMELIBSHARED_EXPORT EventExpressionParser : public QObject
{
	Q_OBJECT

	QTMVVM_INJECT_PROP(SyncedSettings*, settings, _settings)

public:
	Q_INVOKABLE explicit EventExpressionParser(QObject *parent = nullptr);

	Expressions::MultiTerm parseMultiExpression(const QString &expression);
	Expressions::TermSelection parseExpression(const QString &expression);

	QSharedPointer<Schedule> parseSchedule(const Expressions::Term &term);
	QDateTime parseSnoozeTime(const Expressions::Term &term);

Q_SIGNALS:
	void termCompleted(QUuid termId, int termIndex, const Expressions::Term &term);
	void operationCompleted(QUuid doneId);

private:
	SyncedSettings *_settings = nullptr;

	QReadWriteLock _taskLocker;
	QHash<QUuid, QAtomicInt> _taskCounter;

	Expressions::MultiTerm parseExpressionImpl(const QString &expression, bool allowMulti);

	// direct invokations
	void parseTerm(QUuid id, const QStringRef &expression, const Expressions::Term &term, int termIndex);
	bool validatePartialTerm(const Expressions::Term &term);
	bool validateFullTerm(Expressions::Term &term);
	// async invokations
	void parseMultiTerm(QUuid id, const QString &expression, Expressions::MultiTerm *terms);
	template <typename TSubTerm>
	void parseSubTerm(QUuid id, const QStringRef &expression, Expressions::Term term, int termIndex);

	void addTasks(QUuid id, int count);
	void completeTask(QUuid id);
};

Q_DECLARE_METATYPE(Expressions::Term)
Q_DECLARE_METATYPE(Expressions::TermSelection)
Q_DECLARE_METATYPE(Expressions::MultiTerm)

#endif // EVENTEXPRESSIONPARSER_H