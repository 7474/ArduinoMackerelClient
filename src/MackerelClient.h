#ifndef _MACKERELCLIENT_H_
#define _MACKERELCLIENT_H_

// https://github.com/espressif/arduino-esp32/tree/master/libraries/WiFiClientSecure
#include <WiFiClientSecure.h>
// https://arduinojson.org/
#include <ArduinoJson.h>
// https://github.com/janelia-arduino/Vector
#include <Vector.h>

typedef const char *MackerelStr;

typedef struct _MackerelHostMetric
{
    MackerelStr hostId;
    MackerelStr name;
    time_t time; // epoch sec
    float value;
} MackerelHostMetric;

typedef struct _MackerelServiceMetric
{
    MackerelStr name;
    time_t time; // epoch sec
    float value;
} MackerelServiceMetric;

// Return value rule:
// - Pointer:
//     - NULL: Error
//     - other: Success
// - int:
//     - 0: Success
//     - other:
class MackerelClient
{
public:
    MackerelClient(
        MackerelHostMetric *hostMetricsStorage,
        size_t hostMetricsStorageSize,
        MackerelServiceMetric *serviceMetricsStorage,
        size_t serviceMetricsStorageSize,
        MackerelStr apiKey,
        MackerelStr apiHost = "api.mackerelio.com");
    ~MackerelClient();

    /*!
    * https://mackerel.io/ja/api-docs/entry/hosts#create
    * @return HostId
    */
    MackerelStr initializeHost(MackerelStr name);
    void setHostId(MackerelStr hostId);

    int addHostMetric(MackerelStr name, float value);
    int postHostMetrics();

    int addServiceMetric(MackerelStr name, float value);
    int postServiceMetrics(MackerelStr serviceName);

private:
    MackerelStr apiHost;
    MackerelStr apiKey;
    WiFiClientSecure client;

    Vector<MackerelHostMetric> hostMetricsPool;
    Vector<MackerelServiceMetric> serviceMetricsPool;

    time_t getEpoch();

    MackerelStr get(MackerelStr path);
    MackerelStr post(MackerelStr path, JsonDocument &body);
    int openRequest(MackerelStr method, MackerelStr path);
    MackerelStr receiveResponse();
};

#endif