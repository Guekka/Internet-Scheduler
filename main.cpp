/* Copyright (C) 2021 Edgar B
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */


#define WIN32_LEAN_AND_MEAN

#define NOGDICAPMASKS
#define NOVIRTUALKEYCODES
#define NOWINMESSAGES
#define NOWINSTYLES
#define NOSYSMETRICS
#define NOMENUS
#define NOICONS
#define NOKEYSTATES
#define NOSYSCOMMANDS
#define NORASTEROPS
#define NOSHOWWINDOW
#define OEMRESOURCE
#define NOATOM
#define NOCLIPBOARD
#define NOCOLOR
#define NOCTLMGR
#define NODRAWTEXT
#define NOGDI
#define NOKERNEL
#define NOUSER
#define NONLS
#define NOMB
#define NOMEMMGR
#define NOMETAFILE
#define NOMINMAX
#define NOMSG
#define NOOPENFILE
#define NOSCROLL
#define NOSERVICE
#define NOSOUND
#define NOTEXTMETRIC
#define NOWH
#define NOWINOFFSETS
#define NOCOMM
#define NOKANJI
#define NOHELP
#define NOPROFILER
#define NODEFERWINDOWPOS
#define NOMCX

// Has to be first
#include <winsock2.h>

#include <iphlpapi.h>

#include <chrono>
#include <iostream>
#include <map>
#include <vector>

#include "timer.hpp"

/*
constexpr ULONG default_buffer_size = 15000;
std::vector<IP_ADAPTER_INDEX_MAP> list_adapters()
{
    const auto family = AF_INET;
    //   family = AF_INET6;

    // Set the flags to pass to GetAdaptersAddresses
    const auto flags = GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_INCLUDE_PREFIX;

    const auto list = []() noexcept -> std::vector<IP_ADAPTER_ADDRESSES> {
        auto buf_size = default_buffer_size;
        auto adresses = std::vector<IP_ADAPTER_ADDRESSES>(default_buffer_size);
        auto ret = GetAdaptersAddresses(family, flags, NULL, adresses.data(), &buf_size);
        if (ret == ERROR_SUCCESS) {
            return adresses;
        }

        if (ret == ERROR_BUFFER_OVERFLOW) {
            adresses.resize(buf_size);
            ret = GetAdaptersAddresses(family, flags, NULL, adresses.data(), &buf_size);
        }

        if (ret == ERROR_SUCCESS) {
            return adresses;
        }
        return {};
    }();

    std::vector<IP_ADAPTER_INDEX_MAP> result;

    auto address = list.data();
    while (address) {
        result.emplace_back(GetInterfaceInfo(address));
        address = address->Next;
    }

    return result;
}
*/
std::vector<IP_ADAPTER_INDEX_MAP> list_adapters()
{
    // Declare and initialize variables
    ULONG ulOutBufLen = 0;

    // Make an initial call to GetInterfaceInfo to get
    // the necessary size in the ulOutBufLen variable
    auto dwRetVal = GetInterfaceInfo(NULL, &ulOutBufLen);
    std::vector<IP_INTERFACE_INFO> infos(ulOutBufLen);

    // Make a second call to GetInterfaceInfo to get
    // the actual data we need
    dwRetVal = GetInterfaceInfo(infos.data(), &ulOutBufLen);
    if (dwRetVal == NO_ERROR) {
        printf("Number of Adapters: %ld\n\n", infos.data()->NumAdapters);
        std::vector<IP_ADAPTER_INDEX_MAP> addresses(infos.data()->NumAdapters);
        for (int i = 0; i < infos.data()->NumAdapters; i++) {
            addresses.emplace_back(infos.data()->Adapter[i]);
        }
        return addresses;
    } else {
        printf("GetInterfaceInfo failed with error: %lu\n", dwRetVal);
        return {};
    }
}

void disable_internet(const std::vector<IP_ADAPTER_INDEX_MAP> &interfaces)
{
    for (auto el : interfaces)
        IpReleaseAddress(&el);
}

void enable_internet(const std::vector<IP_ADAPTER_INDEX_MAP> &interfaces)
{
    for (auto el : interfaces)
        IpRenewAddress(&el);
}

struct TimePoint
{
    std::chrono::hours hour;
    std::chrono::minutes minute;
    std::chrono::seconds second;

    auto operator<=>(const TimePoint &) const = default;
};

struct Period
{
    TimePoint start;
    TimePoint end;
};

class Day
{
    std::vector<Period> allowed_;

public:
    enum State { Allowed, Forbidden };

    Day(std::vector<Period> allowed) : allowed_(std::move(allowed)) {}

    State get_state(TimePoint time)
    {
        for (const auto &per : allowed_) {
            if (time >= per.start && time <= per.end) {
                return State::Allowed;
            }
        }
        return State::Forbidden;
    }
};

class Schedule
{
public:
    enum DayType { Week, WeekEnd, Holiday };

    std::map<DayType, Day> days_;
};

using namespace std::chrono_literals;

const Day default_weekend_day = {std::vector<Period>{
    Period{TimePoint{7h, 30min, 0s}, TimePoint{11h, 59min, 59s}},
    Period{TimePoint{14h, 0min, 0s}, TimePoint{18h, 59min, 59s}},

}};

const Day default_holiday_day = default_weekend_day;

class InternetSwitch
{
    Day::State state_{Day::Allowed};
    Day schedule_;

public:
    InternetSwitch(Day sched) : schedule_(std::move(sched)) {}

    void update(TimePoint cur_time,
                const std::vector<IP_ADAPTER_INDEX_MAP> &interfaces,
                bool force = false)
    {
        auto new_state = schedule_.get_state(cur_time);
        if (new_state == state_ && !force)
            return;

        state_ = new_state;
        switch (state_) {
        case Day::State::Allowed:
            enable_internet(interfaces);
            break;
        case Day::State::Forbidden:
            disable_internet(interfaces);
            break;
        }
    }
};

TimePoint get_current_time()
{
    auto raw_time = std::chrono::system_clock::now();
    std::time_t timet = std::chrono::system_clock::to_time_t(raw_time);
    std::tm *time = std::localtime(&timet);
    return {std::chrono::hours(time->tm_hour),
            std::chrono::minutes(time->tm_min),
            std::chrono::seconds(time->tm_sec)};
};

int main(int argc, char **argv)
{
    const auto interfaces = list_adapters();

    InternetSwitch iswitch(default_schedule);
    iswitch.update(get_current_time(), interfaces, true);

    Timer::Poller poll(std::chrono::seconds(1),
                       [&] { iswitch.update(get_current_time(), interfaces, true); });

    return 0;
}
