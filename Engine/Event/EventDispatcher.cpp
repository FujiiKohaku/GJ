#include "EventDispatcher.h"
#include "IEventReceiver.h"
#include <algorithm>

EventDispatcher* EventDispatcher::instance_ = nullptr;

EventDispatcher* EventDispatcher::GetInstance()
{
    if (instance_ == nullptr) {
        instance_ = new EventDispatcher();
    }
    return instance_;
}

void EventDispatcher::Finalize()
{
    delete instance_;
    instance_ = nullptr;
}

void EventDispatcher::RegisterListener(const std::string& eventName, IEventReceiver* receiver)
{
    if (!receiver) return;
    auto& list = listeners_[eventName];
    if (std::find(list.begin(), list.end(), receiver) == list.end()) {
        list.push_back(receiver);
    }
}

void EventDispatcher::UnregisterListener(const std::string& eventName, IEventReceiver* receiver)
{
    auto it = listeners_.find(eventName);
    if (it != listeners_.end()) {
        auto& list = it->second;
        list.erase(std::remove(list.begin(), list.end(), receiver), list.end());
    }
}

void EventDispatcher::UnregisterAll(IEventReceiver* receiver)
{
    for (auto& pair : listeners_) {
        auto& list = pair.second;
        list.erase(std::remove(list.begin(), list.end(), receiver), list.end());
    }
}

void EventDispatcher::Dispatch(const std::string& eventName)
{
    auto it = listeners_.find(eventName);
    if (it != listeners_.end()) {
        auto listCopy = it->second;
        for (IEventReceiver* receiver : listCopy) {
            if (receiver) {
                receiver->OnEventReceived(eventName);
            }
        }
    }
}
