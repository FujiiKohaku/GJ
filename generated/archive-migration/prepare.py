from pathlib import Path
import re, shutil, hashlib, json

src = Path('C:/project/GJ3')
dst = Path('C:/project/MyEngine/project')
out = src / 'generated/archive-migration/files'
manifest = {}

def read(root, path):
    return (root / path).read_text(encoding='utf-8-sig')

def save(path, text):
    target = out / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(text, encoding='utf-8-sig', newline='\r\n')
    original = dst / path
    manifest[path] = hashlib.sha256(original.read_bytes()).hexdigest() if original.exists() else None

def replace(text, old, new):
    assert old in text, old[:100]
    return text.replace(old, new)

def function(text, signature, body):
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[:start] + signature + '\n{\n' + body.strip() + '\n}' + text[end:]

h = read(src, 'App/Scene/ArchiveScene.h').replace('ArchiveScene', 'StageSelectScene')
h = h.replace('#include "PageTransition.h"', '#include "ArchiveAudio.h"')
start = h.index('    // 新しい遷移先')
end = h.index('    struct DustMote')
h = h[:start] + '''    enum class StageDestination { GamePlay, GameTest, SpriteTest, TextTest, Title };
    struct StageData {
        std::string name;
        std::string description;
        StageDestination destination = StageDestination::GamePlay;
        std::string stageId;
    };

''' + h[end:]
h = h.replace('        TitleIdle,\n', '').replace('    void UpdateTitleIdle();\n', '')
h = h.replace('    void EnterTitleMode();', '    void InitializeTransition();')
h = h.replace('    PageTransition::RevealOverlay pageReveal_;', '    ArchiveAudio audio_;\n    bool transitionQueued_ = false;\n    std::string confirmedStageId_;')
save('App/Scene/StageSelectScene.h', h)

c = read(src, 'App/Scene/ArchiveScene.cpp').replace('ArchiveScene', 'StageSelectScene')
c = c.replace('#include "TestScene.h"\n#include "GameLabScene.h"\n#include "EditorScene.h"', '''#include "TestScene1.h"
#include "SpriteTestScene.h"
#include "TextTestScene.h"
#include "TitleScene.h"
#include "LoadingScene.h"
#include "App/Game/Stage/StageCatalog.h"
#include "Engine/WinApp/WinApp.h"
#include "Engine/Light/LightManager.h"
#include <format>''')
c = c.replace('    SceneManager::GetInstance()->SetArchiveApproach(0.0f);', '    SceneManager::GetInstance()->SetArchiveApproach(0.0f);', 1)
c = replace(c, '    camera_ = std::make_unique<Camera>();', '''    ShowCursor(TRUE);
    ClipCursor(nullptr);
    SceneManager::GetInstance()->RemovePostEffect(PostEffectType::Fog);
    camera_ = std::make_unique<Camera>();''')
c = c.replace('    SoundManager* audio = SoundManager::GetInstance();', '    audio_.Initialize();')
c = re.sub(r'audio->Load\((\w+), (\w+), AudioCategory::SE\);', r'audio_.Load(\1, \2);', c)
c = re.sub(r'SoundManager::GetInstance\(\)->PlaySE\(', 'audio_.Play(', c)
c = replace(c, '    EnterTitleMode();\n    pageReveal_.InitializeIfRequested();', '    InitializeTransition();\n    StartArchiveApproach();\n    UpdateSceneObjects();')
c = function(c, 'void StageSelectScene::Finalize()', '''    audio_.Stop();
    auto* objects = Object3dManager::GetInstance();
    if (objects->GetDefaultCamera() == camera_.get()) {
        objects->SetDefaultCamera(nullptr);
    }''')
c = function(c, 'void StageSelectScene::InitializeStageData()', '''    auto* catalog = StageCatalog::GetInstance();
    const bool loaded = catalog->Load();
    stages_.clear();
    size_t number = 0;
    if (loaded) {
        for (const auto& stage : catalog->GetStages()) {
            if (stage.id == "gimmick_test") continue;
            stages_.push_back({ std::format("STAGE {:02}  {}", ++number, stage.name),
                stage.description, StageDestination::GamePlay, stage.id });
        }
    }
    if (stages_.empty()) {
        stages_.push_back({ "NO STAGES - RETURN TO TITLE",
            loaded ? "The stage catalog is empty." : "Unable to load the stage catalog.",
            StageDestination::Title, {} });
    }
    stages_.push_back({ "GAME TEST / F1", "Open the gameplay test scene.", StageDestination::GameTest, {} });
    stages_.push_back({ "SPRITE TEST / F2", "Open the sprite test scene.", StageDestination::SpriteTest, {} });
    stages_.push_back({ "TEXT TEST / F3", "Open the text test scene.", StageDestination::TextTest, {} });
    if (loaded) {
        if (const auto* gimmick = catalog->Find("gimmick_test")) {
            stages_.push_back({ "GIMMICK TEST", gimmick->description, StageDestination::GamePlay, gimmick->id });
        }
    }
    stages_.push_back({ "RETURN TO TITLE", "Return to the title screen.", StageDestination::Title, {} });''')
c = function(c, 'void StageSelectScene::EnterTitleMode()', '''    animationTime_ = 0.0f;
    pageTurnProgress_ = 0.0f;
    stageIndexChanged_ = false;
    openingRifflePlayed_ = false;
    UpdateBookOpening(0.0f);
    std::fill(openingPageVisible_.begin(), openingPageVisible_.end(), false);
    UpdateCardTransform(0.0f, 0.0f);
    transitionPage_ = std::make_unique<Sprite>();
    transitionPage_->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
    transitionPage_->SetAnchorPoint({ 0.5f, 0.5f });
    transitionPage_->SetPosition({ 640.0f, 360.0f });
    transitionPage_->SetSize({ 1280.0f, 720.0f });
    transitionPage_->SetColor({ 1.0f, 0.91f, 0.68f, 0.0f });
    transitionPage_->Update();''').replace('void StageSelectScene::EnterTitleMode()', 'void StageSelectScene::InitializeTransition()')
c = c.replace('        EnterTitleMode();', '        transitionQueued_ = true;\n        audio_.Stop();\n        SceneManager::GetInstance()->SetNextScene(std::make_unique<TitleScene>());')
c = c.replace('    pageReveal_.Update(deltaTime);', '''    if (transitionQueued_) return;
    // The previous scene is finalized one frame after our Initialize.
    Object3dManager::GetInstance()->SetDefaultCamera(camera_.get());
    audio_.Update();''')
c = function(c, 'bool StageSelectScene::HandleInput()', '''    Input* input = Input::GetInstance();
    if (!input || state_ != BookSelectState::Idle || transitionQueued_) return false;
    if (input->IsKeyTrigger(DIK_BACK)) {
        StartTitleReturn();
        return true;
    }
    StageDestination shortcut = StageDestination::Title;
    if (input->IsKeyTrigger(DIK_F1)) shortcut = StageDestination::GameTest;
    else if (input->IsKeyTrigger(DIK_F2)) shortcut = StageDestination::SpriteTest;
    else if (input->IsKeyTrigger(DIK_F3)) shortcut = StageDestination::TextTest;
    if (shortcut != StageDestination::Title) {
        auto entry = std::find_if(stages_.begin(), stages_.end(), [shortcut](const StageData& stage) {
            return stage.destination == shortcut;
        });
        if (entry != stages_.end()) {
            currentStageIndex_ = static_cast<int32_t>(entry - stages_.begin());
            RefreshStageText();
            ConfirmStage();
        }
        return true;
    }
    bool leftClick = false, rightClick = false, confirmClick = false;
    if (input->IsMouseTrigger(0)) {
        POINT mouse {};
        RECT client {};
        const HWND window = WinApp::GetInstance()->GetHwnd();
        if (GetCursorPos(&mouse) && ScreenToClient(window, &mouse) && GetClientRect(window, &client) &&
            client.right > 0 && client.bottom > 0) {
            const float x = mouse.x * 1280.0f / client.right;
            const float y = mouse.y * 720.0f / client.bottom;
            if (y >= 200.0f && y <= 560.0f) {
                leftClick = x >= 120.0f && x < 440.0f;
                rightClick = x > 840.0f && x <= 1160.0f;
                confirmClick = x >= 440.0f && x <= 840.0f;
            }
        }
    }
    if (input->IsKeyTrigger(DIK_RIGHT) || input->IsKeyTrigger(DIK_D) || rightClick) {
        StartPageTurn(PageTurnDirection::Right);
    } else if (input->IsKeyTrigger(DIK_LEFT) || input->IsKeyTrigger(DIK_A) || leftClick) {
        StartPageTurn(PageTurnDirection::Left);
    } else if (input->IsKeyTrigger(DIK_RETURN) || input->IsKeyTrigger(DIK_SPACE) || confirmClick) {
        ConfirmStage();
        return true;
    }
    return false;''')
# Remove the unused title wait branch and camera drift method.
c = re.sub(r'    case BookSelectState::TitleIdle:.*?        break;\n', '', c, count=1, flags=re.S)
start = c.index('void StageSelectScene::UpdateTitleIdle()')
end = c.index('void StageSelectScene::UpdateSceneObjects()', start)
c = c[:start] + c[end:]
c = c.replace('    titleText_->SetText("GJ");', '    titleText_->SetText("THE STAGE ARCHIVE");')
c = c.replace('    titleText_->SetPosition({ 640.0f, 150.0f });\n', '')
c = c.replace('    titleText_->SetFontSize(112.0f);', '    titleText_->SetFontSize(48.0f);')
c = c.replace('"ENTER / SPACE : START"', '"LEFT / RIGHT: PAGE    ENTER: SELECT    BACKSPACE: TITLE"')
c = c.replace('"A / D OR LEFT / RIGHT : TURN PAGE    ENTER : SELECT    BACKSPACE : TITLE"', '"A / D: PAGE    ENTER / CLICK: SELECT    BACKSPACE: TITLE    F1 / F2 / F3: TESTS"')
c = c.replace('    confirmedDestination_ = stage.destination;', '    confirmedDestination_ = stage.destination;\n    confirmedStageId_ = stage.stageId;')
c = replace(c, '    state_ = BookSelectState::StageConfirmed;\n    audio_.Play', '''    if (stages_.empty() || state_ != BookSelectState::Idle) return;
    if (stages_[currentStageIndex_].destination == StageDestination::Title) {
        StartTitleReturn();
        return;
    }
    state_ = BookSelectState::StageConfirmed;
    audio_.Play''')
start = c.index('    if (animationTime_ >= kConfirmSceneChangeTime)')
end = c.index('\nfloat StageSelectScene::Clamp01', start)
c = c[:start] + '''    if (animationTime_ >= kConfirmSceneChangeTime && !transitionQueued_) {
        transitionQueued_ = true;
        audio_.Stop();
        PageTransition::RequestReveal();
        auto* manager = SceneManager::GetInstance();
        switch (confirmedDestination_) {
        case StageDestination::GamePlay:
            manager->SetNextSceneWithLoading<LoadingScene, GamePlayScene>(confirmedStageId_);
            break;
        case StageDestination::GameTest:
            manager->SetNextSceneWithLoading<LoadingScene, TestScene1>();
            break;
        case StageDestination::SpriteTest:
            manager->SetNextScene(std::make_unique<SpriteTestScene>());
            break;
        case StageDestination::TextTest:
            manager->SetNextScene(std::make_unique<TextTestScene>());
            break;
        case StageDestination::Title:
            manager->SetNextScene(std::make_unique<TitleScene>());
            break;
        }
    }
}
''' + c[end:]
c = c.replace('    pageReveal_.Draw();\n', '')
c = c.replace('state_ != BookSelectState::TitleIdle &&\n        ', '')
c = c.replace(' && state_ != BookSelectState::TitleIdle', '')
c = replace(c, 'void StageSelectScene::Draw3D()\n{\n    Object3dManager::GetInstance()->PreDraw();', '''void StageSelectScene::Draw3D()
{
    Object3dManager::GetInstance()->PreDraw();
    LightManager::GetInstance()->Bind(DirectXCommon::GetInstance()->GetCommandList());''')
assert 'TitleIdle' not in c and 'EnterTitleMode' not in c
save('App/Scene/StageSelectScene.cpp', c)

# Append only the two primitive factories required by the archive.
s = read(src, 'Engine/3D/ModelManager.cpp')
d = read(dst, 'Engine/3D/ModelManager.cpp')
cube_data = s[s.index('ModelData CreateCubeModelData('):s.index('ModelData CreateCylinderModelData(')]
d = replace(d, 'ModelData CreateCylinderModelData(', cube_data + 'ModelData CreateCylinderModelData(')
factories = s[s.index('Model* ModelManager::CreateBookLeaf('):s.index('Model* ModelManager::CreateCylinder(')]
d = replace(d, 'Model* ModelManager::CreateCylinder(', factories + 'Model* ModelManager::CreateCylinder(')
d = d.replace('#include <cmath>', '#include <cmath>\n#include <cassert>')
save('Engine/3D/ModelManager.cpp', d)
d = read(dst, 'Engine/3D/ModelManager.h')
declarations = read(src, 'Engine/3D/ModelManager.h')
declarations = declarations[declarations.index('    Model* CreateCube('):declarations.index('    Model* CreateCylinder(')]
save('Engine/3D/ModelManager.h', replace(d, '    Model* CreateCylinder(', declarations + '    Model* CreateCylinder('))

shader = read(src, 'resources/Shaders/Object3D/Object3d.PS.hlsl')
branch = shader[shader.index('    else if (gMaterial.enableLighting >= 3'):shader.index('    else if (gMaterial.enableLighting == 8)')]
d = read(dst, 'resources/Shaders/Object3D/Object3d.PS.hlsl')
save('resources/Shaders/Object3D/Object3d.PS.hlsl', replace(d, '    else if (gMaterial.enableLighting != 0)', branch + '    else if (gMaterial.enableLighting != 0)'))
save('resources/Shaders/PostEffect/ArchiveAtmosphere.PS.hlsl', read(src, 'resources/Shaders/PostEffect/ArchiveAtmosphere.PS.hlsl'))
d = read(dst, 'Engine/PostEffect/PostEffectType.h')
save('Engine/PostEffect/PostEffectType.h', replace(d, '    BlackHoleDistortion,', '    BlackHoleDistortion,\n    ArchiveAtmosphere,'))
d = read(dst, 'Engine/PostEffect/PostEffectType.cpp')
save('Engine/PostEffect/PostEffectType.cpp', replace(d, '    case PostEffectType::Copy:', '    case PostEffectType::ArchiveAtmosphere:\n        return "ArchiveAtmosphere";\n    case PostEffectType::Copy:'))
d = read(dst, 'Engine/PostEffect/CopyImageRenderer.cpp')
d = replace(d, '    switch (type) {', '    switch (type) {\n    case PostEffectType::ArchiveAtmosphere:\n        return L"resources/Shaders/PostEffect/ArchiveAtmosphere.PS.hlsl";')
save('Engine/PostEffect/CopyImageRenderer.cpp', d)
d = read(dst, 'Engine/PostEffect/PostEffectManager.cpp')
d = replace(d, '    postEffectParameter.radialBlurCenter =', '''    // ArchiveAtmosphere uses the existing reserved slots; the buffer layout stays intact.
    postEffectParameter.padding0 = 16.5f;
    postEffectParameter.padding1 = 2.3f;
    postEffectParameter.padding2 = sceneManager->GetArchiveApproach();
    postEffectParameter.radialBlurCenter =''')
save('Engine/PostEffect/PostEffectManager.cpp', d)

# The shared overlay consumes the request for both loading and direct test destinations.
p = read(src, 'App/Scene/PageTransition.h')
p = p.replace('inline bool pendingSlimeReveal = false;\n', '')
start = p.index('inline void RequestSlimeReveal()')
end = p.index('class RevealOverlay', start)
p = p[:start] + p[end:]
save('App/Scene/PageTransition.h', p)
d = read(dst, 'App/Scene/SceneManager.h')
d = replace(d, '#include "BaseScene.h"', '#include "BaseScene.h"\n#include "PageTransition.h"')
d = replace(d, '    void Update();', '''    void SetArchiveApproach(float progress) { archiveApproach_ = progress; }
    float GetArchiveApproach() const { return archiveApproach_; }
    void Update();''')
d = replace(d, '    float paintProgress_ =', '    float archiveApproach_ = 0.0f;\n    PageTransition::RevealOverlay pageReveal_;\n    float paintProgress_ =')
save('App/Scene/SceneManager.h', d)
d = read(dst, 'App/Scene/SceneManager.cpp')
d = replace(d, '#include <cassert>', '#include <cassert>\n#include "Engine/Time/TimeManager.h"')
d = replace(d, '    ChangeScene(scene_, nextScene_, retiredScene_);', '''    if (nextScene_) {
        RemovePostEffect(PostEffectType::ArchiveAtmosphere);
        archiveApproach_ = 0.0f;
    }
    if (ChangeScene(scene_, nextScene_, retiredScene_)) {
        pageReveal_.InitializeIfRequested();
    }
    pageReveal_.Update(TimeManager::GetInstance()->GetDeltaTime());''')
d = replace(d, '        scene_->Draw2D();\n    }', '        scene_->Draw2D();\n    }\n    pageReveal_.Draw();')
save('App/Scene/SceneManager.cpp', d)

# Stage-owned voices retain their sample buffers and are destroyed before buffers are released.
save('App/Scene/ArchiveAudio.h', '''#pragma once
#include "Engine/Audio/SoundManager.h"
#include <unordered_map>

class ArchiveAudio {
public:
    ~ArchiveAudio() { Stop(); }
    void Initialize()
    {
        HRESULT result = XAudio2Create(engine_.GetAddressOf());
        if (FAILED(result)) return;
        result = engine_->CreateMasteringVoice(&master_);
        if (FAILED(result)) engine_.Reset();
    }
    void Load(const std::string& name, const std::string& path)
    {
        sounds_.emplace(name, SoundManager::GetInstance()->SoundLoadFile(path));
    }
    void Play(const std::string& name, float volume)
    {
        Update();
        const auto found = sounds_.find(name);
        if (!engine_ || found == sounds_.end() || found->second.buffer.empty()) return;
        const auto& sound = found->second;
        IXAudio2SourceVoice* voice = nullptr;
        if (FAILED(engine_->CreateSourceVoice(&voice, &sound.wfex))) return;
        XAUDIO2_BUFFER buffer {};
        buffer.pAudioData = sound.buffer.data();
        buffer.AudioBytes = static_cast<UINT32>(sound.buffer.size());
        buffer.Flags = XAUDIO2_END_OF_STREAM;
        voice->SetVolume(volume);
        if (FAILED(voice->SubmitSourceBuffer(&buffer)) || FAILED(voice->Start())) {
            voice->DestroyVoice();
            return;
        }
        voices_.push_back(voice);
    }
    void Update()
    {
        for (auto it = voices_.begin(); it != voices_.end();) {
            XAUDIO2_VOICE_STATE state {};
            (*it)->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
            if (state.BuffersQueued == 0) {
                (*it)->DestroyVoice();
                it = voices_.erase(it);
            } else ++it;
        }
    }
    void Stop()
    {
        for (auto* voice : voices_) voice->DestroyVoice();
        voices_.clear();
        if (master_) { master_->DestroyVoice(); master_ = nullptr; }
        engine_.Reset();
        sounds_.clear();
    }
private:
    Microsoft::WRL::ComPtr<IXAudio2> engine_;
    IXAudio2MasteringVoice* master_ = nullptr;
    std::unordered_map<std::string, SoundData> sounds_;
    std::vector<IXAudio2SourceVoice*> voices_;
};
''')

for name in ('CG2_LE2C_FUJII.vcxproj', 'CG2_LE2C_FUJII.vcxproj.filters'):
    d = read(dst, name)
    entry = '    <ClInclude Include="App\\Scene\\StageSelectScene.h"'
    extra = '    <ClInclude Include="App\\Scene\\ArchiveAudio.h" />\n    <ClInclude Include="App\\Scene\\PageTransition.h" />\n'
    save(name, replace(d, entry, extra + entry))

for directory in ('resources/Models/StageSelectBook', 'resources/Audio/StageSelect'):
    for file in (src / directory).rglob('*'):
        if not file.is_file(): continue
        relative = file.relative_to(src).as_posix()
        target = dst / relative
        if target.exists():
            assert file.read_bytes() == target.read_bytes(), f'Resource collision: {relative}'
            continue
        (out / relative).parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(file, out / relative)
        manifest[relative] = None
for relative in ('resources/Textures/white.png', 'resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf'):
    assert (dst / relative).exists(), relative

(out.parent / 'manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
print(f'Prepared {len(manifest)} files. TitleScene is not included.')
