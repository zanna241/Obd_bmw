#include "logger.h"

#include <FS.h>
#include <SD_MMC.h>
#include <math.h>

#include "can.h"
#include "time_manager.h"
#include "vehicle_data.h"
#include "discovery.h"
#include "version.h"
#include <esp_heap_caps.h>

static constexpr int SD_CLK = 12;
static constexpr int SD_CMD = 11;
static constexpr int SD_D0  = 13;

static File logFile;
static File decodedFile;
static File eventFile;
static bool active = false;
static bool sdReady = false;
static String currentPath;
static String catalogPath;
static String decodedPath;
static String eventPath;
static String discoveryPath;

static String files[96];
static size_t sizes[96];
static int fileCount = 0;

static constexpr size_t RAW_BUFFER_SIZE = 32768;
static char rawBufferFallback[4096];
static char *rawBuffer = rawBufferFallback;
static size_t rawBufferCapacity = sizeof(rawBufferFallback);
static size_t rawBufferUsed = 0;
static uint32_t lastFlush = 0;

static uint32_t framesWritten = 0;
static uint32_t framesDropped = 0;
static uint32_t stallCount = 0;
static uint32_t maxWriteMs = 0;
static uint32_t maxFlushMs = 0;
static uint32_t lastIoMs = 0;
static constexpr uint32_t STALL_THRESHOLD_MS = 250;

static void record_io(const char *operation, uint32_t started, uint32_t &maximum)
{
    const uint32_t elapsed = millis() - started;
    lastIoMs = elapsed;
    if (elapsed > maximum) maximum = elapsed;
    if (elapsed >= STALL_THRESHOLD_MS) {
        ++stallCount;
        Serial.printf("LOGGER STALL: %s=%lu ms count=%lu\n", operation,
                      (unsigned long)elapsed, (unsigned long)stallCount);
    }
}

static String normalize_log_path(const String &input)
{
    String name = input;
    name.replace("\\", "/");

    // Keep only the basename. Log management is intentionally confined to /logs.
    int slash = name.lastIndexOf('/');
    if (slash >= 0) name = name.substring(slash + 1);

    if (!name.length() || name.indexOf("..") >= 0) return String();
    return "/logs/" + name;
}

static void refresh_files()
{
    fileCount = 0;
    if (!sdReady) return;

    File root = SD_MMC.open("/logs");
    if (!root || !root.isDirectory()) return;

    File f = root.openNextFile();
    while (f && fileCount < 96) {
        if (!f.isDirectory()) {
            String rawName = f.name();
            String fullPath = normalize_log_path(rawName);

            if (fullPath.length() && fullPath.endsWith(".csv")) {
                files[fileCount] = fullPath;
                sizes[fileCount] = f.size();
                fileCount++;
            }
        }
        f = root.openNextFile();
    }
}

static String session_name()
{
    if (time_valid()) {
        String iso = time_iso();
        // YYYY-MM-DD HH:MM:SS -> YYYYMMDD_HHMMSS
        iso.replace("-", "");
        iso.replace(":", "");
        iso.replace(" ", "_");
        return iso;
    }

    return String("boot_") + String(millis());
}

static bool flush_raw_buffer()
{
    if (!active || !logFile || rawBufferUsed == 0) return true;

    const uint32_t started = millis();
    size_t written = logFile.write(
        (const uint8_t *)rawBuffer,
        rawBufferUsed
    );

    record_io("raw_write", started, maxWriteMs);
    if (written != rawBufferUsed) {
        Serial.printf(
            "LOGGER ERROR: SD write %u/%u bytes\n",
            (unsigned)written,
            (unsigned)rawBufferUsed
        );
        return false;
    }

    rawBufferUsed = 0;
    return true;
}

static void write_catalog_file()
{
    if (!sdReady || !catalogPath.length()) return;

    File f = SD_MMC.open(catalogPath, FILE_WRITE);
    if (!f) {
        Serial.println("LOGGER ERROR: impossibile creare catalog CSV");
        return;
    }

    f.println("id_hex,id_dec,extended,count,last_ms,dlc,last_data");

    for (int i = 0; i < can_catalog_count(); ++i) {
        CanCatalogEntry e;
        if (!can_catalog_get(i, e)) continue;

        char dataHex[3 * 8 + 1] = {0};
        size_t pos = 0;

        for (uint8_t b = 0; b < e.dlc && b < 8; ++b) {
            int n = snprintf(
                dataHex + pos,
                sizeof(dataHex) - pos,
                b ? " %02X" : "%02X",
                e.data[b]
            );
            if (n > 0) pos += (size_t)n;
            if (pos >= sizeof(dataHex)) break;
        }

        f.printf(
            "0x%lX,%lu,%d,%lu,%lu,%u,\"%s\"\n",
            (unsigned long)e.id,
            (unsigned long)e.id,
            e.extended ? 1 : 0,
            (unsigned long)e.count,
            (unsigned long)e.lastMillis,
            e.dlc,
            dataHex
        );
    }

    f.flush();
    f.close();

    Serial.printf("CAN CATALOG: %s\n", catalogPath.c_str());
}

bool logger_begin()
{
    Serial.println("SD: inizializzazione SD_MMC 1-bit...");
    Serial.printf("SD pins CLK=%d CMD=%d D0=%d\n", SD_CLK, SD_CMD, SD_D0);

    if (!SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0)) {
        Serial.println("SD ERROR: SD_MMC.setPins() fallito");
        return false;
    }

    if (!SD_MMC.begin("/sdcard", true)) {
        Serial.println("SD ERROR: mount fallito");
        Serial.println("Verifica microSD e FAT32.");
        return false;
    }

    if (SD_MMC.cardType() == CARD_NONE) {
        Serial.println("SD ERROR: nessuna scheda rilevata");
        return false;
    }

    sdReady = true;

    if (!SD_MMC.exists("/logs")) SD_MMC.mkdir("/logs");

    if (rawBuffer == rawBufferFallback && psramFound()) {
        char *p = (char*)heap_caps_malloc(RAW_BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (p) { rawBuffer=p; rawBufferCapacity=RAW_BUFFER_SIZE; Serial.printf("LOGGER: RAW buffer in PSRAM %u bytes\n", (unsigned)rawBufferCapacity); }
    }

    Serial.printf(
        "SD OK: %.1f GB totali, %.1f MB usati\n",
        SD_MMC.totalBytes() / 1073741824.0,
        SD_MMC.usedBytes() / 1048576.0
    );

    refresh_files();
    return true;
}

bool logger_start()
{
    if (!sdReady) return false;
    if (active) return true;

    if (!SD_MMC.exists("/logs") && !SD_MMC.mkdir("/logs")) return false;

    String session = session_name();

    currentPath = "/logs/" + String(FW_TAG) + "_can_" + session + "_raw.csv";
    catalogPath = "/logs/" + String(FW_TAG) + "_can_" + session + "_catalog.csv";
    decodedPath = "/logs/" + String(FW_TAG) + "_can_" + session + "_decoded.csv";
    eventPath = "/logs/" + String(FW_TAG) + "_can_" + session + "_events.csv";
    discoveryPath = "/logs/" + String(FW_TAG) + "_can_" + session + "_discovery.csv";

    logFile = SD_MMC.open(currentPath, FILE_WRITE);
    if (!logFile) {
        Serial.println("LOGGER ERROR: impossibile creare CAN raw CSV");
        return false;
    }

    // One row per actual CAN frame. No decoded BMW values are invented.
    // Header must match logger_log_can_common()'s actual column order
    // exactly (timestamp_us,millis,direction,id_hex,...) - it was missing
    // "direction" here, which silently shifted every column by one for
    // anything parsing this CSV by header name (e.g. spreadsheet imports).
    logFile.println(
        "timestamp_us,millis,direction,id_hex,id_dec,extended,rtr,dlc,"
        "d0,d1,d2,d3,d4,d5,d6,d7"
    );
    logFile.flush();

    decodedFile = SD_MMC.open(decodedPath, FILE_WRITE);
    if (decodedFile) {
        decodedFile.println("millis,rpm,speed,coolant,oil,intake,maf,load,accelerator,baro,ambient,voltage,turbo_bar,boost_abs_kpa,boost_target_kpa,rail_bar,rail_target_bar,egr_cmd,egr_actual,egt1,egt2,egt3,nox1_ppm,nox2_ppm,lambda1,lambda2,dpf_diff_hpa,dpf_trigger_pct,dpf_regen,dpf_soot_g,dpf_ash_g,distance_since_regen_km,dpf_remaining_life_km,successful_regens,gearbox_c,gear,gear_input_rpm,gear_output_rpm,converter_slip_rpm,lockup,gearbox_torque_nm");
        decodedFile.flush();
    }

    eventFile = SD_MMC.open(eventPath, FILE_WRITE);
    if (eventFile) {
        eventFile.println("millis,time,event,rpm,speed,gear,oil_c,dpf_soot_g,dpf_ash_g,atf_c,converter_slip_rpm");
        eventFile.flush();
    }

    rawBufferUsed = 0;
    framesWritten = 0;
    framesDropped = 0;
    stallCount = 0;
    maxWriteMs = 0;
    maxFlushMs = 0;
    lastIoMs = 0;
    lastFlush = millis();

    can_catalog_clear();
    discovery_reset();

    active = true;
    logger_mark_event("LOGGER_START");

    Serial.printf("CAN LOGGER START: %s\n", currentPath.c_str());
    return true;
}


static void write_discovery_file()
{
    if (!sdReady || !discoveryPath.length()) return;
    File f = SD_MMC.open(discoveryPath, FILE_WRITE);
    if (!f) return;

    f.println("id_hex,id_dec,extended,dlc,count,first_ms,last_ms,avg_period_ms,last_data,b0_min,b0_max,b0_changes,b1_min,b1_max,b1_changes,b2_min,b2_max,b2_changes,b3_min,b3_max,b3_changes,b4_min,b4_max,b4_changes,b5_min,b5_max,b5_changes,b6_min,b6_max,b6_changes,b7_min,b7_max,b7_changes");

    for (int i = 0; i < discovery_count(); ++i) {
        DiscoveryCanEntry e;
        if (!discovery_get(i, e)) continue;
        float avg = e.count > 1 ? (float)(e.lastMs - e.firstMs) / (float)(e.count - 1) : 0.0f;
        char dataHex[32] = {0};
        size_t pos = 0;
        for (uint8_t b = 0; b < e.dlc && b < 8; ++b) {
            int n = snprintf(dataHex + pos, sizeof(dataHex) - pos, b ? " %02X" : "%02X", e.last[b]);
            if (n > 0) pos += (size_t)n;
        }
        f.printf("0x%lX,%lu,%d,%u,%lu,%lu,%lu,%.3f,\"%s\"",
                 (unsigned long)e.id,(unsigned long)e.id,e.extended?1:0,e.dlc,
                 (unsigned long)e.count,(unsigned long)e.firstMs,(unsigned long)e.lastMs,avg,dataHex);
        for (int b = 0; b < 8; ++b) {
            f.printf(",%u,%u,%lu", e.minv[b], e.maxv[b], (unsigned long)e.changes[b]);
        }
        f.println();
    }
    f.flush();
    f.close();
}

void logger_stop()
{
    if (!active) return;

    logger_mark_event("LOGGER_STOP");
    flush_raw_buffer();
    logFile.flush();
    logFile.close();
    if (decodedFile) { decodedFile.flush(); decodedFile.close(); }
    if (eventFile) { eventFile.flush(); eventFile.close(); }
    active = false;

    write_catalog_file();
    write_discovery_file();
    refresh_files();

    Serial.printf(
        "CAN LOGGER STOP: frames=%lu dropped=%lu\n",
        (unsigned long)framesWritten,
        (unsigned long)framesDropped
    );
}

static void logger_log_can_common(
    const char *direction,
    uint64_t timestampUs,
    uint32_t id,
    bool extended,
    bool rtr,
    uint8_t dlc,
    const uint8_t *data
)
{
    if (!active || !sdReady) return;

    char line[192];

    int n = snprintf(
        line,
        sizeof(line),
        "%llu,%lu,%s,0x%lX,%lu,%d,%d,%u",
        (unsigned long long)timestampUs,
        (unsigned long)millis(),
        direction,
        (unsigned long)id,
        (unsigned long)id,
        extended ? 1 : 0,
        rtr ? 1 : 0,
        (unsigned)dlc
    );

    if (n <= 0 || n >= (int)sizeof(line)) {
        framesDropped++;
        return;
    }

    size_t pos = (size_t)n;

    for (int i = 0; i < 8; ++i) {
        int x;
        if (i < dlc && data) {
            x = snprintf(line + pos, sizeof(line) - pos, ",%02X", data[i]);
        } else {
            x = snprintf(line + pos, sizeof(line) - pos, ",");
        }

        if (x <= 0 || pos + (size_t)x >= sizeof(line)) {
            framesDropped++;
            return;
        }
        pos += (size_t)x;
    }

    if (pos + 2 >= sizeof(line)) {
        framesDropped++;
        return;
    }

    line[pos++] = '\n';
    line[pos] = '\0';

    if (rawBufferUsed + pos > rawBufferCapacity) {
        if (!flush_raw_buffer()) {
            framesDropped++;
            rawBufferUsed = 0;
        }
    }

    memcpy(rawBuffer + rawBufferUsed, line, pos);
    rawBufferUsed += pos;
    framesWritten++;
}

void logger_log_can_frame(
    uint64_t timestampUs,
    uint32_t id,
    bool extended,
    bool rtr,
    uint8_t dlc,
    const uint8_t *data
)
{
    logger_log_can_common("RX", timestampUs, id, extended, rtr, dlc, data);
}

void logger_log_can_tx(
    uint64_t timestampUs,
    uint32_t id,
    bool extended,
    bool rtr,
    uint8_t dlc,
    const uint8_t *data
)
{
    logger_log_can_common("TX", timestampUs, id, extended, rtr, dlc, data);
}

void logger_loop()
{
    if (!active || !sdReady) return;

    uint32_t now = millis();

    // Periodic buffered SD write.
    if (rawBufferUsed > 0 &&
        (rawBufferUsed >= 16384 || now - lastFlush >= 1000)) {
        flush_raw_buffer();
        lastFlush = now;
    }

    // Decoded 2 Hz snapshot from the same VehicleData used by display and Web.
    static uint32_t lastDecoded = 0;
    if (decodedFile && now - lastDecoded >= 500) {
        lastDecoded = now;
        VehicleData &v = vehicle_data();
        auto pf=[&](float x){ if(isnan(x)) decodedFile.print(""); else decodedFile.print(x,3); };
        decodedFile.print(now); decodedFile.print(',');
        pf(v.rpm);decodedFile.print(',');pf(v.speed);decodedFile.print(',');pf(v.coolant);decodedFile.print(',');pf(v.oil);decodedFile.print(',');pf(v.intake);decodedFile.print(',');pf(v.maf);decodedFile.print(',');pf(v.engineLoad);decodedFile.print(',');pf(v.accelerator);decodedFile.print(',');pf(v.baro);decodedFile.print(',');pf(v.ambient);decodedFile.print(',');pf(v.voltage);decodedFile.print(',');pf(v.turbo);decodedFile.print(',');pf(v.boostAbsKpa);decodedFile.print(',');pf(v.boostTargetKpa);decodedFile.print(',');pf(v.railBar);decodedFile.print(',');pf(v.railTargetBar);decodedFile.print(',');pf(v.egrCommanded);decodedFile.print(',');pf(v.egrActual);decodedFile.print(',');pf(v.egt1);decodedFile.print(',');pf(v.egt2);decodedFile.print(',');pf(v.egt3);decodedFile.print(',');pf(v.nox1Ppm);decodedFile.print(',');pf(v.nox2Ppm);decodedFile.print(',');pf(v.lambda1);decodedFile.print(',');pf(v.lambda2);decodedFile.print(',');pf(v.dpfDiffPressureHpa);decodedFile.print(',');pf(v.dpfNormalizedTrigger);decodedFile.print(',');
        decodedFile.print(v.dpfRegenKnown ? (v.dpfRegen?1:0) : -1);decodedFile.print(',');pf(v.dpfSootMassG);decodedFile.print(',');pf(v.dpfAshMassG);decodedFile.print(',');pf(v.distanceSinceRegenKm);decodedFile.print(',');pf(v.dpfRemainingLifeKm);decodedFile.print(',');decodedFile.print(v.successfulRegensKnown?(long)v.successfulRegens:-1);decodedFile.print(',');pf(v.gearbox);decodedFile.print(',');decodedFile.print(v.gear);decodedFile.print(',');pf(v.gearboxInputRpm);decodedFile.print(',');pf(v.gearboxOutputRpm);decodedFile.print(',');pf(v.converterSlipRpm);decodedFile.print(',');decodedFile.print(v.lockupKnown ? (v.lockup?1:0) : -1);decodedFile.print(',');pf(v.gearboxTorqueNm);decodedFile.println();
    }

    // Metadata flushes are intentionally infrequent: on some cards a flush can
    // block for seconds. Stop/OTA still perform a final synchronous flush.
    static uint32_t lastFileFlush = 0;
    if (now - lastFileFlush >= 15000) {
        lastFileFlush = now;
        const uint32_t started = millis();
        logFile.flush();
        if (decodedFile) decodedFile.flush();
        if (eventFile) eventFile.flush();
        record_io("file_flush", started, maxFlushMs);
        yield();
    }
}

bool logger_active() { return active; }
uint32_t logger_frames_written() { return framesWritten; }
uint32_t logger_frames_dropped() { return framesDropped; }
uint32_t logger_stall_count() { return stallCount; }
uint32_t logger_max_write_ms() { return maxWriteMs; }
uint32_t logger_max_flush_ms() { return maxFlushMs; }
uint32_t logger_last_io_ms() { return lastIoMs; }

bool logger_storage_ready() { return sdReady; }
String logger_storage_name() { return "microSD"; }

uint64_t logger_storage_total()
{
    return sdReady ? SD_MMC.totalBytes() : 0;
}

uint64_t logger_storage_used()
{
    return sdReady ? SD_MMC.usedBytes() : 0;
}

String logger_current_file()
{
    return currentPath;
}

size_t logger_current_size()
{
    if (!active || !logFile) return 0;
    return logFile.size() + rawBufferUsed;
}

int logger_file_count()
{
    refresh_files();
    return fileCount;
}

String logger_file_name(int index)
{
    if (index < 0 || index >= fileCount) return String();
    return files[index];
}

size_t logger_file_size(int index)
{
    if (index < 0 || index >= fileCount) return 0;
    return sizes[index];
}

bool logger_delete(const String &path)
{
    if (!sdReady) {
        Serial.println("LOGGER DELETE: SD non disponibile");
        return false;
    }

    String normalized = normalize_log_path(path);
    if (!normalized.length()) {
        Serial.printf("LOGGER DELETE: percorso non valido: %s\n", path.c_str());
        return false;
    }

    // Do not delete the file currently open for acquisition.
    if (active && normalized == currentPath) {
        Serial.println("LOGGER DELETE: impossibile eliminare il log attivo");
        return false;
    }

    if (!SD_MMC.exists(normalized)) {
        Serial.printf("LOGGER DELETE: file non trovato: %s\n", normalized.c_str());
        refresh_files();
        return false;
    }

    bool ok = SD_MMC.remove(normalized);

    Serial.printf(
        "LOGGER DELETE: %s -> %s\n",
        normalized.c_str(),
        ok ? "OK" : "FAILED"
    );

    refresh_files();
    return ok;
}

void logger_delete_all()
{
    if (!sdReady) return;

    // Close the current acquisition cleanly before mass deletion.
    if (active) logger_stop();

    refresh_files();

    String toDelete[96];
    int n = fileCount;
    for (int i = 0; i < n; ++i) toDelete[i] = files[i];

    for (int i = 0; i < n; ++i) {
        if (toDelete[i].length()) {
            bool ok = SD_MMC.remove(toDelete[i]);
            Serial.printf(
                "LOGGER DELETE ALL: %s -> %s\n",
                toDelete[i].c_str(),
                ok ? "OK" : "FAILED"
            );
        }
    }

    refresh_files();
}


void logger_mark_event(const String &label)
{
    if (!active || !sdReady || !eventFile) return;
    const VehicleData &v = vehicle_data();
    auto fp=[](float x)->String { return isnan(x) ? String() : String(x,3); };
    String safe = label;
    safe.replace("\"", "'");
    safe.replace("\n", " ");
    safe.replace("\r", " ");
    eventFile.printf("%lu,\"%s\",\"%s\",%s,%s,%d,%s,%s,%s,%s,%s\n",
        (unsigned long)millis(), time_iso().c_str(), safe.c_str(),
        fp(v.rpm).c_str(), fp(v.speed).c_str(), v.gear,
        fp(v.oil).c_str(), fp(v.dpfSootMassG).c_str(), fp(v.dpfAshMassG).c_str(),
        fp(v.gearbox).c_str(), fp(v.converterSlipRpm).c_str());
    // Do not force an SD flush from touch/web/CAN callbacks. Filesystem flushes
    // are intentionally centralized in logger_loop() to avoid long synchronous
    // stalls that can starve Wi-Fi and the web server.
}
