#ifndef PROGRAM_OPTIONS_HPP
#define PROGRAM_OPTIONS_HPP
#include <string>
#include <vector>
#include <filesystem>
#include <chrono>
#include "uSEEDLinkToRingServer/seedLinkClientOptions.hpp"
#include "uSEEDLinkToRingServer/dataLinkClientOptions.hpp"

#define APPLICATION_NAME "seedLinkToRingServer"

namespace 
{

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
    ::OTelHTTPLogOptions otelHTTPLogOptions;
    //std::string prometheusURL{"localhost:9020"}; 
    std::vector<USEEDLinkToRingServer::DataLinkClientOptions> dataLinkClientOptions;
    USEEDLinkToRingServer::SEEDLinkClientOptions seedLinkClientOptions;
    int importQueueSize{8192};
    int verbosity{3};
    bool exportLogs{false};
    bool exportMetrics{false};
};

::ProgramOptions parseIniFile(const std::filesystem::path &iniFile);

}
#endif
