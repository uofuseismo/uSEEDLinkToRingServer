#ifndef STREAM_METRICS_HPP
#define STREAM_METRICS_HPP
#include <chrono>
#include <atomic>
#include <map>
#include <spdlog/spdlog.h>
#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/metrics/meter_provider.h>
#include <opentelemetry/metrics/provider.h>
#include <opentelemetry/sdk/metrics/view/view_factory.h>
#include "uSEEDLinkToRingServer/packet.hpp"
#include "uSEEDLinkToRingServer/streamIdentifier.hpp"
#include "getNow.hpp"

namespace
{

opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument>
    mPacketsReceivedCounter;
opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument>
    mFuturePacketsReceivedCounter;
opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument>
    mExpiredPacketsReceivedCounter;
opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument>
    mTotalPacketsReceivedCounter;
opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument>
    mAverageLatencyGauge;
opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument>
    mAverageCountsGauge;
opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument>
    mStandardDeviationCountsGauge;

template<typename T>
class ObservableMap
{
public:
    /*
    void insert_or_assign(const std::string &key, const T value)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mMap.insert_or_assign(key, value);
    }
    */
    /// If the key, value pair is present then value will be added to it
    /// otherwise the key, value pair is initialized to the given value.
    void add_or_assign(const std::string &key, const T value)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        auto idx = mMap.find(key);
        if (idx != mMap.end())
        {
            idx->second = idx->second + value;
        }
        else
        {
            mMap.insert( std::pair {key, value} );
        }
    }
    [[nodiscard]] std::set<std::string> keys() const noexcept
    {
        std::set<std::string> result;
        std::lock_guard<std::mutex> lock(mMutex);
        for (const auto &item : mMap)
        {
            result.insert(item.first);
        }
        return result;
    }
    [[nodiscard]] std::optional<T> operator[](const std::string &key) const
    {
        std::lock_guard<std::mutex> lock(mMutex);
        auto idx = mMap.find(key);
        if (idx != mMap.end())
        {
            return std::make_optional(idx->second);
        }
        return std::nullopt;
    }
    [[nodiscard]] std::optional<T> at(const std::string &key) const
    {
        std::lock_guard<std::mutex> lock(mMutex);
        auto idx = mMap.find(key);
        if (idx != mMap.end())
        {
            return std::make_optional (idx->second);
        }
        return std::nullopt;
    }
    [[nodiscard]] size_t size() const noexcept
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return mMap.size();
    }
    mutable std::mutex mMutex;
    std::map<std::string, T> mMap;
};

// TODO make these thread safe and also able to be purged
::ObservableMap<int64_t> mObservablePacketsReceived;
//std::map<std::string, int64_t> mObservablePacketsReceived;
::ObservableMap<int64_t> mObservableExpiredPacketsReceived;
//std::map<std::string, int64_t> mObservableExpiredPacketsReceived;
::ObservableMap<int64_t> mObservableFuturePacketsReceived;
//std::map<std::string, int64_t> mObservableFuturePacketsReceived;
::ObservableMap<int64_t> mObservableTotalPacketsReceived;
//std::map<std::string, int64_t> mObservableTotalPacketsReceived;
std::map<std::string, double> mObservableAverageLatency;
std::map<std::string, double> mObservableAverageCounts;
std::map<std::string, double> mObservableStandardDeviationOfCounts;

void observePacketsReceived(
    opentelemetry::metrics::ObserverResult observerResult,
    void *)
{
    if (opentelemetry::nostd::holds_alternative
        <
            opentelemetry::nostd::shared_ptr
            <
                opentelemetry::metrics::ObserverResultT<int64_t>
            >   
        > (observerResult))
    {
        auto observer = opentelemetry::nostd::get
        <
            opentelemetry::nostd::shared_ptr
            <
               opentelemetry::metrics::ObserverResultT<int64_t>
            >   
        > (observerResult);
        auto keys = mObservablePacketsReceived.keys();
        for (const auto &key : keys)
        {
            try
            {
                auto value = mObservablePacketsReceived[key];
                if (value)
                {
                    std::map<std::string, std::string>
                       attribute{ {"stream", key} };
                    observer->Observe(*value, attribute);
                }
                else
                {
                    throw std::runtime_error("Could not find " + key 
                                           + " in map");
                }
            }
            catch (const std::exception &e)
            {
                spdlog::warn(e.what());
            }
        }
        /*
        for (const auto &item : mObservablePacketsReceived)
        {
            std::map<std::string, std::string> attribute;
            attribute.insert( std::pair {"stream", item.first} );
            observer->Observe(item.second, attribute);
        }
        */  
    }   
}

void observeExpiredPacketsReceived(
    opentelemetry::metrics::ObserverResult observerResult,
    void *)
{
    if (opentelemetry::nostd::holds_alternative
        <
            opentelemetry::nostd::shared_ptr
            <
                opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult))
    {
        auto observer = opentelemetry::nostd::get
        <
            opentelemetry::nostd::shared_ptr
            <
               opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult);
        auto keys = mObservableExpiredPacketsReceived.keys();
        for (const auto &key : keys)
        {
            try 
            {   
                auto value = mObservableExpiredPacketsReceived[key];
                if (value)
                {   
                    std::map<std::string, std::string>
                       attribute{ {"stream", key} };
                    observer->Observe(*value, attribute);
                }   
                else
                {   
                    throw std::runtime_error("Could not find " + key 
                                           + " in map");
                }   
            }
            catch (const std::exception &e) 
            {   
                spdlog::warn(e.what());
            }   
        }
        /*
        for (const auto &item : mObservableExpiredPacketsReceived)
        {
            std::map<std::string, std::string> attribute;
            attribute.insert( std::pair {"stream", item.first} );
            observer->Observe(item.second, attribute);
        }
        */
    }   
}

void observeFuturePacketsReceived(
    opentelemetry::metrics::ObserverResult observerResult,
    void *)
{
    if (opentelemetry::nostd::holds_alternative
        <
            opentelemetry::nostd::shared_ptr
            <
                opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult))
    {
        auto observer = opentelemetry::nostd::get
        <
            opentelemetry::nostd::shared_ptr
            <
               opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult);
        auto keys = mObservableFuturePacketsReceived.keys();
        for (const auto &key : keys)
        {
            try
            {
                auto value = mObservableFuturePacketsReceived[key];
                if (value)
                {
                    std::map<std::string, std::string>
                       attribute{ {"stream", key} };
                    observer->Observe(*value, attribute);
                }
                else
                {
                    throw std::runtime_error("Could not find " + key
                                           + " in map");
                }
            }
            catch (const std::exception &e)
            {
                spdlog::warn(e.what());
            }
        }
        /*
        for (const auto &item : mObservableFuturePacketsReceived)
        {
            std::map<std::string, std::string> attribute;
            attribute.insert( std::pair {"stream", item.first} );
            observer->Observe(item.second, attribute);
        }
        */
    }
}

void observeTotalPacketsReceived(
    opentelemetry::metrics::ObserverResult observerResult,
    void *)
{
    if (opentelemetry::nostd::holds_alternative
        <
            opentelemetry::nostd::shared_ptr
            <
                opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult))
    {
        auto observer = opentelemetry::nostd::get
        <
            opentelemetry::nostd::shared_ptr
            <
               opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult);
        auto keys = mObservableTotalPacketsReceived.keys();
        for (const auto &key : keys)
        {
            try
            {
                auto value = mObservableTotalPacketsReceived[key];
                if (value)
                {
                    std::map<std::string, std::string>
                        attribute{ {"stream", key} };
                    observer->Observe(*value, attribute);
                }
                else
                {
                    throw std::runtime_error("Could not find " + key
                                           + " in map");
                }
            }
            catch (const std::exception &e)
            {
                spdlog::warn(e.what());
            }
        }
        /*
        for (const auto &item : mObservableTotalPacketsReceived)
        {
            std::map<std::string, std::string> attribute;
            attribute.insert( std::pair {"stream", item.first} );
            observer->Observe(item.second, attribute);//item.first);
        }
        */
    }
}

void observeAverageLatency(
        opentelemetry::metrics::ObserverResult observerResult,
        void *)
{
    if (opentelemetry::nostd::holds_alternative
        <
            opentelemetry::nostd::shared_ptr
            <
                opentelemetry::metrics::ObserverResultT<double>
            >   
        >(observerResult))
    {
        auto observer = opentelemetry::nostd::get
        <
           opentelemetry::nostd::shared_ptr
           <
               opentelemetry::metrics::ObserverResultT<double>
           >   
        > (observerResult);
        for (const auto &item : mObservableAverageLatency)
        {
            std::map<std::string, std::string> attribute;
            attribute.insert(std::pair{"stream", item.first});
            observer->Observe(item.second, attribute);
        }   
    }   
}

void observeAverageCounts(
        opentelemetry::metrics::ObserverResult observerResult,
        void *)
{
    if (opentelemetry::nostd::holds_alternative
        <
            opentelemetry::nostd::shared_ptr
            <
                opentelemetry::metrics::ObserverResultT<double>
            >
        >(observerResult))
    {
        auto observer = opentelemetry::nostd::get
        <
           opentelemetry::nostd::shared_ptr
           <
               opentelemetry::metrics::ObserverResultT<double>
           >
        > (observerResult);
        for (const auto &item : mObservableAverageCounts)
        {
            std::map<std::string, std::string> attribute;
            attribute.insert(std::pair{"stream", item.first});
            observer->Observe(item.second, attribute);
        }
    }
}

void observeStandardDeviationOfAverageCounts(
        opentelemetry::metrics::ObserverResult observerResult,
        void *)
{
    if (opentelemetry::nostd::holds_alternative
        <   
            opentelemetry::nostd::shared_ptr
            <   
                opentelemetry::metrics::ObserverResultT<double>
            >   
        > (observerResult))
    {   
        auto observer = opentelemetry::nostd::get
        <   
           opentelemetry::nostd::shared_ptr
           <
               opentelemetry::metrics::ObserverResultT<double>
           >
        > (observerResult); //->Observe(mObservableStandardDeviationOfCounts.load());
        for (const auto &item : mObservableStandardDeviationOfCounts)
        {
            //std::cout << item.first << " " << item.second << std::endl;
            std::map<std::string, std::string> attribute;
            attribute.insert(std::pair{"stream", item.first});
            observer->Observe(item.second, attribute);//item.first);
        }
    }
}

void initializeImportMetrics(const std::string &applicationName)
{
    // Need a provider from which to get a meter.  This is initialized
    // once and should last the duration of the application.
    auto provider = opentelemetry::metrics::Provider::GetMeterProvider();
 
    // Meter will be bound to application (library, module, class, etc.)
    // so as to identify who is genreating these metrics.
    auto meter = provider->GetMeter(applicationName, "1.2.0");
    
    // Create the metric instruments (instruments are used to report
    // measurements)

    // Good packets
    mPacketsReceivedCounter
        = meter->CreateInt64ObservableCounter(
             "seismic_data.import.seedlink.client.packets.valid",
             "Number of valid data packets received from SEEDLink client.",
             "{packets}");
    mPacketsReceivedCounter->AddCallback(observePacketsReceived, nullptr);

    // Future packets
    mFuturePacketsReceivedCounter
        = meter->CreateInt64ObservableCounter(
             "seismic_data.import.seedlink.client.packets.future",
             "Number of future packets received from SEEDLink client.",
             "{packets}");
    mFuturePacketsReceivedCounter->AddCallback(
        observeFuturePacketsReceived, nullptr);

    // Expired packets
    mExpiredPacketsReceivedCounter
        = meter->CreateInt64ObservableCounter(
             "seismic_data.import.seedlink.client.packets.expired",
             "Number of expired packets received from SEEDLink client.",
             "{packets}");
    mExpiredPacketsReceivedCounter->AddCallback(
        observeExpiredPacketsReceived, nullptr);

    // Total packets
    mTotalPacketsReceivedCounter
        = meter->CreateInt64ObservableCounter(
             "seismic_data.import.seedlink.client.packets.all",
             "Total number of packets received from SEEDLink client.  This includes future and expired packets.",
             "{packets}");
    mTotalPacketsReceivedCounter->AddCallback(
        observeTotalPacketsReceived, nullptr);

    // Average latency
    mAverageLatencyGauge
        = meter->CreateDoubleObservableGauge(
             "seismic_data.import.seedlink.client.windowed_average_latency",
             "Average latency.",
             "{s}");
    mAverageLatencyGauge->AddCallback(observeAverageLatency, nullptr);

    // Average counts of good packets
    mAverageCountsGauge
        = meter->CreateDoubleObservableGauge(
             "seismic_data.import.seedlink.client.windowed_average",
             "Average number of counts sampled every minute.",
             "{counts}");
    mAverageCountsGauge->AddCallback(observeAverageCounts, nullptr);

    // Standard deviation of the average of the good packet counts
    mStandardDeviationCountsGauge
        = meter->CreateDoubleObservableGauge(
             "seismic_data.import.seedlink.client.windowed_standard_deviation",
             "Standard deviation of counts sampled every minute.",
             "{counts}");
    mStandardDeviationCountsGauge->AddCallback(
            observeStandardDeviationOfAverageCounts,
            nullptr);
}



class StreamMetrics
{
public:
    StreamMetrics(const std::string applicationName,
                  const USEEDLinkToRingServer::Packet &packet,
                  std::shared_ptr<spdlog::logger> logger) :
        mApplicationName(applicationName),
        mLogger(logger)
    {
        const auto streamIdentifier
            = packet.getStreamIdentifierReference();
        mName = streamIdentifier.getStringReference();
        SPDLOG_LOGGER_INFO(mLogger, "Making new metrics for {}", mName);
        mMetricsKey = streamIdentifier.getNetwork() + "_"
                    + streamIdentifier.getStation() + "_"
                    + streamIdentifier.getChannel();
        if (streamIdentifier.hasLocationCode())
        {
            auto locationCode = streamIdentifier.getLocationCode();
            if (!locationCode.empty())
            {
                mMetricsKey = mMetricsKey + "_" + locationCode;
            }
        }
        std::transform(mMetricsKey.begin(), mMetricsKey.end(),
                       mMetricsKey.begin(), ::tolower);
        mObservablePacketsReceived[mMetricsKey] = 0; 
        mObservableFuturePacketsReceived[mMetricsKey] = 0;
        mObservableExpiredPacketsReceived[mMetricsKey] = 0;
        mObservableTotalPacketsReceived[mMetricsKey] = 0;
        mObservableAverageLatency[mMetricsKey] = 0;
        mObservableAverageCounts[mMetricsKey] = 0;
        mObservableStandardDeviationOfCounts[mMetricsKey] = 0;

        spdlog::debug("Made new metrics for " + mName);
        update(packet);
    }
    void update(const USEEDLinkToRingServer::Packet &packet)
    {
        if (packet.getStreamIdentifierReference().getStringReference() !=
            mName)
        {
            throw std::runtime_error("Inconsistent names");
        }
        auto endTime
            = std::chrono::duration_cast<std::chrono::microseconds>
             (packet.getEndTime()); 
        auto now = ::getNow();
        if (endTime > mMostRecentSample && endTime <= now)
        {
            bool metricsSuccess{true};
            int nSamples{0};
            double packetSum{0};
            double packetSum2{0};
            auto dataType = packet.getDataType();
            if (dataType == USEEDLinkToRingServer::Packet::DataType::Integer32 ||
                dataType == USEEDLinkToRingServer::Packet::DataType::Double ||
                dataType == USEEDLinkToRingServer::Packet::DataType::Float)
            {
                try
                {
                    nSamples = packet.getNumberOfSamples();
                    packetSum = USEEDLinkToRingServer::computeSumOfSamples(packet);
                    packetSum2
                        = USEEDLinkToRingServer::computeSumOfSamplesSquared(packet);
                }
                catch (const std::exception &e)
                {
                    metricsSuccess = false;
                    spdlog::warn(
                       "Failed to compute packet information stats for " 
                      + mName + " because " + std::string {e.what()});
                }
            }
            now = ::getNow();
            {
            std::lock_guard<std::mutex> lock(mMutex);
            mLastUpdate = now;
            mMostRecentSample = endTime;
            mLatency = now - endTime;
            if (metricsSuccess)
            {
                mRunningPacketsCounter = mRunningPacketsCounter + 1;
                mRunningTotalPacketsCounter = mRunningTotalPacketsCounter + 1;
                mRunningSamplesCounter
                    = mRunningSamplesCounter + nSamples;
                mRunningSum = mRunningSum + packetSum;
                mRunningLatencySum = mRunningLatencySum + mLatency;
                mRunningSumSquared = mRunningSumSquared + packetSum2;
            }
            }
        }
        else if (endTime > now)
        {
            now = ::getNow();
            {
            std::lock_guard<std::mutex> lock(mMutex);
            mRunningFuturePacketsCounter = mRunningFuturePacketsCounter + 1;
            mRunningTotalPacketsCounter = mRunningTotalPacketsCounter + 1;
            mLastUpdate = now;
            }
        }
        else if (endTime < now - std::chrono::months {6})
        {
            now = ::getNow();
            {
            std::lock_guard<std::mutex> lock(mMutex);
            mRunningExpiredPacketsCounter = mRunningExpiredPacketsCounter + 1;
            mRunningTotalPacketsCounter = mRunningTotalPacketsCounter + 1;
            mLastUpdate = now;
            }
        }
        else
        {
            now = ::getNow();
            {
            std::lock_guard<std::mutex> lock(mMutex);
            mRunningTotalPacketsCounter = mRunningTotalPacketsCounter + 1;
            mLastUpdate = now;
            }
        }
    }
    void tabulateAndResetMetrics(const std::chrono::seconds &sampleInterval)
    {
        int64_t packetsCount{0};
        int64_t expiredPacketsCount{0};
        int64_t futurePacketsCount{0};
        int64_t totalPacketsCount{0};
        double averageCounts{0};
        double varianceOfCounts{0};
        double averageLatency{0};
        double besselCorrection{1}; // normalize standard deviation by 1/(n - 1)
        {
        std::lock_guard<std::mutex> lock(mMutex);
        packetsCount = mRunningPacketsCounter;
        expiredPacketsCount = mRunningExpiredPacketsCounter;
        futurePacketsCount= mRunningFuturePacketsCounter; 
        totalPacketsCount = mRunningTotalPacketsCounter;
        if (mRunningSamplesCounter > 0)
        {
            if (mRunningSamplesCounter > 1) 
            {
                besselCorrection
                    = mRunningSamplesCounter
                     /static_cast<double> (mRunningSamplesCounter - 1);
            }
            averageCounts = mRunningSum/mRunningSamplesCounter;
            // Var[x] = E[x^2] - E[x]^2
            varianceOfCounts = mRunningSumSquared/mRunningSamplesCounter
                             - averageCounts*averageCounts;
        } 
        if (mRunningPacketsCounter > 0)
        {
            averageLatency = mRunningLatencySum.count()*1.e-6
                            /mRunningPacketsCounter;
        }
        else
        {
            averageLatency = sampleInterval.count();
        }
        mRunningSum = 0;
        mRunningSumSquared = 0;
        mRunningSamplesCounter = 0;
        mRunningLatencySum = std::chrono::microseconds{0};
        mRunningPacketsCounter = 0;
        mRunningExpiredPacketsCounter = 0;
        mRunningFuturePacketsCounter = 0;
        mRunningTotalPacketsCounter = 0;
        }
        auto stdOfCounts
            = besselCorrection*std::sqrt(std::max(0.0, varianceOfCounts));
        mObservablePacketsReceived.add_or_assign(
            mMetricsKey, packetsCount); //[mMetricsKey] = packetsCount;
        mObservableFuturePacketsReceived.add_or_assign(
            mMetricsKey, futurePacketsCount);
        mObservableExpiredPacketsReceived.add_or_assign(
            mMetricsKey, expiredPacketsCount); //[mMetricsKey] = expiredPacketsCount;
        mObservableTotalPacketsReceived.add_or_assign(
            mMetricsKey, totalPacketsCount);
        mObservableAverageLatency[mMetricsKey] = averageLatency;
        mObservableAverageCounts[mMetricsKey] = averageCounts;
        mObservableStandardDeviationOfCounts[mMetricsKey] = stdOfCounts;
    }
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    std::mutex mMutex;
    std::string mApplicationName;
    std::string mName;
    std::string mMetricsKey;
    std::chrono::microseconds mLastUpdate{0};
    std::chrono::microseconds mLatency{0};
    std::chrono::microseconds mRunningLatencySum{0};
    std::chrono::microseconds mMostRecentSample{0};
    std::chrono::microseconds mCreationTime{::getNow()};
    double mRunningSum{0};
    double mRunningSumSquared{0};
    int64_t mRunningPacketsCounter{0};
    int64_t mRunningFuturePacketsCounter{0};
    int64_t mRunningExpiredPacketsCounter{0};
    int64_t mRunningTotalPacketsCounter{0};
    int64_t mRunningSamplesCounter{0};
};

class MetricsMap
{
public:
    void update(const USEEDLinkToRingServer::Packet &packet,
                std::shared_ptr<spdlog::logger> logger)
    {
        auto identifier
            = packet.getStreamIdentifierReference().getStringReference();
        auto index = mMetrics.find(identifier);
        if (index == mMetrics.end())
        {
            auto streamMetrics
                = std::make_unique<::StreamMetrics>
                  (mApplicationName, packet, logger);
            mMetrics.insert(std::pair {identifier, std::move(streamMetrics)});
        }
        else
        {
            index->second->update(packet);
        } 
    }
    void tabulateAndResetAllMetrics()
    {
        auto now = ::getNow();
        if (now > mLastSampleTime + mSampleInterval)
        {
            mLastSampleTime = now;
            for (auto &metric : mMetrics)
            {
                metric.second->tabulateAndResetMetrics(mSampleInterval);
            }
        }
    }
    std::map<std::string, std::unique_ptr<::StreamMetrics>> mMetrics;
    std::string mApplicationName{"seedLinkImport"};
    std::chrono::microseconds mLastSampleTime{::getNow()};
    std::chrono::seconds mSampleInterval{60};
};

}
#endif
