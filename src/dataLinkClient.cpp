#include <iostream>
#include <string>
#include <array>
#include <mutex>
#include <chrono>
#include <condition_variable>
#ifndef NDEBUG
#include <cassert>
#endif
#ifdef USE_TBB
#include <oneapi/tbb/concurrent_queue.h>
#else
#include <concurrentqueue.h>
#endif
#include <libdali.h>
#include <libmseed.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "uSEEDLinkToRingServer/dataLinkClient.hpp"
#include "uSEEDLinkToRingServer/dataLinkClientOptions.hpp"
#include "uSEEDLinkToRingServer/packet.hpp"
#include "uSEEDLinkToRingServer/streamIdentifier.hpp"
#include "writerMetrics.hpp"

using namespace USEEDLinkToRingServer;

namespace
{
static void spdlogHandler(const char *msg)
{
    if (msg == nullptr){return;}
    std::string message(msg);
    spdlog::info(msg);
}

}

class DataLinkClient::DataLinkClientImpl
{
public:
    /// Constructor
    DataLinkClientImpl(
        const DataLinkClientOptions &options,
        std::shared_ptr<spdlog::logger> logger) :
        mOptions(options),
        mLogger(logger)
    {
        if (mLogger == nullptr)
        {
            mLogger = spdlog::stdout_color_mt("DataLinkConsole");
        }
        mWriteMiniSEED3 = mOptions.writeMiniSEED3();
        mMaxMiniSEEDRecordSize = mOptions.getMiniSEEDRecordSize();
        mMaximumInternalQueueSize = mOptions.getMaximumInternalQueueSize();
#ifdef USE_TBB
        mQueue.set_capacity(mMaximumInternalQueueSize);
#else
        mQueue
            = std::make_unique<moodycamel::ConcurrentQueue<Packet>>
              (mMaximumInternalQueueSize);
#endif
        connect();
    }
    /// Destructor
    ~DataLinkClientImpl()
    {
        stop();
        destroyDataLinkClient();
    }
    /// Creates the client
    void createClient()
    {   
        mAddress = mOptions.getHost() + ":" 
                 + std::to_string(mOptions.getPort());
        auto addressCopy = mAddress;
        auto clientNameCopy = mOptions.getName();
        mDataLinkClient = dl_newdlcp(addressCopy.data(),
                                     clientNameCopy.data());
        mDataLinkClient->iotimeout = mTimeOut.count();
        mDataLinkClient->keepalive = mHeartbeatInterval.count();
    }
    /// Connect
    void connect()
    {
        if (!mDataLinkClient){createClient();}
        disconnect();
        SPDLOG_LOGGER_INFO(mLogger, "Connecting to DataLink server at {}",
                            mAddress);
        if (dl_connect(mDataLinkClient) < 0)
        {
            if (mDataLinkClient)
            {
                dl_freedlcp(mDataLinkClient);
                mDataLinkClient = nullptr;
            }
            throw std::runtime_error("Failed to connect to DataLink client at "
                                   + mClientName + " at " + mAddress);
        }
        SPDLOG_LOGGER_DEBUG(mLogger, "Connected to DataLink server!");
    }
    /// Connected?
    [[nodiscard]] bool isConnected() const noexcept
    {
        if (mDataLinkClient)
        {
            if (mDataLinkClient->link !=-1){return true;}
        }
        return false;
    }
    /// Terminates the connection
    void disconnect()
    {
        if (isConnected())
        {
            SPDLOG_LOGGER_DEBUG(mLogger, "Datalink disconnecting...");
            dl_disconnect(mDataLinkClient);
        }
    }
    /// Destroys the client
    void destroyDataLinkClient()
    {
        disconnect();
        if (mDataLinkClient){dl_freedlcp(mDataLinkClient);}
        mDataLinkClient = nullptr;
    }
    /// Stop the publisher thread
    void stop()
    {
        mKeepRunning.store(false);
        mTerminateRequested = true;
        mConditionVariable.notify_all();
    }
    /// Start the publisher thread
    std::future<void> start()
    {
        stop();
        mKeepRunning = true;
        mTerminateRequested = false;
        auto result = std::async(&DataLinkClientImpl::runWriter, this);
        return result;
    }
    /// Writes the packets
    void runWriter()
    {
        //dl_loginit(3, &spdlogHandler, "", &spdlogHandler, ""); 
#ifndef NDEBUG
        assert(mDataLinkClient != nullptr);
#endif
        SPDLOG_LOGGER_DEBUG(mLogger, "Thread entering packet writer");
        constexpr std::chrono::milliseconds timeOut{15};
        constexpr std::chrono::seconds refreshMetricsInterval{60};
        std::chrono::microseconds lastRefresh{0};
        int consecutiveWriteFailures{0};
        while (mKeepRunning)
        {
            auto now = ::getNow();
            if (now > lastRefresh + refreshMetricsInterval)
            {
                lastRefresh = now; 
            } 
            // Test my connection and, if necessary, reconnect
            if (!isConnected())
            {
                SPDLOG_LOGGER_WARN(mLogger, "Currently not connected");
                for (const auto &waitFor : mReconnectInterval)
                {
                    auto lockTimeOut = waitFor;
                    SPDLOG_LOGGER_INFO(mLogger,
                        "Will attempt to reconnect in {} seconds", 
                        std::to_string(waitFor.count()));
                    {
                    std::unique_lock<std::mutex>
                        conditionVariableLock(mConditionVariableMutex);
                    mConditionVariable.wait_for(conditionVariableLock,
                                                lockTimeOut,
                                                [this]
                                                {
                                                    return mTerminateRequested;
                                                });
                    }
                    try
                    {
                        connect(); // Connects or throws
                        break; // I win
                    }
                    catch (const std::exception &e)
                    {
                        SPDLOG_LOGGER_WARN(mLogger,
                                   "Failed to connect because {}",
                                   std::string {e.what()});
                    }
                }
                if (!isConnected())
                {
                    throw std::runtime_error(
                        "Could not reconnect after "
                      + std::to_string(mReconnectInterval.size())
                      + " attempts");
                }
            }
            // Presumably we're connected - let's rip
            Packet packet;
#ifdef USE_TBB
            if (mQueue.try_pop(packet))
#else
            if (mQueue->try_dequeue(packet))
#endif
            {
                // Make a miniseed packet
                std::vector<DataLinkPacket> dataLinkPackets;
                try
                {
                    dataLinkPackets
                        = toDataLinkPackets(packet,
                                            mMaxMiniSEEDRecordSize,
                                            mWriteMiniSEED3,
                                            mCompression,
                                            mLogger);
                }
                catch (const std::exception &e)
                {
                    MeasurementFetcher::mObservablePacketsWritten.fetch_add(1);
                    MeasurementFetcher::mObservableInvalidPackets.fetch_add(1);
                    SPDLOG_LOGGER_WARN(mLogger,
                                       "Failed to convert packet to mseed"); 
                    continue; 
                }
                // DataLink stream identifier
                std::string streamIdentifier;
                try
                {
                    const auto streamIdentifierReference
                        = packet.getStreamIdentifierReference();
                    streamIdentifier
                        = toDataLinkIdentifier(
                             streamIdentifierReference);
                }
                catch (const std::exception &e)
                {
                    MeasurementFetcher::mObservablePacketsWritten.fetch_add(1);
                    MeasurementFetcher::mObservableInvalidPackets.fetch_add(1);
                    SPDLOG_LOGGER_WARN(mLogger,
                                   "Failed to create stream name because {}",
                                    std::string {e.what()});
                    continue;
                }
                // Write it
                for (auto &dataLinkPacket : dataLinkPackets)
                {
                    if (dataLinkPacket.data.empty())
                    {
                        SPDLOG_LOGGER_WARN(mLogger, "Skipping empty packet");
                        continue;
                    }
                    dltime_t startTime = dataLinkPacket.startTime;
                    dltime_t endTime = dataLinkPacket.endTime;
                    constexpr int writeAcknowledgement{0};
                    auto returnCode
                        = dl_write(mDataLinkClient,
                                   dataLinkPacket.data.data(),
                                   dataLinkPacket.data.size(),
                                   streamIdentifier.data(),
                                   startTime,
                                   endTime,
                                   writeAcknowledgement);
                    if (returnCode < 0)
                    {
                        consecutiveWriteFailures = consecutiveWriteFailures + 1;
                        MeasurementFetcher::mObservablePacketsFailedToWrite.fetch_add(1);
                        SPDLOG_LOGGER_WARN(mLogger,
                      "DataLink failed to write packet for {}.  Failed with {}",
                            streamIdentifier, returnCode);
                        if (consecutiveWriteFailures >= 32)
                        {
                            SPDLOG_LOGGER_ERROR(mLogger,
                               "DataLink too many consecutive write failures - killing connection");
                            disconnect();
                        }
                    }
                    else
                    { 
                        consecutiveWriteFailures = 0;
                        MeasurementFetcher::mObservablePacketsWritten.fetch_add(1);
                    }
                }
            }
            else
            {
                std::this_thread::sleep_for(timeOut);
            }
        }
    }
    /// Enqueues the packet
    void enqueue(Packet &&packet)
    {
#ifdef USE_TBB
        auto approximateQueueSize = mQueue.size();
#else
        auto approximateQueueSize = mQueue->size_approx();
#endif
        if (approximateQueueSize >= mMaximumInternalQueueSize)
        {
            SPDLOG_LOGGER_WARN(mLogger,
                "SEEDLink thread popping elements from export queue");
#ifdef USE_TBB
            while (mQueue.size() >= mMaximumInternalQueueSize)
#else
            while (mQueue->size_approx() >= mMaximumInternalQueueSize)
#endif
            {
                MeasurementFetcher::mObservablePacketsFailedToEnqueue.fetch_add(1);
                Packet workSpace;
#ifdef USE_TBB
                if (!mQueue.try_pop(workSpace))
#else
                if (!mQueue->try_dequeue(workSpace))
#endif
                {
                    SPDLOG_LOGGER_ERROR(mLogger,
                       "Failed to dequeue packet in overfull queue");
                }
            }
        }
        // Enqueue 
#ifdef USE_TBB
        if (!mQueue.try_push(std::move(packet)))
#else
        if (!mQueue->try_enqueue(std::move(packet)))
#endif
        {
            MeasurementFetcher::mObservablePacketsFailedToEnqueue.fetch_add(1);
            SPDLOG_LOGGER_WARN(mLogger,
               "Failed to add packet to export queue - queue may be full");
        }
    }
//private:
    DataLinkClientOptions mOptions;
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    std::mutex mConditionVariableMutex;
    std::mutex mMutex;
    std::condition_variable mConditionVariable;
#ifdef USE_TBB
    oneapi::tbb::concurrent_bounded_queue<Packet> mQueue;
#else
    std::unique_ptr<moodycamel::ConcurrentQueue<Packet>> mQueue{nullptr};
#endif
    DLCP *mDataLinkClient{nullptr};
    std::string mClientName{"daliClient"};
    std::string mAddress;
    std::array<char, MAXPACKETSIZE> mBuffer;
    std::vector<std::chrono::seconds> mReconnectInterval
    {
        std::chrono::seconds {0},
        std::chrono::seconds {5},
        std::chrono::seconds {30},
        std::chrono::seconds {60}
    };
    std::chrono::seconds mTimeOut{1};
    std::chrono::seconds mHeartbeatInterval{5};
    int mMaxMiniSEEDRecordSize{512};
    int mMaximumInternalQueueSize{8192};
    //std::atomic<uint64_t> mPacketsFailedToEnqueue{0};
    //std::atomic<uint64_t> mInvalidPackets{0};
    //std::atomic<uint64_t> mPacketsFailedToWrite{0};
    std::atomic<bool> mKeepRunning{true};
    Compression mCompression{Compression::None};
    bool mWriteMiniSEED3{true};
    bool mTerminateRequested{false};
};

/// Constructor
DataLinkClient::DataLinkClient(
    const DataLinkClientOptions &options,
    std::shared_ptr<spdlog::logger> logger) :
    pImpl(std::make_unique<DataLinkClientImpl> (options, logger))
{
}

/// Destructor
DataLinkClient::~DataLinkClient() = default;

/// Start the writer thread
std::future<void> DataLinkClient::start()
{
    return pImpl->start();
}

/// Allow a thread to enqueue a packet
void DataLinkClient::enqueue(Packet &&packet)
{
    pImpl->enqueue(std::move(packet));
}

void DataLinkClient::enqueue(const Packet &packet)
{
    auto copy = packet;
    enqueue(std::move(copy));
}

/// Stop the writer thread
void DataLinkClient::stop()
{
    pImpl->stop();
}
