#ifndef USEED_LINK_TO_RING_SERVER_DATA_LINK_CLIENT_OPTIONS_HPP
#define USEED_LINK_TO_RING_SERVER_DATA_LINK_CLIENT_OPTIONS_HPP
#include <memory>
#include <future>
namespace USEEDLinkToRingServer
{
/// @class DataLinkClientOptions "dataLinkClientOptions.hpp"
/// @brief Defines the options for influencing the behavior of the datalink
///        client.  Note, this client actually sends the MiniSEED data to
///        the RingServer.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class DataLinkClientOptions
{
public:
    /// @name Constructors
    /// @{

    /// @brief Constructor.
    DataLinkClientOptions();
    /// @brief Copy constructor.
    /// @param[in] options  The options from which to initialize this class.
    DataLinkClientOptions(const DataLinkClientOptions &options);
    /// @brief Move constructor.
    /// @param[in,out] options  The options from which to initialize this class.
    ///                         On exit, option's behavior is undefined.
    DataLinkClientOptions(DataLinkClientOptions &&options) noexcept;
    /// @}

    /// @name Operators
    /// @{

    /// @brief Copy assignment operator.
    /// @param[in] options  The options class to copy to this.
    /// @result A deep copy of the options.
    DataLinkClientOptions& operator=(const DataLinkClientOptions &options);
    /// @brief Move assignment operator.
    /// @param[in,out] options  The options class whose memory will be moved
    ///                         to this.  On exit, option's behavior is
    ///                         undefined.
    /// @result The memory from options moved to this.
    DataLinkClientOptions& operator=(DataLinkClientOptions &&options) noexcept;
    /// @}

    /// @name Properties
    /// @{

    /// @brief Sets the host of the RingServer - e.g., localhost.
    /// @param[in] hots  The host address of the RingServer.
    void setHost(const std::string &host);
    /// @result The host address of the RingServer server.
    /// @note By default this is localhost.
    [[nodiscard]] std::string getHost() const;

    /// @brief Sets the client identifier.
    void setIdentifier(const std::string &identifier);
    /// @result The client identiifer.
    [[nodiscard]] std::string getIdentifier() const;

    /// @brief Sets the port number of the RingServer.
    /// @param[in] port  The port of the server.
    void setPort(uint16_t port) noexcept;
    /// @result The port number of the RingServer server.  By default this 16000.
    [[nodiscard]] uint16_t getPort() const noexcept;

    /// @brief Packets are read from SEEDLink, metrics are computed, then
    ///        put into an internal queue for writing via DataLink.
    ///        This sets the maximum internal queue size.  After this point,
    ///        the oldest packets are popped from the queue.  This caching
    ///        can be very helpful if the DataLink connection is temporarily
    ///        severed.
    /// @param[in] maximumQueueSize  The maximum queue size in number of
    ///                              packets.
    /// @throws std::invalid_argument if this is not positive.
    void setMaximumInternalQueueSize(int maximumQueueSize);
    /// @result The maximum internal queue size in packets.
    [[nodiscard]] int getMaximumInternalQueueSize() const noexcept;

    /// @brief Specifies the size in bytes of the MiniSEED records.
    /// @param[in] recordSize  The MiniSEED record size to write.
    /// @throws std::invalid_argument if not positive or exceeds 512.
    void setMiniSEEDRecordSize(int recordSize);
    /// @result The SEED record size in bytes.  By default this is 512.
    [[nodiscard]] int getMiniSEEDRecordSize() const noexcept;

    /// @brief This will enable writing MSEED3 packets.
    void enableWriteMiniSEED3() noexcept;
    /// @brief This will fall back to writing MSEED2 packets.
    void disableWriteMiniSEED3() noexcept; 
    /// @result True indicates we will write miniseed3 packets to the RingServer.
    /// @note The default is false so as to indicate we are writing MSEED2.
    [[nodiscard]] bool writeMiniSEED3() const noexcept;

    /// @brief Sets the name of the DALI client.
    void setName(const std::string &name);
    /// @result The name of the DALI client.
    [[nodiscard]] std::string getName() const noexcept;
    /// @}

    /// @name Destructors
    /// @{

    /// @brief Resets the class and releases memory.
    void clear() noexcept;
    /// @brief Destructor.
    ~DataLinkClientOptions();
    /// @}
private:
    class DataLinkClientOptionsImpl;
    std::unique_ptr<DataLinkClientOptionsImpl> pImpl;
};

}
#endif
