#pragma once

struct UserCmd;

namespace Fakelag
{
    void run(const UserCmd* cmd, bool& sendPacket) noexcept;
    inline int latest_chocked_packets{};
}
