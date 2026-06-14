#pragma once
// avrdude_backend.h
//
// Thin C++ wrapper around libavrdude's C API.  Centralises every direct
// call into libavrdude so the rest of the GUI never needs to touch
// <libavrdude.h> directly.
//
// Reference: src/main.c in https://github.com/avrdudes/avrdude (≥ v8.0)
// is the authoritative example of how the CLI uses these same functions.
//
// If your installed libavrdude.h differs slightly (function names/argument
// order do shift occasionally between releases), adjust the .cpp file —
// the public interface here (AvrdudeBackend) should not need to change.

#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>
#include <cstdint>

// Forward declarations to avoid leaking libavrdude C types into Qt headers
struct programmer_t;
struct avrpart_t;
struct LISTID_t;

struct PartInfo {
    QString id;          // e.g. "m328p"
    QString desc;        // e.g. "ATmega328P"
};

struct ProgrammerInfo {
    QString id;          // e.g. "arduino"
    QString desc;        // e.g. "Arduino"
    QString type;        // e.g. "arduino" (programmer_types[] key)
};

struct MemoryInfo {
    QString name;        // "flash", "eeprom", "lfuse", …
    int     size{0};
    int     pageSize{0};
    int     numPages{0};
};

struct DeviceDetails {
    QString     desc;
    int         flashSize{0};
    int         eepromSize{0};
    int         sramSize{0};
    QString     signatureExpected;   // "1E 95 0F"
    QVector<MemoryInfo> memories;
};

// Log levels match libavrdude's MSG_* constants (MSG_INFO=0 .. MSG_TRACE2)
// but we re-declare here so callers don't need libavrdude.h.
enum class LogLevel { Error = 0, Warning = 1, Info = 2, Notice = 3, Debug = 4, Trace = 5 };

using LogCallback      = std::function<void(LogLevel, const QString &)>;
using ProgressCallback = std::function<void(const QString &task, int percent)>;

// ----------------------------------------------------------------------------
class AvrdudeBackend
{
public:
    static AvrdudeBackend &instance();

    // Must be called once before anything else.  Loads avrdude.conf (system
    // config, optionally followed by a user config) and populates the part
    // and programmer lists.  Returns false on failure (check lastError()).
    bool loadConfig(const QString &systemConfigPath = {},
                     const QString &userConfigPath  = {});

    bool configLoaded() const { return m_configLoaded; }

    // -- Enumeration ----------------------------------------------------------
    QVector<PartInfo>       availableParts() const;
    QVector<ProgrammerInfo> availableProgrammers() const;
    DeviceDetails            partDetails(const QString &partId) const;

    // -- Connection -------------------------------------------------------------
    // partId / programmerId are the short ids from availableParts() /
    // availableProgrammers(), e.g. "m328p" / "arduino".
    bool connectDevice(const QString &programmerId,
                        const QString &partId,
                        const QString &port,
                        int baud = 0);
    void disconnectDevice();
    bool isConnected() const { return m_connected; }

    // -- Operations (call only while connected) --------------------------------
    // All return false + set lastError() on failure.
    bool readSignature(QString &signatureHexOut);

    bool readMemory (const QString &memName, const QString &filePath, const QString &format = "ihex");
    bool writeMemory(const QString &memName, const QString &filePath, const QString &format = "ihex");
    bool verifyMemory(const QString &memName, const QString &filePath, const QString &format = "ihex");

    // Fuses are read/written as raw bytes (lfuse/hfuse/efuse memories)
    bool readFuse (const QString &fuseName, uint8_t &valueOut);
    bool writeFuse(const QString &fuseName, uint8_t value);

    QString lastError() const { return m_lastError; }

    // -- Callbacks --------------------------------------------------------------
    void setLogCallback(LogCallback cb)           { m_logCb = std::move(cb); }
    void setProgressCallback(ProgressCallback cb) { m_progressCb = std::move(cb); }

    // Internal: used by the static libavrdude callback trampolines in the
    // .cpp file to forward into the registered Qt callbacks. Not intended
    // for use outside avrdude_backend.cpp.
    void forwardLibLog(LogLevel lvl, const QString &msg);
    void forwardLibProgress(const QString &task, int pct);
    LogCallback lastLogCallbackCopy() const;

private:
    AvrdudeBackend();
    ~AvrdudeBackend();
    AvrdudeBackend(const AvrdudeBackend &) = delete;
    AvrdudeBackend &operator=(const AvrdudeBackend &) = delete;

    void emitLog(LogLevel lvl, const QString &msg);
    void emitProgress(const QString &task, int pct);

    // Opaque pointers to libavrdude structures; defined fully in the .cpp
    void *m_partList{nullptr};        // LISTID
    void *m_programmerList{nullptr};  // LISTID
    void *m_pgm{nullptr};             // PROGRAMMER *
    void *m_part{nullptr};            // AVRPART *

    bool    m_configLoaded{false};
    bool    m_connected{false};
    QString m_lastError;

    LogCallback      m_logCb;
    ProgressCallback m_progressCb;
};
