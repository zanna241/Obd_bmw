#include "online_ota.h"
#include "version.h"
#include "logger.h"
#include "can.h"
#include "wifi_manager.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <mbedtls/sha256.h>

static const char *MANIFEST_URL =
    "https://raw.githubusercontent.com/zanna241/Obd_bmw/main/firmware/manifest.json";

// GitHub can serve raw content through either the RSA (G2) or ECC (G3)
// DigiCert hierarchy, depending on the edge reached by the device. Keep both
// trusted roots in one mbedTLS bundle; never replace this with setInsecure().
static const char GITHUB_CA_BUNDLE[] PROGMEM = R"PEM(-----BEGIN CERTIFICATE-----
MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH
MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI
2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx
1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ
q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz
tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ
vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP
BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV
5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY
1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4
NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG
Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91
8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe
pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl
MrY=
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIICPzCCAcWgAwIBAgIQBVVWvPJepDU1w6QP1atFcjAKBggqhkjOPQQDAzBhMQsw
CQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3d3cu
ZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBHMzAe
Fw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVTMRUw
EwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5jb20x
IDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEczMHYwEAYHKoZIzj0CAQYF
K4EEACIDYgAE3afZu4q4C/sLfyHS8L6+c/MzXRq8NOrexpu80JX28MzQC7phW1FG
fp4tn+6OYwwX7Adw9c+ELkCDnOg/QW07rdOkFFk2eJ0DQ+4QE2xy3q6Ip6FrtUPO
Z9wj/wMco+I+o0IwQDAPBgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAd
BgNVHQ4EFgQUs9tIpPmhxdiuNkHMEWNpYim8S8YwCgYIKoZIzj0EAwMDaAAwZQIx
AK288mw/EkrRLTnDCgmXc/SINoyIJ7vmiI1Qhadj+Z4y3maTD/HMsQmP3Wyr+mt/
oAIwOWZbwmSNuJ5Q3KjVSaLtx9zRSX8XAbjIho9OjIgrqJqpisXRAL34VOKa5Vt8
sycX
-----END CERTIFICATE-----)PEM";

static String httpFailure(const char *phase, int code)
{
    String out = String(phase) + " " + String(code);
    String detail = HTTPClient::errorToString(code);
    if (detail.length()) out += " " + detail;
    return out;
}

static String jsonString(const String &json, const char *key)
{
    String token = "\"" + String(key) + "\"";
    int p = json.indexOf(token);
    if (p < 0) return "";
    p = json.indexOf(':', p + token.length());
    if (p < 0) return "";
    p = json.indexOf('"', p + 1);
    if (p < 0) return "";
    String out;
    bool escaped = false;
    for (++p; p < (int)json.length(); ++p) {
        char c = json[p];
        if (escaped) { out += c; escaped = false; continue; }
        if (c == '\\') { escaped = true; continue; }
        if (c == '"') break;
        out += c;
    }
    return out;
}

static size_t jsonSize(const String &json, const char *key)
{
    String token = "\"" + String(key) + "\"";
    int p = json.indexOf(token);
    if (p < 0) return 0;
    p = json.indexOf(':', p + token.length());
    if (p < 0) return 0;
    while (++p < (int)json.length() && isspace((unsigned char)json[p])) {}
    size_t value = 0;
    while (p < (int)json.length() && isdigit((unsigned char)json[p])) {
        value = value * 10 + (json[p++] - '0');
    }
    return value;
}

static int compareVersion(const String &a, const String &b)
{
    int ai = 0, bi = 0;
    for (int part = 0; part < 4; ++part) {
        long av = 0, bv = 0;
        while (ai < (int)a.length() && !isdigit((unsigned char)a[ai])) ai++;
        while (bi < (int)b.length() && !isdigit((unsigned char)b[bi])) bi++;
        while (ai < (int)a.length() && isdigit((unsigned char)a[ai])) av = av * 10 + a[ai++] - '0';
        while (bi < (int)b.length() && isdigit((unsigned char)b[bi])) bv = bv * 10 + b[bi++] - '0';
        if (av < bv) return -1;
        if (av > bv) return 1;
    }
    return 0;
}

void online_ota_begin() {}

bool online_ota_check(OnlineOtaInfo &info)
{
    info = OnlineOtaInfo();
    if (!wifi_connected()) {
        info.error = "Connettere prima il Wi-Fi a Internet";
        return false;
    }

    IPAddress resolved;
    if (!WiFi.hostByName("raw.githubusercontent.com", resolved)) {
        info.error = "DNS GitHub non risolto";
        return false;
    }

    WiFiClientSecure client;
    client.setCACert(GITHUB_CA_BUNDLE);
    client.setHandshakeTimeout(20);
    client.setTimeout(12);
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(12000);
    if (!http.begin(client, MANIFEST_URL)) {
        info.error = "Impossibile aprire il server aggiornamenti";
        return false;
    }
    int code = HTTP_ERROR_CONNECTION_REFUSED;
    for (int attempt = 0; attempt < 3 && code < 0; ++attempt) {
        code = http.GET();
        if (code < 0 && attempt < 2) delay(350);
    }
    if (code != HTTP_CODE_OK) {
        info.error = httpFailure("Manifest", code);
        http.end();
        return false;
    }
    String json = http.getString();
    http.end();

    info.version = jsonString(json, "version");
    info.build = jsonString(json, "build");
    info.url = jsonString(json, "url");
    info.sha256 = jsonString(json, "sha256");
    info.notes = jsonString(json, "notes");
    info.size = jsonSize(json, "size");
    info.sha256.toLowerCase();
    if (!info.version.length() || !info.url.startsWith("https://") ||
        info.sha256.length() != 64 || info.size < 100000) {
        info.error = "Manifest non valido";
        return false;
    }
    info.valid = true;
    info.updateAvailable = compareVersion(FW_VERSION, info.version) < 0;
    return true;
}

bool online_ota_install(const OnlineOtaInfo &info, OnlineOtaProgress progress, String &error)
{
    error = "";
    if (!info.valid || !info.updateAvailable) { error = "Aggiornamento non valido"; return false; }
    if (!wifi_connected()) { error = "Wi-Fi Internet non connesso"; return false; }
    if (logger_active()) logger_stop();

    WiFiClientSecure client;
    client.setCACert(GITHUB_CA_BUNDLE);
    client.setHandshakeTimeout(20);
    client.setTimeout(15);
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(15000);
    if (!http.begin(client, info.url)) { error = "Connessione download fallita"; return false; }
    int code = http.GET();
    if (code != HTTP_CODE_OK) { error = httpFailure("Download", code); http.end(); return false; }
    int announced = http.getSize();
    if (announced > 0 && (size_t)announced != info.size) {
        error = "Dimensione download diversa dal manifest"; http.end(); return false;
    }
    if (!Update.begin(info.size, U_FLASH)) {
        error = "Partizione OTA insufficiente"; Update.printError(Serial); http.end(); return false;
    }

    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);
    WiFiClient *stream = http.getStreamPtr();
    uint8_t buffer[4096];
    size_t received = 0;
    uint32_t lastData = millis();
    if (progress) progress(0, info.size, "DOWNLOAD");

    while (received < info.size) {
        size_t available = stream->available();
        if (available) {
            size_t want = min(available, sizeof(buffer));
            size_t got = stream->readBytes(buffer, want);
            if (!got) continue;
            if (Update.write(buffer, got) != got) {
                error = "Errore scrittura flash";
                Update.abort(); http.end(); mbedtls_sha256_free(&sha); return false;
            }
            mbedtls_sha256_update(&sha, buffer, got);
            received += got;
            lastData = millis();
            if (progress) progress(received, info.size, "DOWNLOAD");
            can_update();
            yield();
        } else {
            if (!stream->connected() || millis() - lastData > 15000) {
                error = "Download interrotto";
                Update.abort(); http.end(); mbedtls_sha256_free(&sha); return false;
            }
            delay(1);
        }
    }
    http.end();

    uint8_t digest[32];
    mbedtls_sha256_finish(&sha, digest);
    mbedtls_sha256_free(&sha);
    char hex[65];
    for (int i = 0; i < 32; ++i) snprintf(hex + i * 2, 3, "%02x", digest[i]);
    hex[64] = 0;
    if (progress) progress(received, info.size, "VERIFICA SHA-256");
    if (!info.sha256.equalsIgnoreCase(hex)) {
        error = "Checksum SHA-256 errato";
        Update.abort();
        return false;
    }
    if (!Update.end(true) || Update.hasError()) {
        error = "Verifica finale firmware fallita";
        Update.printError(Serial);
        return false;
    }
    if (progress) progress(received, info.size, "COMPLETATO");
    return true;
}
