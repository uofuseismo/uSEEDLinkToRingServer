#include <string>
#include <algorithm>
#include "uSEEDLinkToRingServer/dataLinkClientOptions.hpp"

using namespace USEEDLinkToRingServer;

class DataLinkClientOptions::DataLinkClientOptionsImpl
{
public:
    std::string mHost{"localhost"};
    std::string mName{"seedLinkToRingServerDALIClient"};
    int mMaximumInternalQueueSize{8192}; 
    int mMiniSEEDRecordSize{512};
    uint16_t mPort{16000};
    bool mWriteMiniSEED3{false};
};

/// Constructor
DataLinkClientOptions::DataLinkClientOptions() :
    pImpl(std::make_unique<DataLinkClientOptionsImpl> ())
{
}

/// Copy constructor
DataLinkClientOptions::DataLinkClientOptions(
    const DataLinkClientOptions &options)
{
    *this = options;
}

/// Move constructor
DataLinkClientOptions::DataLinkClientOptions(
    DataLinkClientOptions &&options) noexcept
{
    *this = std::move(options);
}

/// Destructor
DataLinkClientOptions::~DataLinkClientOptions() = default;

/// Copy assignment
DataLinkClientOptions& 
DataLinkClientOptions::operator=(const DataLinkClientOptions &options)
{
    if (&options == this){return *this;}
    pImpl = std::make_unique<DataLinkClientOptionsImpl> (*options.pImpl);
    return *this;
}

/// Move assignment
DataLinkClientOptions& 
DataLinkClientOptions::operator=(DataLinkClientOptions &&options) noexcept
{
    if (&options == this){return *this;}
    pImpl = std::move(options.pImpl);
    return *this;
}

/// Host
void DataLinkClientOptions::setHost(const std::string &hostIn)
{
    auto host = hostIn;
    host.erase(std::remove(host.begin(), host.end(), ' '), host.end());
    std::transform(host.begin(), host.end(), host.begin(), ::tolower);
    if (host.empty())
    {
        throw std::invalid_argument("Host is empty");
    }
    if (host.size() >= 100)
    {
        throw std::invalid_argument("Host name is too long");
    }
    pImpl->mHost = host;
}

std::string DataLinkClientOptions::getHost() const
{
    return pImpl->mHost;
}

/// MSEED3
void DataLinkClientOptions::enableWriteMiniSEED3() noexcept
{
    pImpl->mWriteMiniSEED3 = true;
}

void DataLinkClientOptions::disableWriteMiniSEED3() noexcept
{
    pImpl->mWriteMiniSEED3 = false;
}

bool DataLinkClientOptions::writeMiniSEED3() const noexcept
{
    return pImpl->mWriteMiniSEED3;
}

/// Port
void DataLinkClientOptions::setPort(const uint16_t port) noexcept 
{
    pImpl->mPort = port;
}

uint16_t DataLinkClientOptions::getPort() const noexcept
{
    return pImpl->mPort;
}

/// Application name
void DataLinkClientOptions::setName(const std::string &name)
{
    if (name.empty())
    {
        throw std::invalid_argument("Name is empty");
    }
    if (name.size() >= 200)
    {
        pImpl->mName = name.substr(0, 199);
    }
    else
    { 
        pImpl->mName = name;
    }
}

std::string DataLinkClientOptions::getName() const noexcept
{
    return pImpl->mName;
}

// Record size
void DataLinkClientOptions::setMiniSEEDRecordSize(const int size)
{
    if (size < 1 || size > 512)
    {
        throw std::invalid_argument("Output MiniSEED record size "
                                  + std::to_string(size)
                                  + " must be in range [1,512]");
    }
    pImpl->mMiniSEEDRecordSize = size;
}

int DataLinkClientOptions::getMiniSEEDRecordSize() const noexcept
{
    return pImpl->mMiniSEEDRecordSize;
}

/// Max queue size
int DataLinkClientOptions::getMaximumInternalQueueSize() const noexcept
{
    return pImpl->mMaximumInternalQueueSize;
}

