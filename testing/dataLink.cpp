#include <string>
#include "uSEEDLinkToRingServer/dataLinkClientOptions.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

TEST_CASE("USEEDLinkToRingServer::DataLink", "[clientOptions]")
{
    namespace USR = USEEDLinkToRingServer;
    USR::DataLinkClientOptions clientOptions;
    SECTION("Defaults")
    {
        REQUIRE(clientOptions.getHost() == "localhost");
        REQUIRE(clientOptions.getPort() == 16000);
        REQUIRE(clientOptions.getName() == "seedLinkToRingServerDALIClient");
        REQUIRE(clientOptions.writeMiniSEED3() == false);
        REQUIRE(clientOptions.getMiniSEEDRecordSize() == 512);
        REQUIRE(clientOptions.flushPackets() == true);
    }
    const std::string host("127.0.0.1");
    const uint16_t port{1284};
    const std::string name{"abc"};
    int maxSize{412};
    clientOptions.setHost(host);
    clientOptions.setPort(port);
    clientOptions.setName(name);
    clientOptions.setMiniSEEDRecordSize(maxSize);
    clientOptions.enableWriteMiniSEED3();
    clientOptions.disablePacketFlushing();

    REQUIRE(clientOptions.getHost() == host);
    REQUIRE(clientOptions.getPort() == port);
    REQUIRE(clientOptions.getName() == name);
    REQUIRE(clientOptions.writeMiniSEED3() == true);
    REQUIRE(clientOptions.getMiniSEEDRecordSize() == maxSize);
    REQUIRE(clientOptions.flushPackets() == false);

    SECTION("Copy")
    {
        const USR::DataLinkClientOptions copy{clientOptions};
        REQUIRE(copy.getHost() == host);
        REQUIRE(copy.getPort() == port);
        REQUIRE(copy.getName() == name);
        REQUIRE(copy.writeMiniSEED3() == true);
        REQUIRE(copy.getMiniSEEDRecordSize() == maxSize);
        REQUIRE(copy.flushPackets() == false);
    }
}
