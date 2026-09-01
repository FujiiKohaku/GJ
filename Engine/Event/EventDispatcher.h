#pragma once
#include <string>
#include <unordered_map>
#include <vector>

class IEventReceiver;

class EventDispatcher {
public:
    static EventDispatcher* GetInstance();
    static void Finalize();

    void RegisterListener(const std::string& eventName, IEventReceiver* receiver);
    void UnregisterListener(const std::string& eventName, IEventReceiver* receiver);
    void UnregisterAll(IEventReceiver* receiver);

    void Dispatch(const std::string& eventName);

private:
    EventDispatcher() = default;
    ~EventDispatcher() = default;

    EventDispatcher(const EventDispatcher&) = delete;
    EventDispatcher& operator=(const EventDispatcher&) = delete;

    static EventDispatcher* instance_;
    std::unordered_map<std::string, std::vector<IEventReceiver*>> listeners_;
};
