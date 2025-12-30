#include <iostream>
#include <string>
#include <array>
#include <mutex>
#include <chrono>
#include <condition_variable>
#ifndef NDEBUG
#include <cassert>
#endif
#include <readerwriterqueue.h>
#include <libdali.h>
#include <libmseed.h>
#include <spdlog/spdlog.h>
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
    explicit DataLinkClientImpl(const DataLinkClientOptions &options) :
        mOptions(options)
    {
        mWriteMiniSEED3 = mOptions.writeMiniSEED3();
        mMaxMiniSEEDRecordSize = mOptions.getMiniSEEDRecordSize();
        mMaximumInternalQueueSize = mOptions.getMaximumInternalQueueSize();
        mQueue
            = std::make_unique<moodycamel::ReaderWriterQueue<Packet>>
              (mMaximumInternalQueueSize);
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
        spdlog::info("Connecting to DataLink server at " + mAddress);
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
        spdlog::debug("Connected to DataLink server!");
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
            spdlog::debug("Disconnecting...");
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
        spdlog::debug("Thread entering packet writer");
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
                spdlog::warn("Currently not connected");
                for (const auto &waitFor : mReconnectInterval)
                {
                    auto lockTimeOut = waitFor;
                    spdlog::info("Will attempt to reconnect in "
                               + std::to_string(waitFor.count()) + " seconds");
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
                        spdlog::warn("Failed to connect: "
                                   + std::string {e.what()});
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
            if (mQueue->try_dequeue(packet))
            {
                // Make a miniseed packet
                std::vector<DataLinkPacket> dataLinkPackets;
                try
                {
                    dataLinkPackets
                        = toDataLinkPackets(packet,
                                            mMaxMiniSEEDRecordSize,
                                            mWriteMiniSEED3,
                                            mCompression);
                }
                catch (const std::exception &e)
                {
                    MeasurementFetcher::mObservablePacketsWritten.fetch_add(1);
                    MeasurementFetcher::mObservableInvalidPackets.fetch_add(1);
                    spdlog::warn("Failed to convert packet to mseed"); 
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
                    spdlog::warn("Failed to create stream name "
                               + std::string {e.what()});
                    continue;
                }
                // Write it
                for (auto &dataLinkPacket : dataLinkPackets)
                {
                    if (dataLinkPacket.data.empty())
                    {
                        spdlog::warn("Skipping empty packet");
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
                        spdlog::warn("DataLink failed to write packet for "
                                   + streamIdentifier + ".  Failed with " 
                                   + std::to_string(returnCode));
                        if (consecutiveWriteFailures >= 32)
                        {
                            spdlog::warn(
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
        auto approximateQueueSize = mQueue->size_approx();
        if (approximateQueueSize >= mMaximumInternalQueueSize)
        {
            spdlog::warn("SEEDLink thread popping elements from export queue");
            while (mQueue->size_approx() >= mMaximumInternalQueueSize)
            {
                MeasurementFetcher::mObservablePacketsFailedToEnqueue.fetch_add(1);
                mQueue->pop();
            }
        }
        // Enqueue 
        if (!mQueue->try_enqueue(std::move(packet)))
        {
            MeasurementFetcher::mObservablePacketsFailedToEnqueue.fetch_add(1);
            spdlog::warn("Failed to add packet to export queue");
        }
    }
//private:
    std::mutex mConditionVariableMutex;
    std::mutex mMutex;
    std::condition_variable mConditionVariable;
    DataLinkClientOptions mOptions;
    std::unique_ptr<moodycamel::ReaderWriterQueue<Packet>> 
        mQueue{nullptr};
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
DataLinkClient::DataLinkClient(const DataLinkClientOptions &options) :
    pImpl(std::make_unique<DataLinkClientImpl> (options))
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
