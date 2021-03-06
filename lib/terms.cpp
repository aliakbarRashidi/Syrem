#include "terms.h"
#include <QLocale>
#include <QMetaEnum>
using namespace Expressions;

TimeTerm::TimeTerm(QTime time) :
	SubTerm{Timepoint, Hour | Minute},
	_time{time}
{}

TimeTerm::TimeTerm(QObject *parent) :
	SubTerm{parent}
{}

std::pair<QSharedPointer<TimeTerm>, int> TimeTerm::parse(const QStringRef &expression)
{
	const QLocale locale;
	const auto prefix = QStringLiteral("(?:%1)?").arg(trList(TimePrefix).join(QLatin1Char('|')));
	const auto suffix = QStringLiteral("(?:%1)?").arg(trList(TimeSuffix).join(QLatin1Char('|')));

	for(const auto &pattern : trList(TimePattern, false)) {
		QRegularExpression regex {
			QLatin1Char('^') + prefix + QLatin1Char('(') + toRegex(pattern) + QLatin1Char(')') + suffix + QStringLiteral("\\s*"),
			QRegularExpression::DontAutomaticallyOptimizeOption |
			QRegularExpression::CaseInsensitiveOption |
			QRegularExpression::UseUnicodePropertiesOption
		};
		auto match = regex.match(expression);
		if(match.hasMatch()) {
			auto time = locale.toTime(match.captured(1), pattern);
			if(time.isValid()) {
				return {
					QSharedPointer<TimeTerm>::create(time),
					match.capturedLength(0)
				};
			}
		}
	}

	return {};
}

void TimeTerm::apply(QDateTime &datetime, bool applyFenced) const
{
	Q_UNUSED(applyFenced)
	datetime.setTime(_time);
}

void TimeTerm::fixup(QDateTime &datetime) const
{
	datetime = datetime.addDays(1);
}

QString TimeTerm::describe() const
{
	return QLocale().toString(_time, tr("hh:mm"));
}

std::pair<QString, QString> TimeTerm::syntax(bool asLoop)
{
	if(asLoop)
		return {};
	else {
		const auto prefix = trList(TimePrefix, false, false);
		const auto suffix = trList(TimeSuffix, false, false);
		return {
			tr("time"),
			tr("%1 {0..24}[:{0..60}] %2 (and other similar time-formats)")
					.arg(prefix.isEmpty() ?
							 QString{} :
							 QStringLiteral("[%1]").arg(prefix.join(QLatin1Char('|'))),
						 suffix.isEmpty() ?
							 QString{} :
							 QStringLiteral("[%1]").arg(suffix.join(QLatin1Char('|'))))
		};
	}
}

QString TimeTerm::toRegex(QString pattern)
{
	const QLocale locale;
	return dateTimeFormatToRegex(std::move(pattern), [&locale](QString &text) {
		text.replace(QStringLiteral("hh"), QStringLiteral("\\d{2}"))
				.replace(QStringLiteral("h"), QStringLiteral("\\d{1,2}"))
				.replace(QStringLiteral("mm"), QStringLiteral("\\d{2}"))
				.replace(QStringLiteral("m"), QStringLiteral("\\d{1,2}"))
				.replace(QStringLiteral("ss"), QStringLiteral("\\d{2}"))
				.replace(QStringLiteral("s"), QStringLiteral("\\d{1,2}"))
				.replace(QStringLiteral("zzz"), QStringLiteral("\\d{3}"))
				.replace(QStringLiteral("z"), QStringLiteral("\\d{1,3}"))
				.replace(QStringLiteral("ap"),
						 QStringLiteral("(?:%1|%2)")
						 .arg(QRegularExpression::escape(locale.amText()),
							  QRegularExpression::escape(locale.pmText())),
						 Qt::CaseInsensitive);
	});
}



DateTerm::DateTerm(QDate date, bool hasYear, bool isLooped) :
	SubTerm {
		hasYear ? AbsoluteTimepoint : (isLooped ? LoopedTimePoint : Timepoint),
		(hasYear ? Year : InvalidScope) | Month | MonthDay
	},
	_date{date}
{
	Q_ASSERT(!(hasYear && isLooped));
}

DateTerm::DateTerm(QObject *parent) :
	SubTerm{parent}
{}

std::pair<QSharedPointer<DateTerm>, int> DateTerm::parse(const QStringRef &expression)
{
	const QLocale locale;
	const auto prefix = QStringLiteral("(?:%1)?").arg(trList(DatePrefix).join(QLatin1Char('|')));
	const auto suffix = QStringLiteral("(?:%1)?").arg(trList(DateSuffix).join(QLatin1Char('|')));
	QVector<std::tuple<QString, QString, bool>> patterns;
	{
		auto pList = trList(DatePattern, false);
		patterns.reserve(pList.size());
		for(auto &pattern : pList) {
			bool hasYear = false;
			auto escaped = toRegex(pattern, hasYear);
			patterns.append(std::make_tuple(std::move(escaped), std::move(pattern), hasYear));
		}

	}
	// prepare list of combos to try. can be {loop, suffix}, {prefix, loop} or {prefix, suffix}, but the first two only if a loop*fix is defined
	QVector<std::tuple<QString, QString, bool>> exprCombos;
	exprCombos.reserve(3);
	{
		const auto loopPrefix = trList(DateLoopPrefix);
		if(!loopPrefix.isEmpty())
			exprCombos.append(std::make_tuple(QStringLiteral("(?:%1)").arg(loopPrefix.join(QLatin1Char('|'))), suffix, true));
	}
	{
		const auto loopSuffix = trList(DateLoopSuffix);
		if(!loopSuffix.isEmpty())
			exprCombos.append(std::make_tuple(prefix, QStringLiteral("(?:%1)").arg(loopSuffix.join(QLatin1Char('|'))), true));
	}
	exprCombos.append(std::make_tuple(prefix, suffix, false));

	for(const auto &loopCombo : exprCombos) { // (prefix, suffix, isLooped)
		for(const auto &patternInfo : patterns) { // (regex, pattern, isYear)
			if(std::get<2>(loopCombo) && std::get<2>(patternInfo)) // skip year expressions for loops
				continue;
			QRegularExpression regex {
				QLatin1Char('^') + std::get<0>(loopCombo) +
				QLatin1Char('(') + std::get<0>(patternInfo) + QLatin1Char(')') +
				std::get<1>(loopCombo) + QStringLiteral("\\s*"),
				QRegularExpression::DontAutomaticallyOptimizeOption |
				QRegularExpression::CaseInsensitiveOption |
				QRegularExpression::UseUnicodePropertiesOption
			};
			auto match = regex.match(expression);
			if(match.hasMatch()) {
				auto date = locale.toDate(match.captured(1), std::get<1>(patternInfo));
				if(date.isValid()) {
					return {
						QSharedPointer<DateTerm>::create(date, std::get<2>(patternInfo), std::get<2>(loopCombo)),
						match.capturedLength(0)
					};
				}
			}
		}
	}

	return {};
}

void DateTerm::apply(QDateTime &datetime, bool applyFenced) const
{
	Q_UNUSED(applyFenced)
	if(scope.testFlag(Year)) //set the whole date
		datetime.setDate(_date);
	else { // set only day and month, keep year
		datetime.setDate({
							 datetime.date().year(),
							 _date.month(),
							 _date.day()
						 });
	}
}

void DateTerm::fixup(QDateTime &datetime) const
{
	if(!scope.testFlag(Year))
		datetime = datetime.addYears(1);
}

QString DateTerm::describe() const
{
	return QLocale().toString(_date, scope.testFlag(Year) ?  tr("yyyy-MM-dd") : tr("MM-dd"));
}

std::pair<QString, QString> DateTerm::syntax(bool asLoop)
{
	QStringList prefix;
	QStringList suffix;
	if(asLoop) {
		prefix = trList(DateLoopPrefix, false, false);
		suffix = trList(DateLoopSuffix, false, false);
	} else {
		prefix = trList(DatePrefix, false, false);
		suffix = trList(DateSuffix, false, false);
	}
	return {
		tr("date"),
		tr("%1 {1..31}.{1..12}.[<year>] %2 (and other similar date-formats)")
				.arg(prefix.isEmpty() ?
						 QString{} :
						 QStringLiteral("[%1]").arg(prefix.join(QLatin1Char('|'))),
					 suffix.isEmpty() ?
						 QString{} :
						 QStringLiteral("[%1]").arg(suffix.join(QLatin1Char('|'))))
	};
}

QString DateTerm::toRegex(QString pattern, bool &hasYear)
{
	hasYear = false;
	return dateTimeFormatToRegex(std::move(pattern), [&hasYear](QString &text) {
		text.replace(QStringLiteral("dd"), QStringLiteral("\\d{2}"))
				.replace(QRegularExpression{QStringLiteral(R"__((?<!\\)((?:\\{2})*)d)__")}, QStringLiteral("\\1\\d{1,2}")) //special regex to not include previous "\d" replacements, but allow "\\d"
				.replace(QStringLiteral("MM"), QStringLiteral("\\d{2}"))
				.replace(QStringLiteral("M"), QStringLiteral("\\d{1,2}"));
		auto oldLen = text.size();
		text.replace(QStringLiteral("yyyy"), QStringLiteral("-?\\d{4}"))
				.replace(QStringLiteral("yy"), QStringLiteral("\\d{2}"));
		hasYear = hasYear || (text.size() != oldLen);
	});
}



InvertedTimeTerm::InvertedTimeTerm(QTime time) :
	SubTerm{Timepoint, Hour | Minute},
	_time{time}
{}

InvertedTimeTerm::InvertedTimeTerm(QObject *parent) :
	SubTerm{parent}
{}

std::pair<QSharedPointer<InvertedTimeTerm>, int> InvertedTimeTerm::parse(const QStringRef &expression)
{
	const QLocale locale;
	// prepare suffix/prefix
	const auto prefix = QStringLiteral("(?:%1)?").arg(trList(TimePrefix).join(QLatin1Char('|')));
	const auto suffix = QStringLiteral("(?:%1)?").arg(trList(TimeSuffix).join(QLatin1Char('|')));

	// prepare primary expression patterns
	QHash<QString, int> keywordMap;
	QString keywordRegexStr;
	for(const auto &mapping : trList(InvTimeKeyword, false)) {
		const auto split = mapping.split(QLatin1Char(':'));
		Q_ASSERT_X(split.size() == 2, Q_FUNC_INFO, "Invalid InvTimeKeyword translation. Must be keyword and value, seperated by a ':'");
		keywordMap.insert(split[0], split[1].toInt());
		keywordRegexStr.append(QLatin1Char('|') + QRegularExpression::escape(split[0]));
	}
	// prepare hour/minute patterns
	QVector<std::pair<QString, QString>> hourPatterns;
	{
		const auto pList = trList(InvTimeHourPattern, false);
		hourPatterns.reserve(pList.size());
		for(auto &pattern : pList)
			hourPatterns.append({pattern, hourToRegex(pattern)});
	}
	QVector<std::pair<QString, QString>> minPatterns;
	{
		const auto pList = trList(InvTimeMinutePattern, false);
		minPatterns.reserve(pList.size());
		for(auto &pattern : pList)
			minPatterns.append({pattern, minToRegex(pattern)});
	}

	for(const auto &exprPattern : trList(InvTimeExprPattern, false)) {
		const auto split = exprPattern.split(QLatin1Char(':'));
		Q_ASSERT_X(split.size() == 2 && (split[1] == QLatin1Char('+') || split[1] == QLatin1Char('-')),
				Q_FUNC_INFO,
				"Invalid InvTimePattern translation. Must be an expression and sign (+/-), seperated by a ':'");

		for(const auto &hourPattern : hourPatterns) {
			for(const auto &minPattern : minPatterns) {
				QRegularExpression regex {
					QLatin1Char('^') + prefix +
					split[0].arg(QStringLiteral(R"__((?<hours>%1))__").arg(hourPattern.second),
								 QStringLiteral(R"__((?<minutes>%1%2))__").arg(minPattern.second, keywordRegexStr)) +
					suffix + QStringLiteral("\\s*"),
					QRegularExpression::DontAutomaticallyOptimizeOption |
					QRegularExpression::CaseInsensitiveOption |
					QRegularExpression::UseUnicodePropertiesOption
				};
				auto match = regex.match(expression);
				if(match.hasMatch()) {
					// extract minutes and hours from the expression
					auto hours = locale.toTime(match.captured(QStringLiteral("hours")), hourPattern.first).hour();
					auto minutesStr = match.captured(QStringLiteral("minutes"));
					auto minutes = keywordMap.contains(minutesStr) ?
									   keywordMap.value(minutesStr) :
									   locale.toTime(minutesStr, minPattern.first).minute();
					//negative minutes (i.e. 10 to 4 -> 3:50)
					if(split[1] == QLatin1Char('-')) {
						hours = (hours == 0 ? 23 : hours - 1);
						minutes = 60 - minutes;
					}
					QTime time{hours, minutes};
					if(time.isValid()) {
						return {
							QSharedPointer<InvertedTimeTerm>::create(time),
							match.capturedLength(0)
						};
					}
				}
			}
		}
	}

	return {};
}

void InvertedTimeTerm::apply(QDateTime &datetime, bool applyFenced) const
{
	Q_UNUSED(applyFenced)
	datetime.setTime(_time);
}

void InvertedTimeTerm::fixup(QDateTime &datetime) const
{
	datetime = datetime.addDays(1);
}

QString InvertedTimeTerm::describe() const
{
	return QLocale().toString(_time, tr("hh:mm"));
}

std::pair<QString, QString> InvertedTimeTerm::syntax(bool asLoop)
{
	if(asLoop)
		return {};
	else {
		const auto prefix = trList(TimePrefix, false, false);
		const auto suffix = trList(TimeSuffix, false, false);
		return {
			tr("time"),
			tr("%1 {half|quarter|0..60} past|to {0..24} %2")
					.arg(prefix.isEmpty() ?
							 QString{} :
							 QStringLiteral("[%1]").arg(prefix.join(QLatin1Char('|'))),
						 suffix.isEmpty() ?
							 QString{} :
							 QStringLiteral("[%1]").arg(suffix.join(QLatin1Char('|'))))
		};
	}
}

QString InvertedTimeTerm::hourToRegex(QString pattern)
{
	const QLocale locale;
	return dateTimeFormatToRegex(std::move(pattern), [&locale](QString &text) {
		text.replace(QStringLiteral("hh"), QStringLiteral("\\d{2}"))
				.replace(QStringLiteral("h"), QStringLiteral("\\d{1,2}"))
				.replace(QStringLiteral("ap"),
						 QStringLiteral("(?:%1|%2)")
						 .arg(QRegularExpression::escape(locale.amText()),
							  QRegularExpression::escape(locale.pmText())),
						 Qt::CaseInsensitive);
	});
}

QString InvertedTimeTerm::minToRegex(QString pattern)
{
	return dateTimeFormatToRegex(std::move(pattern), [](QString &text) {
		text.replace(QStringLiteral("mm"), QStringLiteral("\\d{2}"))
				.replace(QStringLiteral("m"), QStringLiteral("\\d{1,2}"));
	});
}



MonthDayTerm::MonthDayTerm(int day, bool looped) :
	SubTerm{looped ? LoopedTimePoint : Timepoint, MonthDay},
	_day{day}
{}

MonthDayTerm::MonthDayTerm(QObject *parent) :
	SubTerm{parent}
{}

std::pair<QSharedPointer<MonthDayTerm>, int> MonthDayTerm::parse(const QStringRef &expression)
{
	// get and prepare standard *fixes and indicators
	const auto prefix = QStringLiteral("(?:%1)?").arg(trList(MonthDayPrefix).join(QLatin1Char('|')));
	const auto suffix = QStringLiteral("(?:%1)?").arg(trList(MonthDaySuffix).join(QLatin1Char('|')));
	auto indicators = trList(MonthDayIndicator, false);
	for(auto &indicator : indicators) {
		const auto split = indicator.split(QLatin1Char('_'));
		Q_ASSERT_X(split.size() == 2, Q_FUNC_INFO, "Invalid MonthDayIndicator translation. Must be some indicator text with a '_' as date placeholder");
		indicator = QRegularExpression::escape(split[0]) + QStringLiteral("(\\d{1,2})") + QRegularExpression::escape(split[1]);
	}

	// prepare list of combos to try. can be {loop, suffix}, {prefix, loop} or {prefix, suffix}, but the first two only if a loop*fix is defined
	QVector<std::tuple<QString, QString, bool>> exprCombos;
	exprCombos.reserve(3);
	{
		const auto loopPrefix = trList(MonthDayLoopPrefix);
		if(!loopPrefix.isEmpty())
			exprCombos.append(std::make_tuple(QStringLiteral("(?:%1)").arg(loopPrefix.join(QLatin1Char('|'))), suffix, true));
	}
	{
		const auto loopSuffix = trList(MonthDayLoopSuffix);
		if(!loopSuffix.isEmpty())
			exprCombos.append(std::make_tuple(prefix, QStringLiteral("(?:%1)").arg(loopSuffix.join(QLatin1Char('|'))), true));
	}
	exprCombos.append(std::make_tuple(prefix, suffix, false));

	// then loop through all the combinations and try to find one
	for(const auto &loopCombo : exprCombos) {
		for(const auto &indicator : indicators) {
			QRegularExpression regex {
				QLatin1Char('^') + std::get<0>(loopCombo) + indicator + std::get<1>(loopCombo) + QStringLiteral("\\s*"),
				QRegularExpression::DontAutomaticallyOptimizeOption |
				QRegularExpression::CaseInsensitiveOption |
				QRegularExpression::UseUnicodePropertiesOption
			};
			auto match = regex.match(expression);
			if(match.hasMatch()) {
				bool ok = false;
				auto day = match.captured(1).toInt(&ok);
				if(ok && day >= 1 && day <= 31) {
					auto isLoop = std::get<2>(loopCombo);
					return {
						QSharedPointer<MonthDayTerm>::create(day, isLoop),
						match.capturedLength(0)
					};
				}
			}
		}
	}

	return {};
}

void MonthDayTerm::apply(QDateTime &datetime, bool applyFenced) const
{
	Q_UNUSED(applyFenced)
	auto date = datetime.date();
	datetime.setDate({
		date.year(),
		date.month(),
		std::min(_day, date.daysInMonth()) // shorten day to target month
	});
}

void MonthDayTerm::fixup(QDateTime &datetime) const
{
	datetime = datetime.addMonths(1);
	apply(datetime, false); //apply again to fix the day for cases like "every 31st"
}

QString MonthDayTerm::describe() const
{
	return tr("%1.").arg(_day);
}

std::pair<QString, QString> MonthDayTerm::syntax(bool asLoop)
{
	QStringList prefix;
	QStringList suffix;
	if(asLoop) {
		prefix = trList(MonthDayLoopPrefix, false, false);
		suffix = trList(MonthDayLoopSuffix, false, false);
	} else {
		prefix = trList(MonthDayPrefix, false, false);
		suffix = trList(MonthDaySuffix, false, false);
	}
	return {
		tr("day"),
		tr("%1 {1..31}{.|th|st|nd|rd} %2")
				.arg(prefix.isEmpty() ?
						 QString{} :
						 QStringLiteral("[%1]").arg(prefix.join(QLatin1Char('|'))),
					 suffix.isEmpty() ?
						 QString{} :
						 QStringLiteral("[%1]").arg(suffix.join(QLatin1Char('|'))))
	};
}



WeekDayTerm::WeekDayTerm(int weekDay, bool looped) :
	SubTerm{(looped ? LoopedTimePoint : Timepoint) | FlagNeedsFixupCleanup, WeekDay},
	_weekDay{weekDay}
{}

WeekDayTerm::WeekDayTerm(QObject *parent) :
	SubTerm{parent}
{}

std::pair<QSharedPointer<WeekDayTerm>, int> WeekDayTerm::parse(const QStringRef &expression)
{
	const QLocale locale;
	// get and prepare standard *fixes and indicators
	const auto prefix = QStringLiteral("(?:%1)?").arg(trList(WeekDayPrefix).join(QLatin1Char('|')));
	const auto suffix = QStringLiteral("(?:%1)?").arg(trList(WeekDaySuffix).join(QLatin1Char('|')));
	QString shortDays;
	QString longDays;
	{
		QStringList sList;
		QStringList lList;
		sList.reserve(14);
		lList.reserve(14);
		for(auto i = 1; i <= 7; i++) {
			sList.append(QRegularExpression::escape(locale.dayName(i, QLocale::ShortFormat)));
			sList.append(QRegularExpression::escape(locale.standaloneDayName(i, QLocale::ShortFormat)));
			lList.append(QRegularExpression::escape(locale.dayName(i, QLocale::LongFormat)));
			lList.append(QRegularExpression::escape(locale.standaloneDayName(i, QLocale::LongFormat)));
		}
		sList.removeDuplicates();
		lList.removeDuplicates();
		shortDays = sList.join(QLatin1Char('|'));
		longDays = lList.join(QLatin1Char('|'));
	}

	// prepare list of combos to try. can be {loop, suffix}, {prefix, loop} or {prefix, suffix}, but the first two only if a loop*fix is defined
	QVector<std::tuple<QString, QString, bool>> exprCombos;
	exprCombos.reserve(3);
	{
		const auto loopPrefix = trList(WeekDayLoopPrefix);
		if(!loopPrefix.isEmpty())
			exprCombos.append(std::make_tuple(QStringLiteral("(?:%1)").arg(loopPrefix.join(QLatin1Char('|'))), suffix, true));
	}
	{
		const auto loopSuffix = trList(WeekDayLoopSuffix);
		if(!loopSuffix.isEmpty())
			exprCombos.append(std::make_tuple(prefix, QStringLiteral("(?:%1)").arg(loopSuffix.join(QLatin1Char('|'))), true));
	}
	exprCombos.append(std::make_tuple(prefix, suffix, false));

	// then loop through all the combinations and try to find one
	for(const auto &loopCombo : exprCombos) {
		for(const auto &dayType : {
				std::make_pair(longDays, QStringLiteral("dddd")),
				std::make_pair(shortDays, QStringLiteral("ddd"))
			}) {
			QRegularExpression regex {
				QLatin1Char('^') + std::get<0>(loopCombo) +
				QLatin1Char('(') + dayType.first + QLatin1Char(')') +
				std::get<1>(loopCombo) + QStringLiteral("\\s*"),
				QRegularExpression::DontAutomaticallyOptimizeOption |
				QRegularExpression::CaseInsensitiveOption |
				QRegularExpression::UseUnicodePropertiesOption
			};
			auto match = regex.match(expression);
			if(match.hasMatch()) {
				auto dayName = match.captured(1);
				auto dDate = locale.toDate(dayName, dayType.second);
				if(dDate.isValid()) {
					auto isLoop = std::get<2>(loopCombo);
					return {
						QSharedPointer<WeekDayTerm>::create(dDate.dayOfWeek(), isLoop),
						match.capturedLength(0)
					};
				}
			}
		}
	}

	return {};
}

void WeekDayTerm::apply(QDateTime &datetime, bool applyFenced) const
{
	auto date = datetime.date();
	// get the number of days to add to reach the target date
	date = date.addDays(_weekDay - date.dayOfWeek());
	if(applyFenced && date.month() < datetime.date().month()) {
		date = date.addDays(7);
		Q_ASSERT(date.month() == datetime.date().month());
	}
	else if(applyFenced && date.month() > datetime.date().month()) {
		date = date.addDays(-7);
		Q_ASSERT(date.month() == datetime.date().month());
	}

	Q_ASSERT(date.dayOfWeek() == _weekDay);
	datetime.setDate(date);
}

void WeekDayTerm::fixup(QDateTime &datetime) const
{
	datetime = datetime.addDays(7);
}

void WeekDayTerm::fixupCleanup(QDateTime &datetime) const
{
	if(datetime.date().dayOfWeek() != _weekDay)
		apply(datetime, true);
}

QString WeekDayTerm::describe() const
{
	return QLocale().standaloneDayName(_weekDay, QLocale::LongFormat);
}

std::pair<QString, QString> WeekDayTerm::syntax(bool asLoop)
{
	QStringList prefix;
	QStringList suffix;
	if(asLoop) {
		prefix = trList(WeekDayLoopPrefix, false, false);
		suffix = trList(WeekDayLoopSuffix, false, false);
	} else {
		prefix = trList(WeekDayPrefix, false, false);
		suffix = trList(WeekDaySuffix, false, false);
	}
	return {
		tr("weekday"),
		tr("%1 {Mon[day]..Sun[day]} %2")
				.arg(prefix.isEmpty() ?
						 QString{} :
						 QStringLiteral("[%1]").arg(prefix.join(QLatin1Char('|'))),
					 suffix.isEmpty() ?
						 QString{} :
						 QStringLiteral("[%1]").arg(suffix.join(QLatin1Char('|'))))
	};
}



MonthTerm::MonthTerm(int month, bool looped) :
	SubTerm{looped ? LoopedTimePoint : Timepoint, Month},
	_month{month}
{}

MonthTerm::MonthTerm(QObject *parent) :
	SubTerm{parent}
{}

std::pair<QSharedPointer<MonthTerm>, int> MonthTerm::parse(const QStringRef &expression)
{
	const QLocale locale;
	// get and prepare standard *fixes and indicators
	const auto prefix = QStringLiteral("(?:%1)?").arg(trList(MonthPrefix).join(QLatin1Char('|')));
	const auto suffix = QStringLiteral("(?:%1)?").arg(trList(MonthSuffix).join(QLatin1Char('|')));
	QString shortMonths;
	QString longMonths;
	{
		QStringList sList;
		QStringList lList;
		sList.reserve(24);
		lList.reserve(24);
		for(auto i = 1; i <= 12; i++) {
			sList.append(QRegularExpression::escape(locale.monthName(i, QLocale::ShortFormat)));
			sList.append(QRegularExpression::escape(locale.standaloneMonthName(i, QLocale::ShortFormat)));
			lList.append(QRegularExpression::escape(locale.monthName(i, QLocale::LongFormat)));
			lList.append(QRegularExpression::escape(locale.standaloneMonthName(i, QLocale::LongFormat)));
		}
		sList.removeDuplicates();
		lList.removeDuplicates();
		shortMonths = sList.join(QLatin1Char('|'));
		longMonths = lList.join(QLatin1Char('|'));
	}

	// prepare list of combos to try. can be {loop, suffix}, {prefix, loop} or {prefix, suffix}, but the first two only if a loop*fix is defined
	QVector<std::tuple<QString, QString, bool>> exprCombos;
	exprCombos.reserve(3);
	{
		const auto loopPrefix = trList(MonthLoopPrefix);
		if(!loopPrefix.isEmpty())
			exprCombos.append(std::make_tuple(QStringLiteral("(?:%1)").arg(loopPrefix.join(QLatin1Char('|'))), suffix, true));
	}
	{
		const auto loopSuffix = trList(MonthLoopSuffix);
		if(!loopSuffix.isEmpty())
			exprCombos.append(std::make_tuple(prefix, QStringLiteral("(?:%1)").arg(loopSuffix.join(QLatin1Char('|'))), true));
	}
	exprCombos.append(std::make_tuple(prefix, suffix, false));

	// then loop through all the combinations and try to find one
	for(const auto &loopCombo : exprCombos) {
		for(const auto &monthType : {
				std::make_pair(longMonths, QStringLiteral("MMMM")),
				std::make_pair(shortMonths, QStringLiteral("MMM"))
			}) {
			QRegularExpression regex {
				QLatin1Char('^') + std::get<0>(loopCombo) +
				QLatin1Char('(') + monthType.first + QLatin1Char(')') +
				std::get<1>(loopCombo) + QStringLiteral("\\s*"),
				QRegularExpression::DontAutomaticallyOptimizeOption |
				QRegularExpression::CaseInsensitiveOption |
				QRegularExpression::UseUnicodePropertiesOption
			};
			auto match = regex.match(expression);
			if(match.hasMatch()) {
				auto monthName = match.captured(1);
				auto mDate = locale.toDate(monthName, monthType.second);
				if(mDate.isValid()) {
					auto isLoop = std::get<2>(loopCombo);
					return {
						QSharedPointer<MonthTerm>::create(mDate.month(), isLoop),
						match.capturedLength(0)
					};
				}
			}
		}
	}

	return {};
}

void MonthTerm::apply(QDateTime &datetime, bool applyFenced) const
{
	Q_UNUSED(applyFenced)
	auto date = datetime.date();
	//always set to the first of the month. if a day was specified, that one is set after this
	datetime.setDate({date.year(), _month, 1});
}

void MonthTerm::fixup(QDateTime &datetime) const
{
	datetime = datetime.addYears(1);
}

QString MonthTerm::describe() const
{
	return QLocale().standaloneMonthName(_month, QLocale::LongFormat);
}

std::pair<QString, QString> MonthTerm::syntax(bool asLoop)
{
	QStringList prefix;
	QStringList suffix;
	if(asLoop) {
		prefix = trList(MonthLoopPrefix, false, false);
		suffix = trList(MonthLoopSuffix, false, false);
	} else {
		prefix = trList(MonthPrefix, false, false);
		suffix = trList(MonthSuffix, false, false);
	}
	return {
		tr("month"),
		tr("%1 {Jan[uary]..Dec[ember]} %2")
				.arg(prefix.isEmpty() ?
						 QString{} :
						 QStringLiteral("[%1]").arg(prefix.join(QLatin1Char('|'))),
					 suffix.isEmpty() ?
						 QString{} :
						 QStringLiteral("[%1]").arg(suffix.join(QLatin1Char('|'))))
	};
}



YearTerm::YearTerm(int year) :
	SubTerm{AbsoluteTimepoint, Year},
	_year{year}
{}

YearTerm::YearTerm(QObject *parent) :
	SubTerm{parent}
{}

std::pair<QSharedPointer<YearTerm>, int> YearTerm::parse(const QStringRef &expression)
{
	const auto prefix = QStringLiteral("(?:%1)?").arg(trList(YearPrefix).join(QLatin1Char('|')));
	const auto suffix = QStringLiteral("(?:%1)?").arg(trList(YearSuffix).join(QLatin1Char('|')));

	QRegularExpression regex {
		QLatin1Char('^') + prefix + QStringLiteral("(-?\\d{4,})") + suffix + QStringLiteral("\\s*"),
		QRegularExpression::DontAutomaticallyOptimizeOption |
		QRegularExpression::CaseInsensitiveOption |
		QRegularExpression::UseUnicodePropertiesOption
	};
	auto match = regex.match(expression);
	if(match.hasMatch()) {
		bool ok = false;
		auto year = match.captured(1).toInt(&ok);
		if(ok) {
			return {
				QSharedPointer<YearTerm>::create(year),
				match.capturedLength(0)
			};
		}
	}

	return {};
}

void YearTerm::apply(QDateTime &datetime, bool applyFenced) const
{
	Q_UNUSED(applyFenced)
	datetime.setDate({_year, 1, 1}); //set the year and reset month/date. They will be specified as needed
}

QString YearTerm::describe() const
{
	return QStringLiteral("%1").arg(_year, 4, 10, QLatin1Char('0'));
}

std::pair<QString, QString> YearTerm::syntax(bool asLoop)
{
	if(asLoop)
		return {};
	else {
		const auto prefix = trList(YearPrefix, false, false);
		const auto suffix = trList(YearSuffix, false, false);
		return {
			tr("year"),
			tr("%1 {<4-digit-number>} %2")
					.arg(prefix.isEmpty() ?
							 QString{} :
							 QStringLiteral("[%1]").arg(prefix.join(QLatin1Char('|'))),
						 suffix.isEmpty() ?
							 QString{} :
							 QStringLiteral("[%1]").arg(suffix.join(QLatin1Char('|'))))
		};
	}
}



SequenceTerm::SequenceTerm(Sequence &&sequence, bool looped) :
	SubTerm{looped ? LoopedTimeSpan : Timespan, [&]() {
		Scope scope = InvalidScope;
		for(auto it = sequence.constBegin(); it != sequence.constEnd(); ++it)
			scope |= it.key();
		return scope;
	}()},
	_sequence{std::move(sequence)}
{}

SequenceTerm::SequenceTerm(QObject *parent) :
	SubTerm{parent}
{}

std::pair<QSharedPointer<SequenceTerm>, int> SequenceTerm::parse(const QStringRef &expression)
{
	// get and prepare standard *fixes
	const auto prefix = QStringLiteral("(?:%1)?").arg(trList(SpanPrefix).join(QLatin1Char('|')));
	const auto suffix = QStringLiteral("(%1)").arg(trList(SpanSuffix).join(QLatin1Char('|')));
	const auto conjunctors = QStringLiteral("(%1)").arg(trList(SpanConjuction).join(QLatin1Char('|')));
	// prepare list of combos to try. can be {loop, suffix}, {prefix, loop} or {prefix, suffix}, but the first two only if a loop*fix is defined
	QVector<std::pair<QString, bool>> exprCombos;
	exprCombos.reserve(2);
	{
		const auto loopPrefix = trList(SpanLoopPrefix);
		if(!loopPrefix.isEmpty())
			exprCombos.append(std::make_pair(QStringLiteral("(?:%1)").arg(loopPrefix.join(QLatin1Char('|'))), true));
	}
	exprCombos.append(std::make_pair(prefix, false));

	// prepare lookup of span scopes
	QHash<QString, ScopeFlag> nameLookup;
	QString nameKey;
	{
		QStringList nameKeys;
		for(const auto &scopeInfo : {
				std::make_pair(SpanKeyMinute, Minute),
				std::make_pair(SpanKeyHour, Hour),
				std::make_pair(SpanKeyDay, Day),
				std::make_pair(SpanKeyWeek, Week),
				std::make_pair(SpanKeyMonth, Month),
				std::make_pair(SpanKeyYear, Year)
			}) {
			for(const auto &key : trList(scopeInfo.first, false, false)) {
				nameLookup.insert(key, scopeInfo.second);
				nameKeys.append(key);
			}
		}
		// sort by length to test the longest variants first
		std::sort(nameKeys.begin(), nameKeys.end(), [](const QString &lhs, const QString &rhs) {
			return lhs.size() > rhs.size();
		});
		// escape after sorting
		for(auto &key : nameKeys)
			key = QRegularExpression::escape(key);
		nameKey = nameKeys.join(QLatin1Char('|'));
	}

	for(const auto &loopCombo : exprCombos) {
		// check for the prefix
		QRegularExpression prefixRegex {
			QLatin1Char('^') + loopCombo.first,
			QRegularExpression::DontAutomaticallyOptimizeOption |
			QRegularExpression::CaseInsensitiveOption |
			QRegularExpression::UseUnicodePropertiesOption
		};
		auto prefixMatch = prefixRegex.match(expression);
		if(!prefixMatch.hasMatch())
			continue;

		// iterate through all "and" expressions
		auto offset = prefixMatch.capturedLength(0);
		QRegularExpression regex {
			QLatin1Char('^') +
			QStringLiteral("(?:(\\d+)\\s)%1").arg(loopCombo.second ? QString{QLatin1Char('?')} : QString{}) +
			QLatin1Char('(') + nameKey + QStringLiteral(")(?:") +
			conjunctors + QLatin1Char('|') + suffix + QStringLiteral(")?\\s*"),
			QRegularExpression::DontAutomaticallyOptimizeOption |
			QRegularExpression::CaseInsensitiveOption |
			QRegularExpression::UseUnicodePropertiesOption
		};

		Sequence sequence;
		forever {
			auto match = regex.match(expression.mid(offset));
			if(match.hasMatch()) {
				// get the scope
				auto scope = nameLookup.value(match.captured(2).toLower(), InvalidScope);
				if(scope == InvalidScope || sequence.contains(scope))
					break;
				// get the amount of days
				bool ok = false;
				int numDays;
				if(loopCombo.second && match.capturedLength(1) == 0) {
					ok = true;
					numDays = 1;
				} else
					numDays = match.captured(1).toInt(&ok);
				if(!ok)
					break;
				// add to sequence
				sequence.insert(scope, numDays);

				// check how to continue
				if(match.capturedLength(3) > 0) {//has found conjunction
					offset += match.capturedLength(0);
					// continue in loop
				} else {
					return {
						QSharedPointer<SequenceTerm>::create(std::move(sequence), loopCombo.second),
						offset + match.capturedLength(0)
					};
				}
			} else
				break;
		};
	}

	return {};
}

void SequenceTerm::apply(QDateTime &datetime, bool applyFenced) const
{
	using namespace std::chrono;
	for(auto it = _sequence.constBegin(); it != _sequence.constEnd(); ++it) {
		const auto delta = applyFenced ? (*it - 1) : *it;
		switch(it.key()) {
		case Minute:
			datetime = datetime.addSecs(duration_cast<seconds>(minutes{*it}).count());
			break;
		case Hour:
			datetime = datetime.addSecs(duration_cast<seconds>(hours{*it}).count());
			break;
		case Day:
			datetime = datetime.addDays(delta);
			break;
		case Week:
			datetime = datetime.addDays(static_cast<qint64>(delta) * 7ll);
			break;
		case Month:
			datetime = datetime.addMonths(delta);
			break;
		case Year:
			datetime = datetime.addYears(delta);
			break;
		default:
			Q_UNREACHABLE();
			break;
		}
	}
}

QString SequenceTerm::describe() const
{
	QStringList subTerms;
	for(auto it = _sequence.constBegin(); it != _sequence.constEnd(); ++it) {
		switch(it.key()) {
		case Minute:
			subTerms.append(tr("%n minute(s)", "", *it));
			break;
		case Hour:
			subTerms.append(tr("%n hour(s)", "", *it));
			break;
		case Day:
			subTerms.append(tr("%n day(s)", "", *it));
			break;
		case Week:
			subTerms.append(tr("%n week(s)", "", *it));
			break;
		case Month:
			subTerms.append(tr("%n month(s)", "", *it));
			break;
		case Year:
			subTerms.append(tr("%n year(s)", "", *it));
			break;
		default:
			Q_UNREACHABLE();
			break;
		}
	}

	return tr("in %1").arg(subTerms.join(QStringLiteral(", ")));
}

std::pair<QString, QString> SequenceTerm::syntax(bool asLoop)
{
	QStringList prefix;
	const auto suffix = trList(SpanSuffix, false, false);
	if(asLoop)
		prefix = trList(SpanLoopPrefix, false, false);
	else
		prefix = trList(SpanPrefix, false, false);
	return {
		tr("span"),
		tr("%1 [<number>] {min[utes]|hours|days|weeks|months|years} %2")
				.arg(prefix.isEmpty() ?
						 QString{} :
						 QStringLiteral("[%1]").arg(prefix.join(QLatin1Char('|'))),
					 suffix.isEmpty() ?
						 QString{} :
						 QStringLiteral("[%1]").arg(suffix.join(QLatin1Char('|'))))
	};
}

QMap<QString, int> SequenceTerm::getSequence() const
{
	QMap<QString, int> res;
	const auto metaEnum = QMetaEnum::fromType<Scope>();
	for(auto it = _sequence.constBegin(); it != _sequence.constEnd(); ++it)
		res.insert(QString::fromUtf8(metaEnum.valueToKey(it.key())), it.value());
	return res;
}

void SequenceTerm::setSequence(const QMap<QString, int> &sequence)
{
	_sequence.clear();
	const auto metaEnum = QMetaEnum::fromType<Scope>();
	for(auto it = sequence.constBegin(); it != sequence.constEnd(); ++it) {
		_sequence.insert(static_cast<ScopeFlag>(metaEnum.keyToValue(it.key().toUtf8().constData())),
						 it.value());
	}
}



KeywordTerm::KeywordTerm(int days) :
	SubTerm{Timespan, Day},
	_days{days}
{}

KeywordTerm::KeywordTerm(QObject *parent) :
	SubTerm{parent}
{}

std::pair<QSharedPointer<KeywordTerm>, int> KeywordTerm::parse(const QStringRef &expression)
{
	for(const auto &info : trList(KeywordDayspan, false)) {
		const auto split = info.split(QLatin1Char(':'));
		Q_ASSERT_X(split.size() == 2, Q_FUNC_INFO, "Invalid KeywordDayspan translation. Must be keyword and value, seperated by a ':'");
		QRegularExpression regex {
			QLatin1Char('^') + QRegularExpression::escape(split[0]) + QStringLiteral("\\s*"),
			QRegularExpression::DontAutomaticallyOptimizeOption |
			QRegularExpression::CaseInsensitiveOption |
			QRegularExpression::UseUnicodePropertiesOption
		};
		auto match = regex.match(expression);
		if(match.hasMatch()) {
			return {
				QSharedPointer<KeywordTerm>::create(split[1].toInt()),
				match.capturedLength(0)
			};
		}
	}

	return {};
}

void KeywordTerm::apply(QDateTime &datetime, bool applyFenced) const
{
	Q_UNUSED(applyFenced)
	datetime = datetime.addDays(_days);
}

QString KeywordTerm::describe() const
{
	return tr("in %n day(s)", "", _days);
}

std::pair<QString, QString> KeywordTerm::syntax(bool asLoop)
{
	if(asLoop)
		return {};
	else {
		return {
			tr("keyword"),
			tr("{today|tomorrow}")
		};
	}
}



LimiterTerm::LimiterTerm(bool isFrom) :
	SubTerm{isFrom ? FromSubterm : UntilSubTerm, InvalidScope}
{}

LimiterTerm::LimiterTerm(QObject *parent) :
	SubTerm{parent}
{}

LimiterTerm::LimiterTerm(SubTerm::Type type, Term &&limitTerm) :
	SubTerm{type, InvalidScope},
	_limitTerm{std::move(limitTerm)}
{}

std::pair<QSharedPointer<LimiterTerm>, int> LimiterTerm::parse(const QStringRef &expression)
{
	for(const auto &type : {std::make_pair(LimiterFromPrefix, true), std::make_pair(LimiterUntilPrefix, false)}) {
		QRegularExpression regex {
			QLatin1Char('^') + QStringLiteral("(?:%1)").arg(trList(type.first).join(QLatin1Char('|'))) + QStringLiteral("\\s*"),
			QRegularExpression::DontAutomaticallyOptimizeOption |
			QRegularExpression::CaseInsensitiveOption |
			QRegularExpression::UseUnicodePropertiesOption
		};
		auto match = regex.match(expression);
		if(match.hasMatch()) {
			return {
				QSharedPointer<LimiterTerm>::create(type.second),
				match.capturedLength(0)
			};
		}
	}

	return {};
}

void LimiterTerm::apply(QDateTime &datetime, bool applyFenced) const
{
	Q_UNUSED(applyFenced)
	Q_UNUSED(datetime)
}

QString LimiterTerm::describe() const
{
	return {};
}

Term LimiterTerm::limitTerm() const
{
	return _limitTerm;
}

QSharedPointer<LimiterTerm> LimiterTerm::clone(Term limitTerm) const
{
	return QSharedPointer<LimiterTerm>{new LimiterTerm{type, std::move(limitTerm)}};
}




QString Expressions::trWord(WordKey key, bool escape)
{
	QString word;
	switch(key) {
	case TimePrefix:
		word = EventExpressionParser::tr("at ", "TimePrefix");
		break;
	case TimeSuffix:
		word = EventExpressionParser::tr(" o'clock", "TimeSuffix");
		break;
	case TimePattern:
		word = EventExpressionParser::tr("hh:mm ap|h:mm ap|hh:m ap|h:m ap|hh ap|h ap|"
										 "hh:mm AP|h:mm AP|hh:m AP|h:m AP|hh AP|h AP|"
										 "hh:mm|h:mm|hh:m|h:m|hh|h", "TimePattern");
		break;
	case DatePrefix:
		word = EventExpressionParser::tr("on |on the |the ", "DatePrefix");
		break;
	case DateSuffix:
		word = EventExpressionParser::tr("###empty###", "DateSuffix");
		break;
	case DateLoopPrefix:
		word = EventExpressionParser::tr("every |any |all |on every |on any |on all ", "DateLoopPrefix");
		break;
	case DateLoopSuffix:
		word = EventExpressionParser::tr("###empty###", "DateLoopSuffix");
		break;
	case DatePattern:
		word= EventExpressionParser::tr("dd.MM.yyyy|d.MM.yyyy|dd.M.yyyy|d.M.yyyy|"
										"dd. MM. yyyy|d. MM. yyyy|dd. M. yyyy|d. M. yyyy|"
										"dd-MM-yyyy|d-MM-yyyy|dd-M-yyyy|d-M-yyyy|"

										"dd.MM.yy|d.MM.yy|dd.M.yy|d.M.yy|"
										"dd. MM. yy|d. MM. yy|dd. M. yy|d. M. yy|"
										"dd-MM-yy|d-MM-yy|dd-M-yy|d-M-yy|"

										"dd.MM.|d.MM.|dd.M.|d.M.|"
										"dd. MM.|d. MM.|dd. M.|d. M.|"
										"dd-MM|d-MM|dd-M|d-M", "DatePattern");
		break;
	case InvTimeExprPattern:
		word = EventExpressionParser::tr("%2 past %1:+|%2-past %1:+|%2 to %1:-", "InvTimeExprPattern");
		break;
	case Expressions::InvTimeHourPattern:
		word = EventExpressionParser::tr("hh ap|h ap|hh AP|h AP|hh|h", "InvTimeHourPattern");
		break;
	case Expressions::InvTimeMinutePattern:
		word = EventExpressionParser::tr("mm|m", "InvTimeMinutePattern");
		break;
	case InvTimeKeyword:
		word = EventExpressionParser::tr("quarter:15|half:30", "InvTimeKeywords");
		break;
	case MonthDayPrefix:
		word = EventExpressionParser::tr("on |on the |the |next |on next |on the next ", "MonthDayPrefix");
		break;
	case MonthDaySuffix:
		word = EventExpressionParser::tr(" of", "MonthDaySuffix");
		break;
	case MonthDayLoopPrefix:
		word = EventExpressionParser::tr("every |any |all |on every |on any |on all ", "MonthDayLoopPrefix");
		break;
	case MonthDayLoopSuffix:
		word = EventExpressionParser::tr("###empty###", "MonthDayLoopSuffix");
		break;
	case MonthDayIndicator:
		word = EventExpressionParser::tr("_.|_th|_st|_nd|_rd", "MonthDayIndicator");
		break;
	case WeekDayPrefix:
		word = EventExpressionParser::tr("on |next |on next |on the next ", "WeekDayPrefix");
		break;
	case WeekDaySuffix:
		word = EventExpressionParser::tr("###empty###", "WeekDaySuffix");
		break;
	case WeekDayLoopPrefix:
		word = EventExpressionParser::tr("every |any |all |on every |on any |on all ", "WeekDayLoopPrefix");
		break;
	case WeekDayLoopSuffix:
		word = EventExpressionParser::tr("###empty###", "WeekDayLoopSuffix");
		break;
	case MonthPrefix:
		word = EventExpressionParser::tr("in |on |next |on next |on the next |in next |in the next ", "MonthPrefix");
		break;
	case MonthSuffix:
		word = EventExpressionParser::tr("###empty###", "MonthSuffix");
		break;
	case MonthLoopPrefix:
		word = EventExpressionParser::tr("every |any |all |on every |on any |on all ", "MonthLoopPrefix");
		break;
	case MonthLoopSuffix:
		word = EventExpressionParser::tr("###empty###", "MonthLoopSuffix");
		break;
	case YearPrefix:
		word = EventExpressionParser::tr("in ", "YearPrefix");
		break;
	case YearSuffix:
		word = EventExpressionParser::tr("###empty###", "YearSuffix");
		break;
	case SpanPrefix:
		word = EventExpressionParser::tr("in ", "SpanPrefix");
		break;
	case SpanSuffix:
		word = EventExpressionParser::tr("###empty###", "SpanSuffix");
		break;
	case SpanLoopPrefix:
		word = EventExpressionParser::tr("every |all ", "SpanLoopPrefix");
		break;
	case SpanConjuction:
		word = EventExpressionParser::tr(" and", "SpanConjuction");
		break;
	case SpanKeyMinute:
		word = EventExpressionParser::tr("min|mins|minute|minutes", "SpanKeyMinute");
		break;
	case SpanKeyHour:
		word = EventExpressionParser::tr("hour|hours", "SpanKeyHour");
		break;
	case SpanKeyDay:
		word = EventExpressionParser::tr("day|days", "SpanKeyDay");
		break;
	case SpanKeyWeek:
		word = EventExpressionParser::tr("week|weeks", "SpanKeyWeek");
		break;
	case SpanKeyMonth:
		word = EventExpressionParser::tr("mon|mons|month|months", "SpanKeyMonth");
		break;
	case SpanKeyYear:
		word = EventExpressionParser::tr("year|years", "SpanKeyYear");
		break;
	case KeywordDayspan:
		word = EventExpressionParser::tr("today:0|tomorrow:1", "KeywordDayspan");
		break;
	case LimiterFromPrefix:
		word = EventExpressionParser::tr("from", "LimiterFromPrefix");
		break;
	case LimiterUntilPrefix:
		word = EventExpressionParser::tr("until|to", "LimiterUntilPrefix");
		break;
	case ExpressionSeperator:
		word = EventExpressionParser::tr(";", "ExpressionSeperator");
		break;
	}
	if(word == QStringLiteral("###empty###"))
		return {};
	if(escape)
		word = QRegularExpression::escape(word);
	return word;
}

QStringList Expressions::trList(WordKey key, bool escape, bool sort)
{
	auto resList = trWord(key, false).split(QLatin1Char('|'), QString::SkipEmptyParts);
	if(sort) {
		std::sort(resList.begin(), resList.end(), [](const QString &lhs, const QString &rhs) {
			return lhs.size() > rhs.size();
		});
	}
	if(escape) {
		for(auto &word : resList)
			word = QRegularExpression::escape(word);
	}
	return resList;
}

QString Expressions::dateTimeFormatToRegex(QString pattern, const std::function<void (QString &)> &replacer)
{
	Q_ASSERT(replacer);
	pattern = QRegularExpression::escape(pattern);
	auto afterQuote = 0;
	bool inQuote = false;
	forever {
		const auto nextQuote = pattern.indexOf(QStringLiteral("\\'"), afterQuote); // \' because already regex escaped
		auto replLen = nextQuote == -1 ?
						   pattern.size() - afterQuote :
						   nextQuote - afterQuote;

		if(!inQuote) {
			//special case - quote after quote
			if(nextQuote == afterQuote) {
				afterQuote += 2;
				inQuote = true;
				continue;
			}

			if(replLen > 0) {
				auto subStr = pattern.mid(afterQuote, replLen);
				replacer(subStr);
				pattern.replace(afterQuote, replLen, subStr);
				if(nextQuote != -1) {
					afterQuote += subStr.size() + 2;
					Q_ASSERT(pattern.midRef(afterQuote - 2, 2) == QStringLiteral("\\'"));
				}
			}
		} else if(nextQuote != -1) {
			pattern.replace(afterQuote - 2, replLen + 4, pattern.mid(afterQuote, replLen));
			afterQuote = nextQuote - 2;
		}

		if(nextQuote < 0)
			break;
		inQuote = !inQuote;
	};

	return pattern;
}
