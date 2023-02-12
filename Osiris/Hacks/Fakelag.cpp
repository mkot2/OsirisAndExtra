#include <random>

#include "AntiAim.h"
#include "EnginePrediction.h"
#include "Fakelag.h"
#include "EnginePrediction.h"
#include "Tickbase.h"
#include "AntiAim.h"

#include "../SDK/Entity.h"
#include "../SDK/Localplayer.h"
#include "../SDK/NetworkChannel.h"
#include "../SDK/UserCmd.h"
#include "../SDK/Vector.h"
#undef min
#undef max

namespace Fakelag
{
    template <typename T>
    class uniform_int_random_generator
    {
        std::uniform_int_distribution<T> distribution;
        std::default_random_engine random_engine;
    public:
        explicit uniform_int_random_generator(const unsigned seed) : distribution{}, random_engine{ seed }
        {
        }

        uniform_int_random_generator(const T min, const T max, const unsigned seed) : distribution{ min, max }, random_engine{ seed }
        {
        }

        [[nodiscard]] T get()
        {
            return distribution(random_engine);
        }

        void set_range(const T min, const T max)
        {
            if (distribution.min() == min && distribution.max() == max)
                return;
            distribution = std::uniform_int_distribution{ min, max };
        }
    };
    uniform_int_random_generator<int> random{ static_cast<unsigned>(std::chrono::high_resolution_clock::now().time_since_epoch().count()) };
}

void Fakelag::run(const UserCmd* cmd, bool& sendPacket) noexcept
{
    const auto moving_flag{AntiAim::get_moving_flag(cmd) };
    if (!localPlayer || !localPlayer->isAlive())
        return;

    const auto netChannel = interfaces->engine->getNetworkChannel();
    if (!netChannel)
        return;

    if (AntiAim::getDidShoot())
    {
        sendPacket = true;
        return;
    }   
    if (EnginePrediction::getVelocity().length2D() < 1)
        return;

    random.set_range(0, 2);
    auto choked_packets = config->legitAntiAim.enabled || config->fakeAngle[static_cast<int>(moving_flag)].enabled ? random.get() : 0;

    if (config->tickbase.disabledTickbase && config->tickbase.onshotFl && config->tickbase.readyFire)
    {
        choked_packets = -1;

        latest_choked_packets = choked_packets;

        if (interfaces->engine->isVoiceRecording())
            sendPacket = netChannel->chokedPackets >= 0;
        else
            sendPacket = netChannel->chokedPackets >= choked_packets;

        config->tickbase.readyFire = false;
        return;
    }

    if (config->tickbase.disabledTickbase && config->tickbase.onshotFl && config->tickbase.lastFireShiftTick > memory->globalVars->tickCount + choked_packets)
    {
        choked_packets = 0;

        latest_choked_packets = choked_packets;

        if (interfaces->engine->isVoiceRecording())
            sendPacket = netChannel->chokedPackets >= 0;
        else
            sendPacket = netChannel->chokedPackets >= choked_packets;

        config->tickbase.readyFire = false;
        return;
    }

    if (config->fakelag[static_cast<int>(moving_flag)].enabled)
    {
        const float speed = EnginePrediction::getVelocity().length2D() >= 15.0f ? EnginePrediction::getVelocity().length2D() : 0.0f;
        switch (config->fakelag[static_cast<int>(moving_flag)].mode) {
        case 0: //Static
            choked_packets = config->fakelag[static_cast<int>(moving_flag)].limit;
            break;
        case 1: //Adaptive
            choked_packets = std::clamp(static_cast<int>(std::ceilf(64 / (speed * memory->globalVars->intervalPerTick))), 1, config->fakelag[static_cast<int>(moving_flag)].limit);
            break;
        case 2: // Random
            random.set_range(config->fakelag[static_cast<int>(moving_flag)].randomMinLimit, config->fakelag[static_cast<int>(moving_flag)].limit);
            choked_packets = random.get();
            break;
        case 3: // rand() Random
            srand(static_cast<unsigned>(time(nullptr)));
            choked_packets = rand() % config->fakelag[static_cast<int>(moving_flag)].limit + config->fakelag[static_cast<int>(moving_flag)].randomMinLimit; // NOLINT(concurrency-mt-unsafe)
            break;
        default:
            break;
        }
    }

    choked_packets = std::clamp(choked_packets, 0, maxUserCmdProcessTicks - Tickbase::getTargetTickShift());

    latest_choked_packets = choked_packets;

    if (interfaces->engine->isVoiceRecording())
        sendPacket = netChannel->chokedPackets >= 0;
    else
        sendPacket = netChannel->chokedPackets >= choked_packets;
}