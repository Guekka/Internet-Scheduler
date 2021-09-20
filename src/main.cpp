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

constexpr bool verbose = true;
void log(std::string_view str) {
    if (!verbose)
        return;
    std::clog << str << '\n';
}

struct TimePoint {
    std::chrono::sys_days day =
        std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now());
    std::chrono::hours hour;
    std::chrono::minutes min;
    std::chrono::seconds sec;

    auto operator<=>(const TimePoint&) const = default;
};

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
    log("disabling internet");
    for (auto el : interfaces)
        IpReleaseAddress(&el);
}

void enable_internet(const std::vector<IP_ADAPTER_INDEX_MAP>& interfaces) {
    log("enabling internet");
    for (auto el : interfaces)
        IpRenewAddress(&el);
}
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

    State get_state(TimePoint time) const {
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

    Day::State get_state(TimePoint time) const {
        const auto type = get_type(time.day);
        const auto& day = days_.at(type);
        return day.get_state(time);
    }

    DayType get_type(std::chrono::sys_days day) const {
        if (day == std::chrono::Saturday || day == std::chrono::Sunday)
            return DayType::WeekEnd;
        return DayType::Week;

        // TODO Holiday
    }
};

using namespace std::chrono_literals;

// Note : UTC time used

const Day default_weekend_day = {std::vector{
    Period{TimePoint{.hour = 5h, .min = 30min, .sec = 0s},
           TimePoint{.hour = 10h, .min = 0min, .sec = 0s}},
    Period{TimePoint{.hour = 12h, .min = 0min, .sec = 0s},
           TimePoint{.hour = 18h, .min = 00min, .sec = 0s}},
}};

const Day default_holiday_day = default_weekend_day;

const Day default_week_day = {std::vector{
    Period{TimePoint{.hour = 4h, .min = 30min, .sec = 0s},
           TimePoint{.hour = 20h, .min = 0min, .sec = 0s}},
}};

const Schedule default_schedule = {std::map<Schedule::DayType, Day>{
    {Schedule::DayType::Holiday, default_holiday_day},
    {Schedule::DayType::WeekEnd, default_weekend_day},
    {Schedule::DayType::Week, default_week_day},
}};

class InternetSwitch
{
    Day::State state_{Day::Allowed};
    Schedule schedule_;

   public:
    InternetSwitch(Schedule sched) : schedule_(std::move(sched)) {}

    void update(TimePoint cur_time,
                const std::vector<IP_ADAPTER_INDEX_MAP> &interfaces,
                bool force = false)
    {
        auto new_state = schedule_.get_state(cur_time);
        if (new_state == state_ && !force)
            return;
        log("state changed " + std::to_string(static_cast<int>(new_state)));

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

TimePoint get_current_time() {
    using namespace std::chrono;
    auto raw_time = system_clock::now();
    std::time_t timet = system_clock::to_time_t(raw_time);
    std::tm* time = std::gmtime(&timet);
    return {floor<days>(raw_time), hours(time->tm_hour),
            std::chrono::minutes(time->tm_min),
            std::chrono::seconds(time->tm_sec)};
};

int main(int argc, char **argv)
{
    const auto interfaces = list_adapters();

    InternetSwitch iswitch(default_schedule);
    iswitch.update(get_current_time(), interfaces, true);

    Timer::Poller poll(std::chrono::seconds(1), [&] {
        iswitch.update(get_current_time(), interfaces, true);
    });

    return 0;
}
