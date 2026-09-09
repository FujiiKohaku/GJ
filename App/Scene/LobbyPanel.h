#pragma once
#include "Engine/2D/Sprite.h"
#include "Engine/2D/Text/Text.h"
#include <memory>
#include <vector>

// Runtime UI (also available in Release, independent of ImGui).
class LobbyPanel {
public:
    enum class Action { None, PreviousStage, NextStage, Solo, Start };
    void Initialize();
    Action Update(bool canSelectStage);
    void Draw();
    bool IsTypingName() const { return nameFocused_; }
private:
    struct Button {
        float x, y, width, height;
        std::unique_ptr<Sprite> background;
        std::unique_ptr<Text> label;
        bool visible = true, enabled = true;
    };
    void AddButton(float x, float y, float width, float height);
    void PlaceButton(size_t index, float x, float y, float width, float height);
    void SetButton(size_t index, const std::string& text, bool enabled, bool visible = true);
    std::unique_ptr<Sprite> panel_, nameField_, nameCaret_;
    std::unique_ptr<Text> title_, status_, members_, nameText_;
    std::vector<Button> buttons_;
    size_t page_ = 0;
    bool expanded_ = false;
    bool nameFocused_ = false;
    float caretTimer_ = 0.0f;
    std::string lobbyName_;
};
