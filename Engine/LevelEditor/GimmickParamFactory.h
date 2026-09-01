#pragma once
#include "BaseGimmickParam.h"
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>

class GimmickParamFactory {
public:
    using CreatorFunc = std::function<std::unique_ptr<BaseGimmickParam>()>;

    static GimmickParamFactory* GetInstance();

    // ギミックのパラメータクラスを登録する
    void Register(const std::string& type, CreatorFunc creator);

    // type に対応するパラメータクラスのインスタンスを生成する
    std::unique_ptr<BaseGimmickParam> Create(const std::string& type) const;

private:
    GimmickParamFactory() = default;
    ~GimmickParamFactory() = default;

    std::unordered_map<std::string, CreatorFunc> creators_;
};
