/**
 * @file EventManager.cpp
 * @brief イベントの購読(Subscribe)と発行(Publish)を管理するクラスの実装
 */
#include "EventManager.h"

namespace IrufemiEngine
{
    void EventManager::Subscribe(const std::string& eventName, EventCallback callback)
    {
        listeners_[eventName].push_back(callback);
    }

    void EventManager::Publish(const std::string& eventName)
    {
        auto it = listeners_.find(eventName);
        if (it != listeners_.end())
        {
            for (auto& callback : it->second)
            {
                if (callback)
                {
                    callback();
                }
            }
        }
    }

    void EventManager::Clear()
    {
        listeners_.clear();
    }

} // namespace IrufemiEngine
