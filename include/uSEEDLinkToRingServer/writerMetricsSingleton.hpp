#ifndef USEEDLINK_TO_RINGSERVER_WRITER_METRICS_HPP
#define USEEDLINK_TO_RINGSERVER_WRITER_METRICS_HPP
#include <atomic>
#include <cstdint>
#include <chrono>
#include <mutex>

namespace USEEDLinkToRingServer
{

class WriterMetricsSingleton
{
public:
    [[maybe_unused]] static WriterMetricsSingleton &getInstance();
    
    void incrementPacketsWrittenCounter();

    [[nodiscard]] int64_t getPacketsWrittenCount() const noexcept;

    void incrementInvalidPacketsCounter();

    [[nodiscard]] int64_t getInvalidPacketsCount() const noexcept;

    void incrementFailedPacketsSentCounter();

    [[nodiscard]] int64_t getFailedPacketsSentCount() const noexcept;

    void incrementFailedPacketsFailedToEnqueueCounter();

    [[nodiscard]] int64_t getFailedPacketsFailedToEnqueueCount() const noexcept;

private:
    WriterMetricsSingleton() = default;
    ~WriterMetricsSingleton() = default;
    std::atomic<int64_t> mPacketsWrittenCounter{0};
    std::atomic<int64_t> mInvalidPacketsCounter{0};
    std::atomic<int64_t> mFailedPacketsSentCounter{0};
    std::atomic<int64_t> mPacketsFailedToEnqueueCounter{0};
};

void initializeWriterMetricsSingleton();

}
#endif
