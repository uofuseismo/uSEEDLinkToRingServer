#ifndef METRICS_EXPORTER_HPP
#define METRICS_EXPORTER_HPP
#include <string>
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/metrics/meter_provider.h>
#include <opentelemetry/metrics/provider.h>
#include <opentelemetry/exporters/otlp/otlp_http.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_options.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_options.h>
#include <opentelemetry/sdk/metrics/meter_context.h>
#include <opentelemetry/sdk/metrics/meter_context_factory.h>
#include <opentelemetry/sdk/metrics/meter_provider.h>
#include <opentelemetry/sdk/metrics/meter_provider_factory.h>
#include <opentelemetry/sdk/metrics/provider.h>
#include <opentelemetry/sdk/metrics/view/instrument_selector_factory.h>
#include <opentelemetry/sdk/metrics/view/meter_selector_factory.h>

namespace
{

void initializeMetrics(const ::ProgramOptions &programOptions)
{
    if (!programOptions.exportMetrics){return;}
    namespace otel = opentelemetry;
    otel::exporter::otlp::OtlpHttpMetricExporterOptions exporterOptions;
    exporterOptions.url = programOptions.otelHTTPMetricsOptions.url
                        + programOptions.otelHTTPMetricsOptions.suffix;
    exporterOptions.content_type
        = otel::exporter::otlp::HttpRequestContentType::kBinary;

    auto exporter
        = otel::exporter::otlp::OtlpHttpMetricExporterFactory::Create(
             exporterOptions);

    // Initialize and set the global MeterProvider
    otel::sdk::metrics::PeriodicExportingMetricReaderOptions readerOptions;
    readerOptions.export_interval_millis
        = programOptions.otelHTTPMetricsOptions.exportInterval;
    readerOptions.export_timeout_millis
        = programOptions.otelHTTPMetricsOptions.exportTimeOut;

    auto reader
        = otel::sdk::metrics::PeriodicExportingMetricReaderFactory::Create(
             std::move(exporter),
             readerOptions);

    auto context = otel::sdk::metrics::MeterContextFactory::Create();
    context->AddMetricReader(std::move(reader));

    auto metricsProvider
        = otel::sdk::metrics::MeterProviderFactory::Create(
             std::move(context));
    std::shared_ptr<otel::metrics::MeterProvider>
        provider(std::move(metricsProvider));

    otel::sdk::metrics::Provider::SetMeterProvider(provider);
}

/*
void initializeMetrics(const std::string &prometheusURL)
{
    opentelemetry::exporter::metrics::PrometheusExporterOptions
        prometheusOptions;
    prometheusOptions.url = prometheusURL;
    auto prometheusExporter
        = opentelemetry::exporter::metrics::PrometheusExporterFactory::Create(
              prometheusOptions);

    // Initialize and set the global MeterProvider
    auto providerInstance 
        = opentelemetry::sdk::metrics::MeterProviderFactory::Create();
    auto *meterProvider
        = static_cast<opentelemetry::sdk::metrics::MeterProvider *>
          (providerInstance.get());
    meterProvider->AddMetricReader(std::move(prometheusExporter));

    std::shared_ptr<opentelemetry::metrics::MeterProvider>
        provider(std::move(providerInstance));
    opentelemetry::sdk::metrics::Provider::SetMeterProvider(provider);
}
*/

void cleanupMetrics()
{
     std::shared_ptr<opentelemetry::metrics::MeterProvider> none;
     opentelemetry::sdk::metrics::Provider::SetMeterProvider(none);
}
}

#endif
