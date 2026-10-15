#pragma once

#include <QString>

// Parsers for the machine-readable output of the privileged helper (APT
// status-fd lines plus "@@" markers) and of the Flatpak CLI in non-tty mode.
// They are pure functions so the progress logic can be tested without running
// a package manager.
namespace InstallProgress {

struct HelperEvent
{
    enum class Type {
        Unknown,
        Phase,        // @@phase <name>
        Packages,     // @@packages <pkg> <pkg>…
        Download,     // dlstatus:<n>:<percent>:<message>
        Package,      // pmstatus:<package>:<percent>:<message>
        PackageError, // pmerror:<package>:<percent>:<message>
        Error,        // @@error <kind>
        Warning,      // @@warning <kind>
        Done          // @@done
    };

    Type type = Type::Unknown;
    QString name;        // phase, error kind, warning kind or package
    QString text;        // status message or package list
    double percent = -1; // -1 when the line carries no percentage
    bool configuring = false;
};

HelperEvent parseHelperLine(const QString &line);

struct FlatpakProgress
{
    bool changed = false;
    int operation = 0;  // 1-based operation index, 0 when unknown
    int operations = 0; // total operations, 0 when unknown
    int percent = -1;   // progress of the current operation
};

// Updates `state` with the information in one stdout line of
// `flatpak install` (non-tty output), e.g. "Installing 2/3… ███▌ 45%  1.2 MB/s".
FlatpakProgress parseFlatpakLine(const QString &line, FlatpakProgress state);

// "Retrieving file 3 of 29" → {3, 29}; returns false when the text differs.
bool parseAptFileCounter(const QString &text, int *current, int *total);

} // namespace InstallProgress
