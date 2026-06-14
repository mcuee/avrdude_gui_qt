// avrdude_backend.cpp
//
// Real implementation of AvrdudeBackend using libavrdude's C API.
//
// Build with -DHAVE_LIBAVRDUDE (set automatically by CMakeLists.txt when
// pkg-config finds libavrdude).  Without that macro, every call returns a
// "not built with libavrdude support" error so the GUI still runs for
// layout/testing purposes.
//
// NOTE on API stability: libavrdude's header occasionally renames or
// reorders arguments between releases (it explicitly disclaims a stable
// ABI). The calls below match the API as of AVRDUDE 8.x. If your
// `avrdude.h` / `libavrdude.h` differ, the compiler errors will point at
// the exact lines to adjust — the public AvrdudeBackend interface in the
// header does not need to change.

#include "avrdude_backend.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <cstring>
#include <cstdarg>
#include <cstdio>

#ifdef HAVE_LIBAVRDUDE
extern "C" {
#include <ac_cfg.h>
#include <libavrdude.h>
}
#endif

// ============================================================================
// Global state required by libavrdude
// ============================================================================
#ifdef HAVE_LIBAVRDUDE
// avrdude's library code references a handful of globals that are normally
// defined in src/main.c of the CLI.  We provide minimal definitions here.
extern "C" {
    // progname is used in many libavrdude error messages
    const char *progname = "adgui";
    // verbosity controls how chatty libavrdude's own messages are;
    // we keep it high and filter in the GUI instead.
    int verbose = 2;
    int ovsigck = 0;     // don't override signature-mismatch checks by default
}
#endif

// ----------------------------------------------------------------------------
// Singleton
// ----------------------------------------------------------------------------
AvrdudeBackend &AvrdudeBackend::instance()
{
    static AvrdudeBackend inst;
    return inst;
}

AvrdudeBackend::AvrdudeBackend()  = default;

AvrdudeBackend::~AvrdudeBackend()
{
#ifdef HAVE_LIBAVRDUDE
    if (m_connected) disconnectDevice();
#endif
}

void AvrdudeBackend::emitLog(LogLevel lvl, const QString &msg)
{
    if (m_logCb) m_logCb(lvl, msg);
}

void AvrdudeBackend::emitProgress(const QString &task, int pct)
{
    if (m_progressCb) m_progressCb(task, pct);
}

// ============================================================================
// Message callback bridge: libavrdude -> AvrdudeBackend -> Qt signal
// ============================================================================
#ifdef HAVE_LIBAVRDUDE
// libavrdude >= 7 routes all output through avrdude_message2(), which in
// turn can be redirected via msg_callback (see lists.c / avrdude.h).
// The exact signature is:
//   typedef int (*msg_callback_t)(int level, int fmtflags, const char *fmt, va_list ap);
// We register a static trampoline that forwards into the active backend
// instance (there is only ever one).
static int avrdude_msg_trampoline(int level, int /*fmtflags*/, const char *fmt, va_list ap)
{
    char buf[2048];
    vsnprintf(buf, sizeof(buf), fmt, ap);

    LogLevel lvl = LogLevel::Info;
    switch (level) {
        case MSG_EXT_ERROR:
        case MSG_ERROR:   lvl = LogLevel::Error;   break;
        case MSG_WARNING: lvl = LogLevel::Warning; break;
        case MSG_INFO:    lvl = LogLevel::Info;    break;
        case MSG_NOTICE:
        case MSG_NOTICE2: lvl = LogLevel::Notice;  break;
        case MSG_DEBUG:   lvl = LogLevel::Debug;   break;
        default:          lvl = LogLevel::Trace;   break;
    }

    QString text = QString::fromLocal8Bit(buf).trimmed();
    AvrdudeBackend::instance().forwardLibLog(lvl, text);
    return static_cast<int>(strlen(buf));
}

// progress callback: report_progress(int percent, double etime, const char *hdr, int finish)
static void avrdude_progress_trampoline(int percent, double /*etime*/, const char *hdr, int /*finish*/)
{
    AvrdudeBackend::instance().forwardLibProgress(hdr ? QString::fromLocal8Bit(hdr) : QString(), percent);
}
#endif // HAVE_LIBAVRDUDE

// Small helper methods used by the trampolines above; declared here (not in
// the public header) via friend-free approach: implemented as regular
// public-ish members reached through the singleton. We add tiny inline
// helpers directly:
void AvrdudeBackend::forwardLibLog(LogLevel lvl, const QString &msg) { emitLog(lvl, msg); }
void AvrdudeBackend::forwardLibProgress(const QString &task, int pct) { emitProgress(task, pct); }
LogCallback AvrdudeBackend::lastLogCallbackCopy() const { return m_logCb; }

// ============================================================================
// loadConfig()
// ============================================================================
bool AvrdudeBackend::loadConfig(const QString &systemConfigPath, const QString &userConfigPath)
{
#ifndef HAVE_LIBAVRDUDE
    m_lastError = "Built without libavrdude support (HAVE_LIBAVRDUDE not defined)";
    emitLog(LogLevel::Error, m_lastError);
    return false;
#else
    // init_config() allocates the global part_list / programmers LISTIDs
    init_config();

    // Register our message + progress callbacks as early as possible so
    // even config-parsing diagnostics reach the GUI log.
    avrdude_message2_set_callback(avrdude_msg_trampoline);
    update_progress = avrdude_progress_trampoline;

    QString sysConf = systemConfigPath;
    if (sysConf.isEmpty()) {
        // Typical install locations
        const QStringList candidates = {
            "/etc/avrdude.conf",
            "/usr/local/etc/avrdude.conf",
            "/usr/share/avrdude/avrdude.conf",
            QCoreApplication::applicationDirPath() + "/avrdude.conf",
        };
        for (const auto &c : candidates) {
            if (QFileInfo::exists(c)) { sysConf = c; break; }
        }
    }

    if (sysConf.isEmpty()) {
        m_lastError = "Could not locate avrdude.conf — pass a path explicitly";
        emitLog(LogLevel::Error, m_lastError);
        return false;
    }

    // read_config() parses the file and appends to the global part_list /
    // programmers lists.  It returns -1 on failure.
    if (read_config(sysConf.toLocal8Bit().constData()) != 0) {
        m_lastError = QString("Failed to parse %1").arg(sysConf);
        emitLog(LogLevel::Error, m_lastError);
        return false;
    }
    emitLog(LogLevel::Notice, QString("Loaded config: %1").arg(sysConf));

    if (!userConfigPath.isEmpty() && QFileInfo::exists(userConfigPath)) {
        if (read_config(userConfigPath.toLocal8Bit().constData()) != 0) {
            emitLog(LogLevel::Warning,
                    QString("Failed to parse user config %1 (ignored)").arg(userConfigPath));
        } else {
            emitLog(LogLevel::Notice, QString("Loaded user config: %1").arg(userConfigPath));
        }
    }

    // Stash the global lists (declared in libavrdude.h) as opaque pointers.
    m_partList       = &part_list;
    m_programmerList = &programmers;

    m_configLoaded = true;
    return true;
#endif
}

// ============================================================================
// availableParts() / availableProgrammers()
// ============================================================================
QVector<PartInfo> AvrdudeBackend::availableParts() const
{
    QVector<PartInfo> result;
#ifdef HAVE_LIBAVRDUDE
    if (!m_configLoaded) return result;

    LISTID parts = part_list;
    AVRPART *p;
    for (LNODEID ln = lfirst(parts); ln; ln = lnext(ln)) {
        p = static_cast<AVRPART *>(ldata(ln));
        PartInfo info;
        info.id   = QString::fromLocal8Bit(p->id);
        info.desc = QString::fromLocal8Bit(p->desc);
        result.push_back(info);
    }
#endif
    return result;
}

QVector<ProgrammerInfo> AvrdudeBackend::availableProgrammers() const
{
    QVector<ProgrammerInfo> result;
#ifdef HAVE_LIBAVRDUDE
    if (!m_configLoaded) return result;

    LISTID pgms = programmers;
    PROGRAMMER *pgm;
    for (LNODEID ln = lfirst(pgms); ln; ln = lnext(ln)) {
        pgm = static_cast<PROGRAMMER *>(ldata(ln));

        // pgm->id is itself a LISTID of strings (a programmer can have
        // multiple aliases); take the first one as the canonical id.
        QString id;
        if (pgm->id && lfirst(pgm->id))
            id = QString::fromLocal8Bit(static_cast<const char *>(ldata(lfirst(pgm->id))));
        if (id.isEmpty())
            continue;

        ProgrammerInfo info;
        info.id   = id;
        info.desc = QString::fromLocal8Bit(pgm->desc);
        info.type = QString::fromLocal8Bit(pgm->type);
        result.push_back(info);
    }
#endif
    return result;
}

DeviceDetails AvrdudeBackend::partDetails(const QString &partId) const
{
    DeviceDetails d;
#ifdef HAVE_LIBAVRDUDE
    if (!m_configLoaded) return d;

    AVRPART *p = locate_part(part_list, partId.toLocal8Bit().constData());
    if (!p) return d;

    d.desc       = QString::fromLocal8Bit(p->desc);
    d.flashSize  = 0;
    d.eepromSize = 0;
    d.sramSize   = p->n_interrupts >= 0 ? 0 : 0; // sram not directly in AVRPART; see mem list

    char sigbuf[16] = {0};
    snprintf(sigbuf, sizeof(sigbuf), "%02X %02X %02X",
             p->signature[0], p->signature[1], p->signature[2]);
    d.signatureExpected = QString::fromLocal8Bit(sigbuf);

    for (LNODEID ln = lfirst(p->mem); ln; ln = lnext(ln)) {
        AVRMEM *m = static_cast<AVRMEM *>(ldata(ln));
        MemoryInfo mi;
        mi.name     = QString::fromLocal8Bit(m->desc);
        mi.size     = m->size;
        mi.pageSize = m->page_size;
        mi.numPages = m->num_pages;
        d.memories.push_back(mi);

        if (mi.name == "flash")  d.flashSize  = m->size;
        if (mi.name == "eeprom") d.eepromSize = m->size;
    }
#else
    Q_UNUSED(partId);
#endif
    return d;
}

// ============================================================================
// connectDevice() / disconnectDevice()
// ============================================================================
bool AvrdudeBackend::connectDevice(const QString &programmerId, const QString &partId,
                                    const QString &port, int baud)
{
#ifndef HAVE_LIBAVRDUDE
    Q_UNUSED(programmerId); Q_UNUSED(partId); Q_UNUSED(port); Q_UNUSED(baud);
    m_lastError = "Built without libavrdude support";
    emitLog(LogLevel::Error, m_lastError);
    return false;
#else
    if (!m_configLoaded) {
        m_lastError = "Config not loaded";
        emitLog(LogLevel::Error, m_lastError);
        return false;
    }
    if (m_connected) disconnectDevice();

    PROGRAMMER *pgm = locate_programmer(programmers, programmerId.toLocal8Bit().constData());
    if (!pgm) {
        m_lastError = QString("Unknown programmer: %1").arg(programmerId);
        emitLog(LogLevel::Error, m_lastError);
        return false;
    }
    // pgm_dup() so we don't mutate the shared config-list entry
    pgm = pgm_dup(pgm);

    AVRPART *p = locate_part(part_list, partId.toLocal8Bit().constData());
    if (!p) {
        m_lastError = QString("Unknown part: %1").arg(partId);
        emitLog(LogLevel::Error, m_lastError);
        pgm_free(pgm);
        return false;
    }
    p = avr_dup_part(p);

    // The programmer "type" function table must be (re)initialised
    if (pgm->initpgm) pgm->initpgm(pgm);

    strncpy(pgm->port, port.toLocal8Bit().constData(), PATH_MAX - 1);
    if (baud > 0) pgm->baudrate = baud;

    emitLog(LogLevel::Notice,
            QString("Opening %1 on %2 …").arg(programmerId, port));

    if (pgm->open(pgm, pgm->port) < 0) {
        m_lastError = QString("Could not open programmer %1 on %2")
                           .arg(programmerId, port);
        emitLog(LogLevel::Error, m_lastError);
        pgm_free(pgm);
        avr_free_part(p);
        return false;
    }

    pgm->enable(pgm, p);
    pgm->initialize(pgm, p);

    m_pgm  = pgm;
    m_part = p;
    m_connected = true;

    emitLog(LogLevel::Notice, "Connected.");
    return true;
#endif
}

void AvrdudeBackend::disconnectDevice()
{
#ifdef HAVE_LIBAVRDUDE
    if (!m_connected) return;

    auto *pgm = static_cast<PROGRAMMER *>(m_pgm);
    auto *p   = static_cast<AVRPART *>(m_part);

    if (pgm) {
        pgm->disable(pgm);
        pgm->powerdown(pgm);
        pgm->close(pgm);
        pgm_free(pgm);
    }
    if (p) avr_free_part(p);

    m_pgm = nullptr;
    m_part = nullptr;
    m_connected = false;
    emitLog(LogLevel::Notice, "Disconnected.");
#endif
}

// ============================================================================
// readSignature()
// ============================================================================
bool AvrdudeBackend::readSignature(QString &signatureHexOut)
{
#ifndef HAVE_LIBAVRDUDE
    m_lastError = "Built without libavrdude support";
    return false;
#else
    if (!m_connected) { m_lastError = "Not connected"; return false; }

    auto *pgm = static_cast<PROGRAMMER *>(m_pgm);
    auto *p   = static_cast<AVRPART *>(m_part);

    AVRMEM *sig = avr_locate_mem(p, "signature");
    if (!sig) { m_lastError = "Part has no signature memory"; return false; }

    emitProgress("Reading signature", 0);
    int rc = avr_read(pgm, p, sig, false);
    emitProgress("Reading signature", 100);
    if (rc < 0) {
        m_lastError = "Failed to read signature";
        return false;
    }

    char buf[16];
    snprintf(buf, sizeof(buf), "%02X %02X %02X",
             sig->buf[0], sig->buf[1], sig->buf[2]);
    signatureHexOut = QString::fromLocal8Bit(buf);
    return true;
#endif
}

// ============================================================================
// readMemory() / writeMemory() / verifyMemory()
// ============================================================================
bool AvrdudeBackend::readMemory(const QString &memName, const QString &filePath, const QString &format)
{
#ifndef HAVE_LIBAVRDUDE
    Q_UNUSED(memName); Q_UNUSED(filePath); Q_UNUSED(format);
    m_lastError = "Built without libavrdude support";
    return false;
#else
    if (!m_connected) { m_lastError = "Not connected"; return false; }
    auto *pgm = static_cast<PROGRAMMER *>(m_pgm);
    auto *p   = static_cast<AVRPART *>(m_part);

    AVRMEM *mem = avr_locate_mem(p, memName.toLocal8Bit().constData());
    if (!mem) { m_lastError = QString("No such memory: %1").arg(memName); return false; }

    emitLog(LogLevel::Notice, QString("Reading %1 (%2 bytes)…").arg(memName).arg(mem->size));
    emitProgress(QString("Reading %1").arg(memName), 0);

    int rc = avr_read(pgm, p, mem, false);
    if (rc < 0) { m_lastError = QString("Read of %1 failed").arg(memName); return false; }
    emitProgress(QString("Reading %1").arg(memName), 70);

    FILEFMT fmt = (format == "raw") ? FMT_RBIN
                : (format == "srec") ? FMT_SREC
                : FMT_IHEX;

    rc = fileio(FIO_WRITE, filePath.toLocal8Bit().constData(), fmt, p, mem, -1);
    emitProgress(QString("Reading %1").arg(memName), 100);
    if (rc < 0) { m_lastError = QString("Could not write %1").arg(filePath); return false; }

    emitLog(LogLevel::Notice, QString("Wrote %1 bytes to %2").arg(rc).arg(filePath));
    return true;
#endif
}

bool AvrdudeBackend::writeMemory(const QString &memName, const QString &filePath, const QString &format)
{
#ifndef HAVE_LIBAVRDUDE
    Q_UNUSED(memName); Q_UNUSED(filePath); Q_UNUSED(format);
    m_lastError = "Built without libavrdude support";
    return false;
#else
    if (!m_connected) { m_lastError = "Not connected"; return false; }
    auto *pgm = static_cast<PROGRAMMER *>(m_pgm);
    auto *p   = static_cast<AVRPART *>(m_part);

    AVRMEM *mem = avr_locate_mem(p, memName.toLocal8Bit().constData());
    if (!mem) { m_lastError = QString("No such memory: %1").arg(memName); return false; }

    FILEFMT fmt = (format == "raw") ? FMT_RBIN
                : (format == "srec") ? FMT_SREC
                : FMT_IHEX;

    emitLog(LogLevel::Notice, QString("Reading input file %1…").arg(filePath));
    int size = fileio(FIO_READ, filePath.toLocal8Bit().constData(), fmt, p, mem, -1);
    if (size < 0) { m_lastError = QString("Could not read %1").arg(filePath); return false; }

    emitLog(LogLevel::Notice, QString("Writing %1 (%2 bytes)…").arg(memName).arg(size));
    emitProgress(QString("Writing %1").arg(memName), 0);

    int rc = avr_write(pgm, p, mem, size, true /* auto-erase where supported */);
    emitProgress(QString("Writing %1").arg(memName), 100);
    if (rc < 0) { m_lastError = QString("Write of %1 failed").arg(memName); return false; }

    emitLog(LogLevel::Notice, QString("Wrote %1 bytes to %2").arg(rc).arg(memName));
    return true;
#endif
}

bool AvrdudeBackend::verifyMemory(const QString &memName, const QString &filePath, const QString &format)
{
#ifndef HAVE_LIBAVRDUDE
    Q_UNUSED(memName); Q_UNUSED(filePath); Q_UNUSED(format);
    m_lastError = "Built without libavrdude support";
    return false;
#else
    if (!m_connected) { m_lastError = "Not connected"; return false; }
    auto *pgm = static_cast<PROGRAMMER *>(m_pgm);
    auto *p   = static_cast<AVRPART *>(m_part);

    AVRMEM *mem = avr_locate_mem(p, memName.toLocal8Bit().constData());
    if (!mem) { m_lastError = QString("No such memory: %1").arg(memName); return false; }

    FILEFMT fmt = (format == "raw") ? FMT_RBIN
                : (format == "srec") ? FMT_SREC
                : FMT_IHEX;

    // Load the expected contents into a scratch copy of the memory
    AVRMEM *verifyMem = avr_dup_mem(mem);
    int size = fileio(FIO_READ, filePath.toLocal8Bit().constData(), fmt, p, verifyMem, -1);
    if (size < 0) {
        m_lastError = QString("Could not read %1").arg(filePath);
        avr_free_mem(verifyMem);
        return false;
    }

    emitLog(LogLevel::Notice, QString("Reading device %1 for verification…").arg(memName));
    emitProgress(QString("Verifying %1").arg(memName), 0);
    if (avr_read(pgm, p, mem, false) < 0) {
        m_lastError = QString("Read of %1 failed").arg(memName);
        avr_free_mem(verifyMem);
        return false;
    }
    emitProgress(QString("Verifying %1").arg(memName), 60);

    bool ok = true;
    for (int i = 0; i < size; ++i) {
        if (mem->buf[i] != verifyMem->buf[i]) {
            emitLog(LogLevel::Error,
                    QString("Verify mismatch at 0x%1: device=0x%2 file=0x%3")
                        .arg(i, 0, 16)
                        .arg(mem->buf[i], 2, 16, QChar('0'))
                        .arg(verifyMem->buf[i], 2, 16, QChar('0')));
            ok = false;
            break;
        }
    }
    emitProgress(QString("Verifying %1").arg(memName), 100);
    avr_free_mem(verifyMem);

    if (!ok) { m_lastError = QString("Verification of %1 failed").arg(memName); return false; }
    emitLog(LogLevel::Notice, QString("%1 verified OK (%2 bytes)").arg(memName).arg(size));
    return true;
#endif
}

// ============================================================================
// Fuses
// ============================================================================
bool AvrdudeBackend::readFuse(const QString &fuseName, uint8_t &valueOut)
{
#ifndef HAVE_LIBAVRDUDE
    Q_UNUSED(fuseName); Q_UNUSED(valueOut);
    m_lastError = "Built without libavrdude support";
    return false;
#else
    if (!m_connected) { m_lastError = "Not connected"; return false; }
    auto *pgm = static_cast<PROGRAMMER *>(m_pgm);
    auto *p   = static_cast<AVRPART *>(m_part);

    AVRMEM *mem = avr_locate_mem(p, fuseName.toLocal8Bit().constData());
    if (!mem) { m_lastError = QString("No such fuse: %1").arg(fuseName); return false; }

    unsigned char val = 0;
    if (pgm->read_byte(pgm, p, mem, 0, &val) < 0) {
        m_lastError = QString("Failed to read %1").arg(fuseName);
        return false;
    }
    valueOut = val;
    return true;
#endif
}

bool AvrdudeBackend::writeFuse(const QString &fuseName, uint8_t value)
{
#ifndef HAVE_LIBAVRDUDE
    Q_UNUSED(fuseName); Q_UNUSED(value);
    m_lastError = "Built without libavrdude support";
    return false;
#else
    if (!m_connected) { m_lastError = "Not connected"; return false; }
    auto *pgm = static_cast<PROGRAMMER *>(m_pgm);
    auto *p   = static_cast<AVRPART *>(m_part);

    AVRMEM *mem = avr_locate_mem(p, fuseName.toLocal8Bit().constData());
    if (!mem) { m_lastError = QString("No such fuse: %1").arg(fuseName); return false; }

    if (avr_write_byte(pgm, p, mem, 0, value) < 0) {
        m_lastError = QString("Failed to write %1").arg(fuseName);
        return false;
    }

    // Read back to confirm
    unsigned char check = 0;
    if (pgm->read_byte(pgm, p, mem, 0, &check) < 0 || check != value) {
        m_lastError = QString("%1 write verify mismatch (wrote 0x%2, read 0x%3)")
                           .arg(fuseName)
                           .arg(value, 2, 16, QChar('0'))
                           .arg(check, 2, 16, QChar('0'));
        return false;
    }
    return true;
#endif
}
