#ifndef USEED_LINK_TO_RING_SERVER_DATA_LINK_CLIENT_HPP
#define USEED_LINK_TO_RING_SERVER_DATA_LINK_CLIENT_HPP
#include <memory>
#include <future>
namespace USEEDLinkToRingServer
{
 class Packet;
 class DataLinkClientOptions;
}

namespace USEEDLinkToRingServer
{
/// @class DataLinkClient
/// @brief The client which writes MiniSEED data to the RingServer.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class DataLinkClient
{
public:
    explicit DataLinkClient(const DataLinkClientOptions &options);

    /// @brief Starts the publisher thread. 
    [[nodiscard]] std::future<void> start();
    /// @brief Enqueues a packet to write.
    void enqueue(Packet &&packet);
    /// @brief Enqueues a packet to write.
    void enqueue(const Packet &packet);
    /// @brief Stops the publisher thread.
    void stop();
    /// @brief Destructor.
    ~DataLinkClient();

    DataLinkClient(const DataLinkClient &) = delete;
    DataLinkClient(DataLinkClient &&) noexcept = delete;
    DataLinkClient& operator=(const DataLinkClient &) = delete;
    DataLinkClient& operator=(DataLinkClient &&) noexcept = delete;
private:
    class DataLinkClientImpl;
    std::unique_ptr<DataLinkClientImpl> pImpl;
};
}
#endif
