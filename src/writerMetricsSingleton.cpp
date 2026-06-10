#include <iostream>
#include <atomic>
#include <cstdint>
#include <chrono>
#include <mutex>
#include <spdlog/spdlog.h>
#include "uSEEDLinkToRingServer/writerMetricsSingleton.hpp"

using namespace USEEDLinkToRingServer;

WriterMetricsSingleton &WriterMetricsSingleton::getInstance()
{
    std::mutex mutex;
    const std::scoped_lock lock{mutex};
    static WriterMetricsSingleton instance;
    return instance;
}
    
void WriterMetricsSingleton::incrementPacketsWrittenCounter()
{
    auto n = mPacketsWrittenCounter.fetch_add(1, std::memory_order_relaxed);

}

int64_t WriterMetricsSingleton::getPacketsWrittenCount() const noexcept
{
    return mPacketsWrittenCounter.load(std::memory_order_relaxed);
}

void WriterMetricsSingleton::incrementInvalidPacketsCounter()
{
    mInvalidPacketsCounter.fetch_add(1, std::memory_order_relaxed);
}

int64_t WriterMetricsSingleton::getInvalidPacketsCount() const noexcept
{   
    return mInvalidPacketsCounter.load(std::memory_order_relaxed);
}

void WriterMetricsSingleton::incrementFailedPacketsSentCounter()
{   
    mFailedPacketsSentCounter.fetch_add(1, std::memory_order_relaxed);
}   

int64_t WriterMetricsSingleton::getFailedPacketsSentCount() const noexcept
{   
    return mFailedPacketsSentCounter.load(std::memory_order_relaxed);
}   

void WriterMetricsSingleton::incrementFailedPacketsFailedToEnqueueCounter()
{   
    mPacketsFailedToEnqueueCounter.fetch_add(1, std::memory_order_relaxed);
}   

int64_t WriterMetricsSingleton::getFailedPacketsFailedToEnqueueCount()
    const noexcept
{
    return mPacketsFailedToEnqueueCounter.load(std::memory_order_relaxed);
}   

void USEEDLinkToRingServer::initializeWriterMetricsSingleton()
{
    WriterMetricsSingleton::getInstance();
}

