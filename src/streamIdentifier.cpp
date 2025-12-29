#include <string>
#include <algorithm>
#include "uSEEDLinkToRingServer/streamIdentifier.hpp"

using namespace USEEDLinkToRingServer;

namespace
{

/// @result True indicates that the string is empty or full of blanks.
[[maybe_unused]] [[nodiscard]]
bool isEmpty(const std::string &s) 
{
    if (s.empty()){return true;}
    return std::all_of(s.begin(), s.end(), [](const char c)
                       {
                           return std::isspace(c);
                       });
}
/// @result True indicates that the string is empty or full of blanks.
[[maybe_unused]] [[nodiscard]]
bool isEmpty(const std::string_view &s) 
{
    if (s.empty()){return true;}
    return std::all_of(s.begin(), s.end(), [](const char c)
                       {
                           return std::isspace(c);
                       });
}

[[nodiscard]] std::string convertString(const std::string_view &s) 
{
    std::string temp{s};
    temp.erase(std::remove(temp.begin(), temp.end(), ' '), temp.end());
    std::transform(temp.begin(), temp.end(), temp.begin(), ::toupper);
    return temp;
}

}

class StreamIdentifier::StreamIdentifierImpl
{
public:
    void setString()
    {
        mString.clear();
        if (!mNetwork.empty() &&
            !mStation.empty() &&
            !mChannel.empty() &&
            mHasLocationCode)
        {
            mString = mNetwork + "."
                    + mStation + "."
                    + mChannel;
            if (!mLocationCode.empty())
            {
                mString = mString + "." + mLocationCode;
            }
        }
    }
    std::string mNetwork;
    std::string mStation;
    std::string mChannel;
    std::string mLocationCode;   
    std::string mString;
    bool mHasLocationCode{false};
};

/// Constructor
StreamIdentifier::StreamIdentifier() :
    pImpl(std::make_unique<StreamIdentifierImpl> ())
{
}

/// Constructor
StreamIdentifier::StreamIdentifier(
    const std::string_view &network,
    const std::string_view &station,
    const std::string_view &channel,
    const std::string_view &locationCode)
{
    StreamIdentifier temp;
    temp.setNetwork(network);
    temp.setStation(station);
    temp.setChannel(channel);
    temp.setLocationCode(locationCode);
    *this = std::move(temp);
}

/// Copy constructor
StreamIdentifier::StreamIdentifier(const StreamIdentifier &identifier)
{
    *this = identifier;
}

/// Move constructor
StreamIdentifier::StreamIdentifier(StreamIdentifier &&identifier) noexcept
{
    *this = std::move(identifier);
}

/// Copy assignment
StreamIdentifier& 
StreamIdentifier::operator=(const StreamIdentifier &identifier)
{
    if (&identifier == this){return *this;}
    pImpl = std::make_unique<StreamIdentifierImpl> (*identifier.pImpl);
    return *this;
}

/// Move assignment
StreamIdentifier& 
StreamIdentifier::operator=(StreamIdentifier &&identifier) noexcept
{
    if (&identifier == this){return *this;}
    pImpl = std::move(identifier.pImpl);
    return *this;
}

/// Reset class
void StreamIdentifier::clear() noexcept
{
    pImpl->mNetwork.clear();
    pImpl->mStation.clear();
    pImpl->mChannel.clear();
    pImpl->mLocationCode.clear();
    pImpl->setString();
}

/// Destructor
StreamIdentifier::~StreamIdentifier() = default;

/// Network
void StreamIdentifier::setNetwork(const std::string_view &network)
{
    auto s = ::convertString(network);
    if (::isEmpty(s)){throw std::invalid_argument("Network is empty");}
    pImpl->mNetwork = std::move(s);
    pImpl->setString();
}

std::string StreamIdentifier::getNetwork() const
{
    if (!hasNetwork()){throw std::runtime_error("Network not set yet");}
    return pImpl->mNetwork;
}

bool StreamIdentifier::hasNetwork() const noexcept
{
    return !pImpl->mNetwork.empty();
}

/// Station
void StreamIdentifier::setStation(const std::string_view &station)
{
    auto s = ::convertString(station);
    if (::isEmpty(s)){throw std::invalid_argument("Station is empty");}
    pImpl->mStation = std::move(s);
    pImpl->setString();
}

std::string StreamIdentifier::getStation() const
{
    if (!hasStation()){throw std::runtime_error("Station not set yet");}
    return pImpl->mStation;
}

bool StreamIdentifier::hasStation() const noexcept
{
    return !pImpl->mStation.empty();
}

/// Channel
void StreamIdentifier::setChannel(const std::string_view &channel)
{
    auto s = ::convertString(channel);
    if (::isEmpty(s)){throw std::invalid_argument("Channel is empty");}
    pImpl->mChannel = std::move(s);
    pImpl->setString();
}

std::string StreamIdentifier::getChannel() const
{
    if (!hasChannel()){throw std::runtime_error("Channel not set yet");}
    return pImpl->mChannel;
}

bool StreamIdentifier::hasChannel() const noexcept
{
    return !pImpl->mChannel.empty();
}

/// Location code
void StreamIdentifier::setLocationCode(const std::string_view &locationCode)
{
    auto s = ::convertString(locationCode);
    if (::isEmpty(locationCode))
    {
        pImpl->mLocationCode = "";
    }
    else
    {
        pImpl->mLocationCode = std::move(s);
    }
    pImpl->mHasLocationCode = true;
    pImpl->setString();
}

std::string StreamIdentifier::getLocationCode() const
{
    if (!hasLocationCode())
    {
        throw std::runtime_error("Location code not set yet");
    }   
    return pImpl->mLocationCode;
}

bool StreamIdentifier::hasLocationCode() const noexcept
{
    return pImpl->mHasLocationCode;
}

/// To name
std::string StreamIdentifier::toString() const
{
    if (pImpl->mString.empty())
    {
        if (!hasNetwork()){throw std::runtime_error("Network not set");}
        if (!hasStation()){throw std::runtime_error("Station not set");}
        if (!hasChannel()){throw std::runtime_error("Channel not set");}
        if (!hasLocationCode())
        {
            throw std::runtime_error("Location code not set");
        }
    }
    return pImpl->mString;
}

const std::string &StreamIdentifier::getStringReference() const
{
    if (pImpl->mString.empty())
    {   
        if (!hasNetwork()){throw std::runtime_error("Network not set");}
        if (!hasStation()){throw std::runtime_error("Station not set");}
        if (!hasChannel()){throw std::runtime_error("Channel not set");}
        if (!hasLocationCode())
        {   
            throw std::runtime_error("Location code not set");
        }   
    }   
    return *&pImpl->mString;
}

std::string USEEDLinkToRingServer::toDataLinkIdentifier(
    const StreamIdentifier &streamIdentifier)
{
    auto result = streamIdentifier.getNetwork() + "_"
                + streamIdentifier.getStation() + "_"
                + streamIdentifier.getLocationCode() + "_"
                + streamIdentifier.getChannel() + "/MSEED";
    return result;
}

bool USEEDLinkToRingServer::operator<(const StreamIdentifier &lhs,
                                      const StreamIdentifier &rhs)
{
    return lhs.getStringReference() < rhs.getStringReference();
}

bool USEEDLinkToRingServer::operator==(const StreamIdentifier &lhs,
                                       const StreamIdentifier &rhs)
{
    return lhs.getStringReference() == rhs.getStringReference();
}
