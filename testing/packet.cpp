#include <iostream>
#include <vector>
#include <numeric>
#include <set>
#include <map>
#include <cmath>
#include <string>
#include <chrono>
#include <limits>
#include "uSEEDLinkToRingServer/packet.hpp"
#include "uSEEDLinkToRingServer/streamIdentifier.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
    
TEST_CASE("USEEDLinkToRingServer::StreamIdentifier", "[streamIdentifier]")
{
    SECTION("Has location code")
    {
        const std::string network{"UU"};
        const std::string station{"FTU"};
        const std::string channel{"HHN"}; 
        const std::string locationCode{"01"};
        USEEDLinkToRingServer::StreamIdentifier identifier;

        REQUIRE_NOTHROW(identifier.setNetwork(network));
        REQUIRE_NOTHROW(identifier.setStation(station));
        REQUIRE_NOTHROW(identifier.setChannel(channel));
        REQUIRE_NOTHROW(identifier.setLocationCode(locationCode));

        REQUIRE(identifier.getNetwork() == network);
        REQUIRE(identifier.getStation() == station);
        REQUIRE(identifier.getChannel() == channel);
        REQUIRE(identifier.getLocationCode() == locationCode);
        REQUIRE(identifier.toString() == "UU.FTU.HHN.01");
        REQUIRE(identifier.getStringReference() == "UU.FTU.HHN.01");
        REQUIRE(USEEDLinkToRingServer::toDataLinkIdentifier(identifier) ==
                "UU_FTU_01_HHN/MSEED");
    }
    SECTION("Does not have location code")
    {   
        const std::string network{"UU"};
        const std::string station{"FTU"};
        const std::string channel{"HHN"}; 
        const std::string locationCode{""};
        USEEDLinkToRingServer::StreamIdentifier identifier;

        REQUIRE_NOTHROW(identifier.setNetwork(network));
        REQUIRE_NOTHROW(identifier.setStation(station));
        REQUIRE_NOTHROW(identifier.setChannel(channel));
        REQUIRE_NOTHROW(identifier.setLocationCode(locationCode));

        REQUIRE(identifier.getNetwork() == network);
        REQUIRE(identifier.getStation() == station);
        REQUIRE(identifier.getChannel() == channel);
        REQUIRE(identifier.getLocationCode() == locationCode);
        REQUIRE(identifier.toString() == "UU.FTU.HHN");
        REQUIRE(identifier.getStringReference() == "UU.FTU.HHN");
        REQUIRE(USEEDLinkToRingServer::toDataLinkIdentifier(identifier) ==
                "UU_FTU__HHN/MSEED");
    }   

}

TEST_CASE("USEEDLinkToRingServer::Packet", "[packet]")
{
    using namespace USEEDLinkToRingServer;
    const std::string network{"UU"};
    const std::string station{"FTU"};
    const std::string channel{"HHN"};
    const std::string locationCode{"01"};
    const double samplingRate{100};
    const std::chrono::nanoseconds startTime{1759952887000000000};
    StreamIdentifier identifier;

    REQUIRE_NOTHROW(identifier.setNetwork(network));
    REQUIRE_NOTHROW(identifier.setStation(station));
    REQUIRE_NOTHROW(identifier.setChannel(channel));
    REQUIRE_NOTHROW(identifier.setLocationCode(locationCode));

    Packet packet;
    REQUIRE_NOTHROW(packet.setStreamIdentifier(identifier));
    REQUIRE_NOTHROW(packet.setSamplingRate(samplingRate));
    REQUIRE_NOTHROW(packet.setStartTime(startTime));

    REQUIRE(packet.getStreamIdentifierReference().getNetwork() == network);
    REQUIRE(packet.getStreamIdentifier().getNetwork() == network);
    REQUIRE(std::abs(packet.getSamplingRate() - samplingRate) < 1.e-14);
    REQUIRE(packet.getStartTime() == startTime);

    SECTION("double start time")
    {
        packet.setStartTime(startTime.count()*1.e-9);
        REQUIRE(packet.getStartTime() == startTime);
    }

    SECTION("integer32")
    {
        std::vector<int> data{1, 2, 3, -4};
        auto dataTemp = data;
        REQUIRE_NOTHROW(packet.setData(std::move(dataTemp)));
        REQUIRE(packet.getEndTime().count() == startTime.count() + 30000000);
        REQUIRE(packet.getDataType() ==
                USEEDLinkToRingServer::Packet::DataType::Integer32);
        auto dataBack = packet.getData<int> ();
        REQUIRE(dataBack.size() == data.size());
        REQUIRE(packet.getNumberOfSamples() == static_cast<int> (data.size()));
        for (int i = 0; i < static_cast<int> (data.size()); ++i)
        {
            REQUIRE(dataBack.at(i) == data.at(i));
        }
        REQUIRE(packet.getNumberOfSamples() == static_cast<int> (data.size()));
        std::vector<DataLinkPacket> dlPackets;
        constexpr USEEDLinkToRingServer::Compression
            compression{USEEDLinkToRingServer::Compression::None};
        REQUIRE_NOTHROW(dlPackets = USEEDLinkToRingServer::toDataLinkPackets(packet, 512, true, compression));
        REQUIRE(std::abs(computeSumOfSamples(packet) - 2) < 1.e-14);
        REQUIRE(std::abs(computeSumOfSamplesSquared(packet) - 30) < 1.e-14);
    }

    SECTION("double")
    {
        std::vector<double> data{-4, 2, 3, 1};
        REQUIRE_NOTHROW(packet.setData(data));
        REQUIRE(packet.getEndTime().count() == startTime.count() + 30000000);
        REQUIRE(packet.getDataType() ==
                USEEDLinkToRingServer::Packet::DataType::Double);
        auto dataBack = packet.getData<double> ();
        REQUIRE(dataBack.size() == data.size());
        REQUIRE(packet.getNumberOfSamples() == static_cast<int> (data.size()));
        for (int i = 0; i < static_cast<int> (data.size()); ++i)
        {
            REQUIRE(std::abs(dataBack.at(i) - data.at(i)) < 1.e-14);
        }
        std::vector<DataLinkPacket> dlPackets;
        constexpr USEEDLinkToRingServer::Compression
            compression{USEEDLinkToRingServer::Compression::None};
        REQUIRE_NOTHROW(dlPackets = USEEDLinkToRingServer::toDataLinkPackets(packet, 512, true, compression));
        REQUIRE(std::abs(computeSumOfSamples(packet) - 2) < 1.e-14);
        REQUIRE(std::abs(computeSumOfSamplesSquared(packet) - 30) < 1.e-14);
    }

    SECTION("float")
    {
        std::vector<float> data{-4, 1, 2, 3};
        REQUIRE_NOTHROW(packet.setData(data));
        REQUIRE(packet.getEndTime().count() == startTime.count() + 30000000);
        REQUIRE(packet.getDataType() ==
                USEEDLinkToRingServer::Packet::DataType::Float);
        auto dataBack = packet.getData<float> ();
        REQUIRE(dataBack.size() == data.size());
        REQUIRE(packet.getNumberOfSamples() == static_cast<int> (data.size()));
        for (int i = 0; i < static_cast<int> (data.size()); ++i)
        {
            REQUIRE(std::abs(dataBack.at(i) - data.at(i)) < 1.e-7);
        }
        std::vector<DataLinkPacket> dlPackets;
        constexpr USEEDLinkToRingServer::Compression
            compression{USEEDLinkToRingServer::Compression::None};
        REQUIRE_NOTHROW(dlPackets = USEEDLinkToRingServer::toDataLinkPackets(packet, 512, true, compression));
        REQUIRE(std::abs(computeSumOfSamples(packet) - 2) < 1.e-14);
        REQUIRE(std::abs(computeSumOfSamplesSquared(packet) - 30) < 1.e-14);
    }

    SECTION("text")
    {   
        std::vector<char> data{'a', 'b', 'c', 'd'};
        REQUIRE_NOTHROW(packet.setData(data));
        //REQUIRE(packet.getEndTime().count() == startTime.count() + 30000000);
        REQUIRE(packet.getDataType() ==
                USEEDLinkToRingServer::Packet::DataType::Text);
        auto dataBack = packet.getData<char> (); 
        REQUIRE(dataBack.size() == data.size());
        REQUIRE(packet.getNumberOfSamples() == static_cast<int> (data.size()));
        for (int i = 0; i < static_cast<int> (data.size()); ++i)
        {
            REQUIRE(dataBack.at(i) == data.at(i));
        }
        std::vector<DataLinkPacket> dlPackets;
        constexpr USEEDLinkToRingServer::Compression
            compression{USEEDLinkToRingServer::Compression::None};
        REQUIRE_NOTHROW(dlPackets = USEEDLinkToRingServer::toDataLinkPackets(packet, 512, true, compression));
    }   

    SECTION("Big Record")
    {
        std::vector<int> data(1024);
        std::iota(data.begin(), data.end(), 0);
        REQUIRE_NOTHROW(packet.setData(data));
        std::vector<DataLinkPacket> dlPackets;
        constexpr USEEDLinkToRingServer::Compression
            compression{USEEDLinkToRingServer::Compression::None};
        REQUIRE_NOTHROW(dlPackets = USEEDLinkToRingServer::toDataLinkPackets(packet, 512, true, compression));
    }
}
