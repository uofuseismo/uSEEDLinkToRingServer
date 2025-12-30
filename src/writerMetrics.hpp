#ifndef WRITER_METRICS_HPP
#define WRITER_METRICS_HPP
#include <chrono>
#include <atomic>
#include <spdlog/spdlog.h>
#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/metrics/meter_provider.h>
#include <opentelemetry/metrics/provider.h>
#include <opentelemetry/sdk/metrics/view/view_factory.h>
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
            opentelemetry::nostd::get
            <
               opentelemetry::nostd::shared_ptr
               <
                   opentelemetry::metrics::ObserverResultT<int64_t>
               >
            >(observerResult)->Observe(mObservablePacketsWritten.load());
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
            opentelemetry::nostd::get
            <
               opentelemetry::nostd::shared_ptr
               <
                   opentelemetry::metrics::ObserverResultT<int64_t>
               >
            >(observerResult)->Observe(mObservableInvalidPackets.load());
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
            opentelemetry::nostd::get
            <
               opentelemetry::nostd::shared_ptr
               <
                   opentelemetry::metrics::ObserverResultT<int64_t>
               >
            >(observerResult)->Observe(mObservablePacketsFailedToWrite.load());
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
            opentelemetry::nostd::get
            <
               opentelemetry::nostd::shared_ptr
               <
                   opentelemetry::metrics::ObserverResultT<int64_t>
               >
            >(observerResult)->Observe(mObservablePacketsFailedToEnqueue.load());
        }
    }   

    static std::atomic<int64_t> mObservablePacketsWritten;
    static std::atomic<int64_t> mObservableInvalidPackets;
    static std::atomic<int64_t> mObservablePacketsFailedToWrite;
    static std::atomic<int64_t> mObservablePacketsFailedToEnqueue;
};

std::atomic<int64_t> MeasurementFetcher::mObservablePacketsWritten{0};
std::atomic<int64_t> MeasurementFetcher::mObservableInvalidPackets{0};
std::atomic<int64_t> MeasurementFetcher::mObservablePacketsFailedToWrite{0};
std::atomic<int64_t> MeasurementFetcher::mObservablePacketsFailedToEnqueue{0};

}

#endif
