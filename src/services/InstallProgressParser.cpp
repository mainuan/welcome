#include "InstallProgressParser.h"

#include <QRegularExpression>
#include <QStringList>

namespace InstallProgress {

namespace {

double clampPercent(double value)
{
    if (value < 0) {
        return 0;
    }
    return value > 100 ? 100 : value;
}

// APT status lines are "<kind>:<field>:<percent>:<message>"; the message may
// itself contain colons, so only the first three separators are significant.
bool splitStatusLine(const QString &line, QString *field, double *percent, QString *message)
{
    const qsizetype first = line.indexOf(QLatin1Char(':'));
    const qsizetype second = first < 0 ? -1 : line.indexOf(QLatin1Char(':'), first + 1);
    const qsizetype third = second < 0 ? -1 : line.indexOf(QLatin1Char(':'), second + 1);
    if (third < 0) {
        return false;
    }
    bool ok = false;
    const double value = line.mid(second + 1, third - second - 1).toDouble(&ok);
    if (!ok) {
        return false;
    }
    *field = line.mid(first + 1, second - first - 1);
    *percent = clampPercent(value);
    *message = line.mid(third + 1).trimmed();
    return true;
}

} // namespace

HelperEvent parseHelperLine(const QString &rawLine)
{
    HelperEvent event;
    const QString line = rawLine.trimmed();

    if (line.startsWith(QStringLiteral("@@"))) {
        const QString body = line.mid(2);
        const qsizetype space = body.indexOf(QLatin1Char(' '));
        const QString keyword = space < 0 ? body : body.left(space);
        const QString argument = space < 0 ? QString() : body.mid(space + 1).trimmed();
        if (keyword == QStringLiteral("phase") && !argument.isEmpty()) {
            event.type = HelperEvent::Type::Phase;
            event.name = argument;
        } else if (keyword == QStringLiteral("packages")) {
            event.type = HelperEvent::Type::Packages;
            event.text = argument;
        } else if (keyword == QStringLiteral("error")) {
            event.type = HelperEvent::Type::Error;
            event.name = argument.isEmpty() ? QStringLiteral("failed") : argument;
        } else if (keyword == QStringLiteral("warning") && !argument.isEmpty()) {
            event.type = HelperEvent::Type::Warning;
            event.name = argument;
        } else if (keyword == QStringLiteral("done")) {
            event.type = HelperEvent::Type::Done;
        }
        return event;
    }

    QString field;
    QString message;
    double percent = -1;
    if (line.startsWith(QStringLiteral("dlstatus:"))) {
        if (splitStatusLine(line, &field, &percent, &message)) {
            event.type = HelperEvent::Type::Download;
            event.percent = percent;
            event.text = message;
        }
    } else if (line.startsWith(QStringLiteral("pmstatus:"))) {
        if (splitStatusLine(line, &field, &percent, &message)) {
            event.type = HelperEvent::Type::Package;
            event.name = field;
            event.percent = percent;
            event.text = message;
            event.configuring = message.startsWith(QStringLiteral("Preparing to configure"))
                || message.startsWith(QStringLiteral("Configuring"))
                || message.startsWith(QStringLiteral("Installed"))
                || message.startsWith(QStringLiteral("Running post-installation trigger"));
        }
    } else if (line.startsWith(QStringLiteral("pmerror:"))) {
        if (splitStatusLine(line, &field, &percent, &message)) {
            event.type = HelperEvent::Type::PackageError;
            event.name = field;
            event.percent = percent;
            event.text = message;
        }
    }
    return event;
}

FlatpakProgress parseFlatpakLine(const QString &line, FlatpakProgress state)
{
    static const QRegularExpression operationPattern(QStringLiteral("(?:^|\\s)(\\d+)/(\\d+)(?:…|\\.\\.\\.|\\s|$)"));
    static const QRegularExpression percentPattern(QStringLiteral("(\\d{1,3})%"));

    state.changed = false;
    const QRegularExpressionMatch operation = operationPattern.match(line);
    const QRegularExpressionMatch percent = percentPattern.match(line);

    if (operation.hasMatch()) {
        const int current = operation.captured(1).toInt();
        const int total = operation.captured(2).toInt();
        if (current > 0 && total >= current && total < 1000) {
            if (current != state.operation) {
                state.percent = -1;
            }
            state.operation = current;
            state.operations = total;
            state.changed = true;
        }
    } else if (line.trimmed().endsWith(QStringLiteral("…")) && !percent.hasMatch() && state.operation == 0) {
        // A single-operation transaction prints "Installing…" without a counter.
        state.operation = 1;
        state.operations = 1;
        state.changed = true;
    }

    if (percent.hasMatch() && (operation.hasMatch() || state.operation > 0)) {
        const int value = percent.captured(1).toInt();
        if (value >= 0 && value <= 100) {
            if (state.operation == 0) {
                state.operation = 1;
                state.operations = 1;
            }
            state.percent = value;
            state.changed = true;
        }
    }
    return state;
}

bool parseAptFileCounter(const QString &text, int *current, int *total)
{
    static const QRegularExpression pattern(QStringLiteral("(\\d+)\\D+(\\d+)\\s*$"));
    if (!text.startsWith(QStringLiteral("Retrieving file"))) {
        return false;
    }
    const QRegularExpressionMatch match = pattern.match(text);
    if (!match.hasMatch()) {
        return false;
    }
    *current = match.captured(1).toInt();
    *total = match.captured(2).toInt();
    return *total > 0 && *current > 0 && *current <= *total;
}

} // namespace InstallProgress
