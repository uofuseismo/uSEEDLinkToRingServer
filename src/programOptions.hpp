#ifndef PROGRAM_OPTIONS_HPP
#define PROGRAM_OPTIONS_HPP
#include <string>
#include <vector>
#include <filesystem>
#include <chrono>
#include "uSEEDLinkToRingServer/seedLinkClientOptions.hpp"
#include "uSEEDLinkToRingServer/dataLinkClientOptions.hpp"
//#include "uSEEDLinkToRingServer/seedLinkWriterOptions.hpp"

#define APPLICATION_NAME "seedLinkToRingServer"

namespace 
{

struct OTelGRPCMetricsOptions
{
    std::string url{"localhost"};
    uint16_t port{4317};
    std::chrono::milliseconds exportInterval{std::chrono::seconds{30}};
    std::chrono::milliseconds exportTimeOut{500};
    std::filesystem::path certificatePath; // Path to the cert file
};

struct OTelGRPCLogOptions
{
    std::string url{"localhost"};
    uint16_t port{4317};
    //std::chrono::milliseconds exportTimeOut{500};
    std::filesystem::path certificatePath; // Path to the cert file
};

struct OTelHTTPMetricsOptions
{
    std::string url{"localhost:4318"};
    std::chrono::milliseconds exportInterval{30000};
    std::chrono::milliseconds exportTimeOut{500};
    std::string suffix{"/v1/metrics"};
};

struct OTelHTTPLogOptions
{
    std::string url{"localhost:4318"};
    std::filesystem::path certificatePath;
    std::string suffix{"/v1/logs"};
};

struct ProgramOptions
{
    std::string applicationName{APPLICATION_NAME};
    ::OTelHTTPMetricsOptions otelHTTPMetricsOptions;
    ::OTelGRPCMetricsOptions otelGRPCMetricsOptions;
    ::OTelHTTPLogOptions otelHTTPLogOptions;
    ::OTelGRPCLogOptions otelGRPCLogOptions;
    //std::string prometheusURL{"localhost:9020"}; 
    std::vector<USEEDLinkToRingServer::DataLinkClientOptions>
        dataLinkClientOptions;
    //std::vector<USEEDLinkToRingServer::SEEDLinkWriterOptions>
    //    seedLinkWriterOptions;
    USEEDLinkToRingServer::SEEDLinkClientOptions seedLinkClientOptions;
    std::string dataSource;
    int importQueueSize{8192};
    int verbosity{3};
    bool exportLogs{false};
    bool exportMetrics{false};
    bool exportMetricsWithHTTP{true};
    bool exportLogsWithHTTP{true};
};

::ProgramOptions parseIniFile(const std::filesystem::path &iniFile);

}
#endif
