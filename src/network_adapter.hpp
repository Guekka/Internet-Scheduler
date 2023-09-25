#pragma once

#include <cstddef>
#include <vector>

class NetworkAdapter {
    virtual void enable_internet() = 0;
    virtual void disable_internet() = 0;
};
