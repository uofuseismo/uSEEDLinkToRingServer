#ifndef USEED_LINK_TO_RING_SERVER_LOGGER_HPP
#define USEED_LINK_TO_RING_SERVER_LOGGER_HPP 
#include <opentelemetry/exporters/otlp/otlp_http_exporter_options.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_log_record_exporter_factory.h>
#include <opentelemetry/logs/provider.h>
#include <opentelemetry/sdk/logs/logger_provider_factory.h>
#include <opentelemetry/sdk/logs/simple_log_record_processor_factory.h>
#include "otelSpdlogSink.hpp"
namespace
{
std::shared_ptr<opentelemetry::sdk::logs::LoggerProvider> loggerProvider{nullptr};

std::shared_ptr<spdlog::logger> initializeLogger(const ::ProgramOptions &programOptions)
{
    std::shared_ptr<spdlog::logger> logger{nullptr};
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt> (); 
    if (programOptions.exportLogs)
    {
        namespace otel = opentelemetry;
        otel::exporter::otlp::OtlpHttpLogRecordExporterOptions httpOptions;
        httpOptions.url = programOptions.otelHTTPLogOptions.url 
                        + programOptions.otelHTTPLogOptions.suffix;
        using providerPtr
            = otel::nostd::shared_ptr<opentelemetry::logs::LoggerProvider>;
        auto exporter
              = otel::exporter::otlp::OtlpHttpLogRecordExporterFactory::Create(httpOptions);
        auto processor
            = otel::sdk::logs::SimpleLogRecordProcessorFactory::Create(
                 std::move(exporter));
        loggerProvider
            = otel::sdk::logs::LoggerProviderFactory::Create(
                std::move(processor));
        std::shared_ptr<opentelemetry::logs::LoggerProvider> apiProvider = loggerProvider;
        otel::logs::Provider::SetLoggerProvider(apiProvider);

        auto otelLogger
            = std::make_shared<spdlog::sinks::opentelemetry_sink_mt> ();
        logger
            = std::make_shared<spdlog::logger>
              (spdlog::logger ("OTelLogger", {otelLogger, consoleSink}));
    }
    else
    {
        auto consoleSink
            = std::make_shared<spdlog::sinks::stdout_color_sink_mt> (); 
        logger
            = std::make_shared<spdlog::logger>
              (spdlog::logger ("", {consoleSink}));
    }
    return logger;
}

void cleanupLogger()
{
    if (loggerProvider)
    {
        loggerProvider->ForceFlush();
        loggerProvider.reset();
        std::shared_ptr<opentelemetry::logs::LoggerProvider> none;
        opentelemetry::logs::Provider::SetLoggerProvider(none);
    }
}

}
#endif
