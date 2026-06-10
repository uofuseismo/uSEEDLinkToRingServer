#ifndef WRITER_METRICS_HPP
#define WRITER_METRICS_HPP
#include <atomic>
#include <cstdint>
#include <chrono>
#include <mutex>
#include <spdlog/spdlog.h>
#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/metrics/meter_provider.h>
#include <opentelemetry/metrics/provider.h>
#include <opentelemetry/sdk/metrics/view/view_factory.h>
#include "uSEEDLinkToRingServer/writerMetricsSingleton.hpp"
#include "getNow.hpp"

namespace
{

class MeasurementFetcher
{
public:

    static void observePacketsWritten(
        opentelemetry::metrics::ObserverResult observerResult,
        void *)
    {   
        if (opentelemetry::nostd::holds_alternative
            <
                opentelemetry::nostd::shared_ptr
                <
                    opentelemetry::metrics::ObserverResultT<int64_t>
                >
            >(observerResult))
        {
            auto &metrics = USEEDLinkToRingServer::WriterMetricsSingleton::getInstance();
            auto nWritten = metrics.getPacketsWrittenCount();
            opentelemetry::nostd::get
            <
               opentelemetry::nostd::shared_ptr
               <
                   opentelemetry::metrics::ObserverResultT<int64_t>
               >
            >(observerResult)->Observe(nWritten);
        }
    }

    static void observeInvalidPackets(
        opentelemetry::metrics::ObserverResult observerResult,
        void *)
    {   
        if (opentelemetry::nostd::holds_alternative
            <   
                opentelemetry::nostd::shared_ptr
                <   
                    opentelemetry::metrics::ObserverResultT<int64_t>
                >   
            >(observerResult))
        {
            auto &metrics = USEEDLinkToRingServer::WriterMetricsSingleton::getInstance();
            auto nInvalid = metrics.getInvalidPacketsCount();
            opentelemetry::nostd::get
            <
               opentelemetry::nostd::shared_ptr
               <
                   opentelemetry::metrics::ObserverResultT<int64_t>
               >
            >(observerResult)->Observe(nInvalid);
        }
    }

    static void observePacketsFailedToWrite(
        opentelemetry::metrics::ObserverResult observerResult,
        void *)
    {
        if (opentelemetry::nostd::holds_alternative
            <
                opentelemetry::nostd::shared_ptr
                <
                    opentelemetry::metrics::ObserverResultT<int64_t>
                >
            >(observerResult))
        {
            auto &metrics = USEEDLinkToRingServer::WriterMetricsSingleton::getInstance();
            auto nNotSent = metrics.getFailedPacketsSentCount();
            opentelemetry::nostd::get
            <
               opentelemetry::nostd::shared_ptr
               <
                   opentelemetry::metrics::ObserverResultT<int64_t>
               >
            >(observerResult)->Observe(nNotSent);
        }
    }

    static void observePacketsFailedToEnqueue(
        opentelemetry::metrics::ObserverResult observerResult,
        void *)
    {   
        if (opentelemetry::nostd::holds_alternative
            <
                opentelemetry::nostd::shared_ptr
                <
                    opentelemetry::metrics::ObserverResultT<int64_t>
                >
            >(observerResult))
        {
            auto &metrics = USEEDLinkToRingServer::WriterMetricsSingleton::getInstance();
            auto nNotEnqueued = metrics.getFailedPacketsFailedToEnqueueCount();
            opentelemetry::nostd::get
            <
               opentelemetry::nostd::shared_ptr
               <
                   opentelemetry::metrics::ObserverResultT<int64_t>
               >
            >(observerResult)->Observe(nNotEnqueued);
        }
    }   

    //static std::atomic<int64_t> mObservablePacketsWritten;
    //static std::atomic<int64_t> mObservableInvalidPackets;
    //static std::atomic<int64_t> mObservablePacketsFailedToWrite;
    //static std::atomic<int64_t> mObservablePacketsFailedToEnqueue;
};

/*
std::atomic<int64_t> MeasurementFetcher::mObservablePacketsWritten{0};
std::atomic<int64_t> MeasurementFetcher::mObservableInvalidPackets{0};
std::atomic<int64_t> MeasurementFetcher::mObservablePacketsFailedToWrite{0};
std::atomic<int64_t> MeasurementFetcher::mObservablePacketsFailedToEnqueue{0};
*/

}

#endif
