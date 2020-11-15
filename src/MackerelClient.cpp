#include "MackerelClient.h"

// https://github.com/bblanchon/ArduinoStreamUtils
#include <StreamUtils.h>
#include <time.h>

// XXX hostによって異なるだろうから固定だとまずそう。
// https://www.cybertrust.co.jp/sureserver/download-ca/sureserver-ov.html
const char *mackerel_root_ca =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDdzCCAl+gAwIBAgIBADANBgkqhkiG9w0BAQsFADBdMQswCQYDVQQGEwJKUDEl\n"
    "MCMGA1UEChMcU0VDT00gVHJ1c3QgU3lzdGVtcyBDTy4sTFRELjEnMCUGA1UECxMe\n"
    "U2VjdXJpdHkgQ29tbXVuaWNhdGlvbiBSb290Q0EyMB4XDTA5MDUyOTA1MDAzOVoX\n"
    "DTI5MDUyOTA1MDAzOVowXTELMAkGA1UEBhMCSlAxJTAjBgNVBAoTHFNFQ09NIFRy\n"
    "dXN0IFN5c3RlbXMgQ08uLExURC4xJzAlBgNVBAsTHlNlY3VyaXR5IENvbW11bmlj\n"
    "YXRpb24gUm9vdENBMjCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBANAV\n"
    "OVKxUrO6xVmCxF1SrjpDZYBLx/KWvNs2l9amZIyoXvDjChz335c9S672XewhtUGr\n"
    "zbl+dp+++T42NKA7wfYxEUV0kz1XgMX5iZnK5atq1LXaQZAQwdbWQonCv/Q4EpVM\n"
    "VAX3NuRFg3sUZdbcDE3R3n4MqzvEFb46VqZab3ZpUql6ucjrappdUtAtCms1FgkQ\n"
    "hNBqyjoGADdH5H5XTz+L62e4iKrFvlNVspHEfbmwhRkGeC7bYRr6hfVKkaHnFtWO\n"
    "ojnflLhwHyg/i/xAXmODPIMqGplrz95Zajv8bxbXH/1KEOtOghY6rCcMU/Gt1SSw\n"
    "awNQwS08Ft1ENCcadfsCAwEAAaNCMEAwHQYDVR0OBBYEFAqFqXdlBZh8QIH4D5cs\n"
    "OPEK7DzPMA4GA1UdDwEB/wQEAwIBBjAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3\n"
    "DQEBCwUAA4IBAQBMOqNErLlFsceTfsgLCkLfZOoc7llsCLqJX2rKSpWeeo8HxdpF\n"
    "coJxDjrSzG+ntKEju/Ykn8sX/oymzsLS28yN/HH8AynBbF0zX2S2ZTuJbxh2ePXc\n"
    "okgfGT+Ok+vx+hfuzU7jBBJV1uXk3fs+BXziHV7Gp7yXT2g69ekuCkO2r1dcYmh8\n"
    "t/2jioSgrGK+KwmHNPBqAbubKVY8/gA3zyNs8U6qtnRGEmyR7jTV7JqR50S+kDFy\n"
    "1UkC9gLl9B/rfNmWVan/7Ir5mUf/NVoCqgTLiluHcSmRvaS0eg29mvVXIwAHIRc/\n"
    "SjnRBUkLp7Y3gaVdjKozXoEofKd9J+sAro03\n"
    "-----END CERTIFICATE-----\n";

MackerelClient::MackerelClient(
    MackerelHostMetric *hostMetricsStorage,
    size_t hostMetricsStorageSize,
    MackerelServiceMetric *serviceMetricsStorage,
    size_t serviceMetricsStorageSize,
    MackerelStr apiKey,
    MackerelStr apiHost)
{
        hostMetricsPool.setStorage(hostMetricsStorage, hostMetricsStorageSize, 0);
        serviceMetricsPool.setStorage(serviceMetricsStorage, serviceMetricsStorageSize, 0);
        this->apiKey = apiKey;
        this->apiHost = apiHost;

        // https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFiClientSecure/examples/WiFiClientSecure/WiFiClientSecure.ino
        client.setCACert(mackerel_root_ca);
}

MackerelClient::~MackerelClient()
{
}

int MackerelClient::registerHost(MackerelStr name, char* registeredHostId)
{
        // https://mackerel.io/ja/api-docs/entry/hosts#create
        DynamicJsonDocument doc(1024);
        doc["name"] = name;
        // TODO metaの要素を引数で追加指定できるようにする。
        // TODO metaを含めてホスト情報を更新できるようにする。
        JsonObject metaObj = doc.createNestedObject("meta");
        metaObj["agent-name"] = "ArduinoMackerelClient";
        metaObj["agent-version"] = MACKERELCLIENT_VERSION;

        DynamicJsonDocument response(128);
        if (post("/api/v0/hosts", doc, response))
        {
                return -1;
        }

        strcpy(registeredHostId, response["id"]);
        return 0;
}

void MackerelClient::setHostId(MackerelStr hostId)
{
        strcpy(this->hostId, hostId);
}

int MackerelClient::addHostMetric(MackerelStr name, float value)
{
        if (hostMetricsPool.full())
        {
                return -1;
        }

        MackerelHostMetric metric;
        metric.hostId = hostId;
        metric.time = getEpoch();
        metric.name = name;
        metric.value = value;

        hostMetricsPool.push_back(metric);
        return 0;
}

int MackerelClient::postHostMetrics()
{
        if (hostMetricsPool.empty())
        {
                return 0;
        }

        // https://mackerel.io/ja/api-docs/entry/host-metrics#post
        DynamicJsonDocument doc(1024);

        JsonArray data = doc.to<JsonArray>();
        for (MackerelHostMetric &metric : hostMetricsPool)
        {
                JsonObject metricObj = data.createNestedObject();
                metricObj["hostId"] = metric.hostId;
                metricObj["name"] = metric.name;
                metricObj["time"] = metric.time;
                metricObj["value"] = metric.value;
        }

        DynamicJsonDocument response(128);
        post("/api/v0/tsdb", doc, response);

        hostMetricsPool.clear();
        return 0;
}

int MackerelClient::addServiceMetric(MackerelStr name, float value)
{
        if (serviceMetricsPool.full())
        {
                return -1;
        }

        MackerelServiceMetric metric;
        metric.time = getEpoch();
        metric.name = name;
        metric.value = value;

        serviceMetricsPool.push_back(metric);
        return 0;
}

int MackerelClient::postServiceMetrics(MackerelStr serviceName)
{
        if (serviceMetricsPool.empty())
        {
                return 0;
        }

        // https://mackerel.io/ja/api-docs/entry/service-metrics#post
        DynamicJsonDocument doc(1024);

        JsonArray data = doc.to<JsonArray>();
        for (MackerelServiceMetric &metric : serviceMetricsPool)
        {
                JsonObject metricObj = data.createNestedObject();
                metricObj["name"] = metric.name;
                metricObj["time"] = metric.time;
                metricObj["value"] = metric.value;
        }

        char path[128];
        sprintf(path, "/api/v0/services/%s/tsdb", serviceName);
        DynamicJsonDocument response(128);
        post(path, doc, response);

        serviceMetricsPool.clear();
        return 0;
}

time_t MackerelClient::getEpoch()
{
        // TODO RTC使えるならそっちだろうとは思う
        return time(NULL);
}

// TODO この辺はJSONのRESTクライアントとして切り出せそう。
int MackerelClient::get(MackerelStr path, JsonDocument &response)
{
        openRequest("GET", path);
        client.println("Connection: close");
        client.println();

        return receiveResponse(response);
}

struct NullWriter
{
        size_t write(uint8_t c)
        {
                return 1;
        }
        size_t write(const uint8_t *s, size_t n)
        {
                return n;
        }
};

int MackerelClient::post(MackerelStr path, JsonDocument &body, JsonDocument &response)
{
        // TODO sizeの取り方。。。
        // NullWriter nullWriter;
        // size_t bodyLength = serializeJson(body, nullWriter);
        size_t bodyLength = serializeJson(body, Serial);

        openRequest("POST", path);
        client.println("Content-Type: application/json");
        client.print("Content-Length: ");
        client.println(bodyLength);
        client.println("Connection: close");
        client.println();
        serializeJson(body, client);

        return receiveResponse(response);
}

int MackerelClient::openRequest(MackerelStr method, MackerelStr path)
{
        Serial.println("\nStarting connection to server...");
        Serial.print("Host: ");
        Serial.println(apiHost);
        if (!client.connect(apiHost, 443))
        {
                Serial.println("Connection failed!");
                return -1;
        }
        Serial.println("Connected to server!");
        client.print(method);
        client.print(" ");
        client.print(path);
        client.println(" HTTP/1.0");
        client.print("Host: ");
        client.println(apiHost);
        client.print("X-Api-Key: ");
        client.println(apiKey);

        return 0;
}

int MackerelClient::receiveResponse(JsonDocument &response)
{
        while (client.connected())
        {
                String line = client.readStringUntil('\n');
                Serial.println(line);
                if (line == "\r")
                {
                        Serial.println("headers received");
                        break;
                }
        }
        while (client.available())
        {
                ReadLoggingStream loggingStream(client, Serial);
                deserializeJson(response, loggingStream);
        }
        client.stop();
        Serial.println("\nbody received");

        return 0;
}
