#include <string>
#include "uSEEDLinkToRingServer/version.hpp"

using namespace USEEDLinkToRingServer;

int Version::getMajor() noexcept
{
    return uSEEDLinkToRingServer_MAJOR;
}

int Version::getMinor() noexcept
{
    return uSEEDLinkToRingServer_MINOR;
}

int Version::getPatch() noexcept
{
    return uSEEDLinkToRingServer_PATCH;
}

bool Version::isAtLeast(const int major, const int minor,
                        const int patch) noexcept
{
    if (uSEEDLinkToRingServer_MAJOR < major){return false;}
    if (uSEEDLinkToRingServer_MAJOR > major){return true;}
    if (uSEEDLinkToRingServer_MINOR < minor){return false;}
    if (uSEEDLinkToRingServer_MINOR > minor){return true;}
    if (uSEEDLinkToRingServer_PATCH < patch){return false;}
    return true;
}

std::string Version::getVersion() noexcept
{
    std::string version{uSEEDLinkToRingServer_VERSION};
    return version;
}

