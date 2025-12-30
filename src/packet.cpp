#include <iostream>
#include <numeric>
#include <string>
#include <vector>
#include <chrono>
#include <cmath>
#include <spdlog/spdlog.h>
#include <libmseed.h>
#include <libslink.h>
#ifndef NDEBUG
#include <cassert>
#endif
#include "uSEEDLinkToRingServer/packet.hpp"
#include "uSEEDLinkToRingServer/streamIdentifier.hpp"

using namespace USEEDLinkToRingServer;

namespace
{

void msRecordHandler(char *record, int recordLength, void *buffer)
{
    // Awkward but I need the start/end times to send this.
    constexpr uint32_t flags{0};
    constexpr int8_t verbose{0};
    MS3Record *miniSEEDRecord{nullptr};
    auto returnCode = msr3_parse(record,
                                 recordLength,
                                 &miniSEEDRecord,
                                 flags,
                                 verbose);
    int64_t startTime{0};
    int64_t endTime{0};
    if (returnCode == MS_NOERROR && miniSEEDRecord)
    {
        startTime = miniSEEDRecord->starttime;
        endTime = msr3_endtime(miniSEEDRecord);
    }
    if (miniSEEDRecord){msr3_free(&miniSEEDRecord);}
    if (MS_NOERROR)
    {
        spdlog::warn("Error decoding packet");
    }

    std::string nextRecord(record, record + recordLength);
    auto outputPackets
        = reinterpret_cast<std::vector<DataLinkPacket> *> (buffer);
    DataLinkPacket packet
    {
       std::move(nextRecord),
       startTime,
       endTime
    }; 
    outputPackets->push_back(std::move(packet));
}    

}


class Packet::PacketImpl
{
public:
    [[nodiscard]] int size() const
    {   
        if (mDataType == Packet::DataType::Unknown)
        {
            return 0;
        }
        else if (mDataType == Packet::DataType::Integer32)
        {   
            return static_cast<int> (mInteger32Data.size());
        }   
        else if (mDataType == Packet::DataType::Float)
        {
            return static_cast<int> (mFloatData.size());
        }
        else if (mDataType == Packet::DataType::Double)
        {
            return static_cast<int> (mDoubleData.size());
        }
        else if (mDataType == Packet::DataType::Text)
        {
            return static_cast<int> (mTextData.size());
        }
#ifndef NDEBUG
        assert(false);
#else
        throw std::runtime_error("Unhandled data type in packet impl size");
#endif
    }
    void clearData()
    {
        mInteger32Data.clear();
        mFloatData.clear();
        mDoubleData.clear();
        mTextData.clear();
        mDataType = Packet::DataType::Unknown;
    }
    void setData(std::vector<int> &&data)
    {
        if (data.empty()){return;}
        clearData();
        mInteger32Data = std::move(data);
        mDataType = Packet::DataType::Integer32;
        updateEndTime();
    }
    void setData(std::vector<float> &&data)
    {
        if (data.empty()){return;}
        clearData();
        mFloatData = std::move(data);
        mDataType = Packet::DataType::Float;
        updateEndTime();
    }
    void setData(std::vector<double> &&data)
    {
        if (data.empty()){return;}
        clearData();
        mDoubleData = std::move(data);
        mDataType = Packet::DataType::Double;
        updateEndTime();
    }
    void setData(std::vector<char> &&data)
    {
        if (data.empty()){return;}
        clearData();
        mTextData = std::move(data);
        mDataType = Packet::DataType::Text;
        updateEndTime();
    }   
    void updateEndTime()
    {
        mEndTimeMicroSeconds = mStartTimeMicroSeconds;
        auto nSamples = size();  
        if (nSamples > 0 && mSamplingRate > 0)
        {
            auto traceDuration
                = std::round( ((nSamples - 1)/mSamplingRate)*1000000000 );
            auto iTraceDuration = static_cast<int64_t> (traceDuration);
            std::chrono::nanoseconds traceDurationMuS{iTraceDuration};
            mEndTimeMicroSeconds = mStartTimeMicroSeconds + traceDurationMuS;
        }
    }
    StreamIdentifier mIdentifier;
    std::vector<char> mTextData;
    std::vector<int> mInteger32Data;
    std::vector<float> mFloatData;
    std::vector<double> mDoubleData;
    std::chrono::nanoseconds mStartTimeMicroSeconds{0};
    std::chrono::nanoseconds mEndTimeMicroSeconds{0};
    double mSamplingRate{0};
    Packet::DataType mDataType{Packet::DataType::Unknown};
    bool mHasIdentifier = false;
};

/// Clear class
void Packet::clear() noexcept
{
    pImpl->clearData();
    pImpl->mIdentifier.clear();
    pImpl->mHasIdentifier = false;
    pImpl->mStartTimeMicroSeconds = std::chrono::nanoseconds {0};
    pImpl->mEndTimeMicroSeconds = std::chrono::nanoseconds {0};
    pImpl->mSamplingRate = 0;
}

/// Constructor
Packet::Packet() :
    pImpl(std::make_unique<PacketImpl> ())
{
}

/// Copy constructor
Packet::Packet(const Packet &packet)
{
    *this = packet;
}

/// Move constructor
Packet::Packet(Packet &&packet) noexcept
{
    *this = std::move(packet);
}

/// Copy assignment
Packet& Packet::operator=(const Packet &packet)
{
    if (&packet == this){return *this;}
    pImpl = std::make_unique<PacketImpl> (*packet.pImpl);
    return *this;
}

/// Move assignment
Packet& Packet::operator=(Packet &&packet) noexcept
{
    if (&packet == this){return *this;}
    pImpl = std::move(packet.pImpl);
    return *this;
}

/// Destructor
Packet::~Packet() = default;

/// Identifier
void Packet::setStreamIdentifier(const StreamIdentifier &identifier)
{
    StreamIdentifier copy{identifier};
    setStreamIdentifier(std::move(copy));
}

void Packet::setStreamIdentifier(StreamIdentifier &&identifier)
{
    if (!identifier.hasNetwork())
    { 
        throw std::invalid_argument("Network not set");
    }
    if (!identifier.hasStation())
    {
        throw std::invalid_argument("Station not set");
    }
    if (!identifier.hasChannel())
    {
        throw std::invalid_argument("Channel not set");
    }
    if (!identifier.hasLocationCode())
    {
        throw std::invalid_argument("Location code not set");
    }
    pImpl->mIdentifier = std::move(identifier);
    pImpl->mHasIdentifier = true;
}

const StreamIdentifier &Packet::getStreamIdentifierReference() const
{
    if (!hasStreamIdentifier()){throw std::runtime_error("Identifier not set");}
    return *&pImpl->mIdentifier;
}

StreamIdentifier Packet::getStreamIdentifier() const
{
    if (!hasStreamIdentifier()){throw std::runtime_error("Identifier not set");}
    return pImpl->mIdentifier;
}

bool Packet::hasStreamIdentifier() const noexcept
{
    return pImpl->mHasIdentifier;
}

/// Sampling rate
void Packet::setSamplingRate(const double samplingRate) 
{
    if (samplingRate <= 0)
    {
        throw std::invalid_argument("samplingRate = "
                                  + std::to_string(samplingRate)
                                  + " must be positive");
    }
    pImpl->mSamplingRate = samplingRate;
    pImpl->updateEndTime();
}

double Packet::getSamplingRate() const
{
    if (!hasSamplingRate()){throw std::runtime_error("Sampling rate not set");}
    return pImpl->mSamplingRate;
}

bool Packet::hasSamplingRate() const noexcept
{
    return (pImpl->mSamplingRate > 0);     
}

/// Number of samples
int Packet::getNumberOfSamples() const noexcept
{
    return pImpl->size();
}

/// Start time
void Packet::setStartTime(const double startTime) noexcept
{
    auto iStartTimeMuS = static_cast<int64_t> (std::round(startTime*1.e9));
    std::chrono::nanoseconds startTimeMuS{iStartTimeMuS};
    setStartTime(startTimeMuS);
}

void Packet::setStartTime(
    const std::chrono::nanoseconds &startTime) noexcept
{
    pImpl->mStartTimeMicroSeconds = startTime;
    pImpl->updateEndTime();
}

std::chrono::nanoseconds Packet::getStartTime() const noexcept
{
    return pImpl->mStartTimeMicroSeconds;
}

std::chrono::nanoseconds Packet::getEndTime() const
{
    if (!hasSamplingRate())
    {   
        throw std::runtime_error("Sampling rate not set");
    }   
    if (getNumberOfSamples() < 1)
    {   
        throw std::runtime_error("No samples in signal");
    }   
    return pImpl->mEndTimeMicroSeconds;
}

/// Sets the data
template<typename U>
void Packet::setData(std::vector<U> &&x)
{
    pImpl->setData(std::move(x));
    pImpl->updateEndTime();
}

template<typename U>
void Packet::setData(const std::vector<U> &x)
{
    auto xWork = x;
    setData(std::move(xWork));
}

template<typename U>
void Packet::setData(const int nSamples, const U *x)
{
    // Invalid
    if (nSamples < 0){throw std::invalid_argument("nSamples not positive");}
    if (x == nullptr){throw std::invalid_argument("x is NULL");}
    std::vector<U> data(nSamples);
    std::copy(x, x + nSamples, data.begin());
    setData(std::move(data));
}

/// Gets a reference to the underlying data
/*
const void *Packet::getDataPointer() const noexcept
{
    auto dataType = getDataType();
    if (pImpl->mDataType == Packet::DataType::Integer32)
    {   
        return pImpl->mInteger32Data.data();
    }   
    else if (dataType == Packet::DataType::Float)
    {   
        return pImpl->mFloatData.data();
    }   
    else if (dataType == Packet::DataType::Double)
    {   
        return pImpl->mDoubleData.data();
    }   
    else if (dataType == Packet::DataType::Text)
    {   
        return pImpl->mTextData.data();
    }
    else
    {
#ifndef NDEBUG
        assert(false);
#endif
    }
    return nullptr;
}
*/

/// Gets the data
template<typename U>
std::vector<U> Packet::getData() const noexcept
{
    std::vector<U> result;
    auto nSamples = getNumberOfSamples();
    if (nSamples < 1){return result;}
    result.resize(nSamples);
    auto dataType = getDataType();
    if (dataType == DataType::Integer32)
    {   
        std::copy(pImpl->mInteger32Data.begin(),
                  pImpl->mInteger32Data.end(),
                  result.begin());
    }   
    else if (dataType == DataType::Float)
    {   
        std::copy(pImpl->mFloatData.begin(),
                  pImpl->mFloatData.end(),
                  result.begin());
    }   
    else if (dataType == DataType::Double)
    {   
        std::copy(pImpl->mDoubleData.begin(),
                  pImpl->mDoubleData.end(),
                  result.begin());
    }   
    else if (dataType == DataType::Text)
    {
        std::copy(pImpl->mTextData.begin(),
                  pImpl->mTextData.end(),
                  result.begin());
    }
    else
    {   
#ifndef NDEBUG
        assert(false);
#endif
        constexpr U zero{0};
        std::fill(result.begin(), result.end(), zero); 
    }   
    return result;
}

const void* Packet::getDataPointer() const noexcept
{
    if (getNumberOfSamples() < 1){return nullptr;}
    auto dataType = getDataType();
    if (dataType == DataType::Integer32)
    {   
        return pImpl->mInteger32Data.data();
    }   
    else if (dataType == DataType::Float)
    {   
        return pImpl->mFloatData.data();
    }   
    else if (dataType == DataType::Double)
    {   
        return pImpl->mDoubleData.data();
    }   
    else if (dataType == DataType::Text)
    {
        return pImpl->mTextData.data();
    }
    else if (dataType  == DataType::Unknown)
    {   
        return nullptr;
    }   
#ifndef NDEBUG
    else 
    {   
        assert(false);
    }   
#endif
    return nullptr;
}

/// Data type
Packet::DataType Packet::getDataType() const noexcept
{
    return pImpl->mDataType;
}

std::vector<USEEDLinkToRingServer::DataLinkPacket> 
USEEDLinkToRingServer::toDataLinkPackets(
    const Packet &packet,
    const int maxRecordLength,
    const bool useMiniSEED3,
    const Compression compression)
{
    std::vector<DataLinkPacket> outputPackets;
    MS3Record msRecord MS3Record_INITIALIZER;//{nullptr};
    // Pack the easy stuff
    msRecord.datasamples = nullptr;
    try
    {
        msRecord.reclen = maxRecordLength > 0 ? maxRecordLength : 4096;
        msRecord.pubversion = 1;
        msRecord.starttime
            = static_cast<int64_t> (packet.getStartTime().count());
        msRecord.samprate = packet.getSamplingRate();
        msRecord.numsamples = packet.getNumberOfSamples();
    }
    catch (const std::exception &e)
    {
        throw std::runtime_error("Failed to pack mseed record because "
                               + std::string {e.what()});
    }
    // Pack the sid
    const auto streamIdentifier = packet.getStreamIdentifierReference();
    std::string locationCode;
    if (streamIdentifier.hasLocationCode())
    {
        locationCode = streamIdentifier.getLocationCode();
    }
    auto sidLength
        = ms_nslc2sid(
            msRecord.sid, LM_SIDLEN, 0,
            const_cast<char *> (streamIdentifier.getNetwork().c_str()),
            const_cast<char *> (streamIdentifier.getStation().c_str()),
            const_cast<char *> (locationCode.c_str()),
            const_cast<char *> (streamIdentifier.getChannel().c_str()));
    if (sidLength < 1)
    {
        throw std::runtime_error("Failed to pack SID");
    }
    // Now do the data
    int encodingInteger{-1};
    auto dataType = packet.getDataType(); 
    std::vector<double> i64Data;
    if (msRecord.numsamples > 0)
    {
        if (dataType == Packet::DataType::Integer32)
        {
            msRecord.encoding = DE_INT32;
            if (compression == Compression::STEIM1)
            {
                msRecord.encoding = DE_STEIM1;
            }
            else if (compression == Compression::STEIM2)
            {
                msRecord.encoding = DE_STEIM2;
            }
            msRecord.sampletype = 'i';
            msRecord.datasamples = (void *) packet.getDataPointer();
            //    = reinterpret_cast<void *> (pImpl->mInteger32Data.data());
        }
        else if (dataType == Packet::DataType::Float)
        {
            msRecord.encoding = DE_FLOAT32;
            msRecord.sampletype = 'f';
            msRecord.datasamples = (void *) packet.getDataPointer();
            //    = reinterpret_cast<void *> (pImpl->mFloatData.data());
        }
        else if (dataType == Packet::DataType::Double)
        {
            msRecord.encoding = DE_FLOAT64;
            msRecord.sampletype = 'd';
            msRecord.datasamples = (void *) packet.getDataPointer();
            //   = reinterpret_cast<void *> (pImpl->mDoubleData.data());
        }
        else if (dataType == Packet::DataType::Text)
        {
            msRecord.encoding = DE_TEXT;
            msRecord.sampletype = 't';
            msRecord.datasamples = (void *) packet.getDataPointer();
            //   = reinterpret_cast<void *> (pImpl->mTextData.data());
        }
        else
        {
            throw std::runtime_error("Unhandled precision");
        }
    }
    msRecord.samplecnt = msRecord.numsamples;
    // Package it
    uint32_t writeToBufferFlags{0};
    writeToBufferFlags |= MSF_FLUSHDATA;
    writeToBufferFlags |= MSF_MAINTAINMSTL; // Do not modify while packing
    if (!useMiniSEED3){writeToBufferFlags |= MSF_PACKVER2;}
    int64_t packedSamplesCount{0};
    constexpr int8_t verbose{0};
    auto nRecordsCreated = msr3_pack(&msRecord,
                                     &msRecordHandler,
                                     &outputPackets,
                                     &packedSamplesCount,
                                     writeToBufferFlags,
                                     verbose);
    msRecord.datasamples = nullptr;
    if (nRecordsCreated < 0)
    {
        throw std::runtime_error("Failed to pack miniSEED");
    }
    if (nRecordsCreated != static_cast<int> (outputPackets.size()))
    {
        spdlog::warn("Inconsistent records created/output packets created");
    }
    if (msRecord.numsamples > 0 && packedSamplesCount < msRecord.numsamples)
    { 
        spdlog::warn("Its possible not all samples were packed");
    }
    return outputPackets;
}

double USEEDLinkToRingServer::computeSumOfSamples(const Packet &packet)
{
    double result{0};
    auto nSamples = packet.getNumberOfSamples();
    if (nSamples < 1){return 0;}
    auto dataType = packet.getDataType();
    constexpr double zero{0};
    if (dataType == Packet::DataType::Integer32)
    {
        auto dPtr = static_cast<const int *> (packet.getDataPointer()); 
        result = std::accumulate(dPtr, dPtr + nSamples, zero);
    }
    else if (dataType == Packet::DataType::Double)
    {
        auto dPtr = static_cast<const double *> (packet.getDataPointer());
        result = std::accumulate(dPtr, dPtr + nSamples, zero);
    }
    else if (dataType == Packet::DataType::Float)
    {
        auto dPtr = static_cast<const float *> (packet.getDataPointer());
        result = std::accumulate(dPtr, dPtr + nSamples, zero);
    }
    else if (dataType == Packet::DataType::Text)
    {
        throw std::runtime_error("Cannot compute sum of text data");
    }
    else
    {
        throw std::runtime_error("Unhandled data type");
    }
    return result; 
}

double USEEDLinkToRingServer::computeSumOfSamplesSquared(const Packet &packet)
{
    double result{0};
    auto nSamples = packet.getNumberOfSamples();
    if (nSamples < 1){return 0;} 
    auto dataType = packet.getDataType();
    constexpr double zero{0};
    if (dataType == Packet::DataType::Integer32)
    {
        auto dPtr = static_cast<const int *> (packet.getDataPointer()); 
        result = std::inner_product(dPtr, dPtr + nSamples, dPtr, zero);
    }   
    else if (dataType == Packet::DataType::Double)
    {
        auto dPtr = static_cast<const double *> (packet.getDataPointer());
        result = std::inner_product(dPtr, dPtr + nSamples, dPtr, zero);
    }   
    else if (dataType == Packet::DataType::Float)
    {   
        auto dPtr = static_cast<const float *> (packet.getDataPointer());
        result = std::inner_product(dPtr, dPtr + nSamples, dPtr, zero);
    }   
    else if (dataType == Packet::DataType::Text)
    {   
        throw std::runtime_error("Cannot compute sum squared of text data");
    }   
    else
    {   
        throw std::runtime_error("Unhandled data type");
    }   
    return result; 
}


///--------------------------------------------------------------------------///
///                               Template Instantiation                     ///
///--------------------------------------------------------------------------///
template void USEEDLinkToRingServer::Packet::setData(const std::vector<double> &);
template void USEEDLinkToRingServer::Packet::setData(const std::vector<float> &);
template void USEEDLinkToRingServer::Packet::setData(const std::vector<int> &);
template void USEEDLinkToRingServer::Packet::setData(const std::vector<char> &);

template void USEEDLinkToRingServer::Packet::setData(std::vector<double> &&);
template void USEEDLinkToRingServer::Packet::setData(std::vector<float> &&);
template void USEEDLinkToRingServer::Packet::setData(std::vector<int> &&);
template void USEEDLinkToRingServer::Packet::setData(std::vector<char> &&);

template void USEEDLinkToRingServer::Packet::setData(const int, const double *);
template void USEEDLinkToRingServer::Packet::setData(const int, const float *);
template void USEEDLinkToRingServer::Packet::setData(const int, const int *);
template void USEEDLinkToRingServer::Packet::setData(const int, const char *);

template std::vector<int> USEEDLinkToRingServer::Packet::getData<int> () const noexcept;
template std::vector<double> USEEDLinkToRingServer::Packet::getData<double> () const noexcept;
template std::vector<float> USEEDLinkToRingServer::Packet::getData<float> () const noexcept;
template std::vector<char> USEEDLinkToRingServer::Packet::getData<char> () const noexcept;
