#include "GimmickParamFactory.h"
#include "Engine/Logger/Logger.h"

GimmickParamFactory* GimmickParamFactory::GetInstance() {
    static GimmickParamFactory instance;
    return &instance;
}

void GimmickParamFactory::Register(const std::string& type, CreatorFunc creator) {
    creators_[type] = std::move(creator);
}

std::unique_ptr<BaseGimmickParam> GimmickParamFactory::Create(const std::string& type) const {
    auto it = creators_.find(type);
    if (it != creators_.end()) {
        return it->second();
    }
    // 未登録のタイプが来たら警告を出し、nullptr を返す
    // ギミックなし（ただの壁など）の場合もここを通るため、単に nullptr を返すだけで良い
    return nullptr;
}
