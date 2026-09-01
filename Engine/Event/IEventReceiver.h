#pragma once
#include <string>

class IEventReceiver {
public:
    virtual ~IEventReceiver() = default;
    virtual void OnEventReceived(const std::string& eventName) = 0;
};
