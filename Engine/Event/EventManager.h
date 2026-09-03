/**
 * @file EventManager.h
 * @brief イベントの購読(Subscribe)と発行(Publish)を管理するクラス
 */
#pragma once
#include <string>
#include <functional>
#include <unordered_map>
#include <vector>

namespace IrufemiEngine
{
    /**
     * @brief 文字列ベースのイベントディスパッチャ（Observerパターン）
     * @details MapChipStage等が所有し、局所的なスコープでイベントを管理する
     */
    class EventManager
    {
    public:
        using EventCallback = std::function<void()>;

        EventManager() = default;
        ~EventManager() = default;

        /**
         * @brief イベントを購読する
         * @param eventName 待機するイベント名
         * @param callback イベント発生時に呼ばれるコールバック関数
         */
        void Subscribe(const std::string& eventName, EventCallback callback);

        /**
         * @brief イベントを発行する
         * @param eventName 発行するイベント名
         */
        void Publish(const std::string& eventName);

        /**
         * @brief 登録された全てのイベントをクリアする
         * @details ステージのリセット時などに呼び出す
         */
        void Clear();

    private:
        // イベント名をキーとしたコールバック関数のリスト
        std::unordered_map<std::string, std::vector<EventCallback>> listeners_;
    };
} // namespace IrufemiEngine
