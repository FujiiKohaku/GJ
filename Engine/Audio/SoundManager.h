#pragma once

#include <Windows.h>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <wrl.h>
#include <xaudio2.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfobjects.h>
#include <mfreadwrite.h>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

struct SoundData {
    WAVEFORMATEX wfex {};
    std::vector<BYTE> buffer;
    bool IsValid() const { return !buffer.empty() && wfex.nChannels > 0 && wfex.nSamplesPerSec > 0; }
};

enum class AudioCategory { BGM, SE, Voice };

struct AudioHandle {
    uint32_t id = 0;
    uint32_t generation = 0;
    bool IsValid() const { return id != 0; }
    friend bool operator==(const AudioHandle&, const AudioHandle&) = default;
};

class SoundManager {
public:
    static SoundManager* GetInstance();
    SoundManager(const SoundManager&) = delete;
    SoundManager& operator=(const SoundManager&) = delete;

    class ConstructorKey { ConstructorKey() = default; friend class SoundManager; };
    explicit SoundManager(ConstructorKey);
    ~SoundManager();

    void Initialize();
    void Update();
    void Finalize();

    bool Load(const std::string& name, const std::string& filename, AudioCategory category = AudioCategory::SE);
    bool Unload(const std::string& name);
    void UnloadAll();
    bool IsLoaded(const std::string& name) const;

    AudioHandle Play(const std::string& name, bool loop = false, float volume = 1.0f);
    AudioHandle PlayBGM(const std::string& name, float volume = 1.0f);
    AudioHandle PlaySE(const std::string& name, float volume = 1.0f);
    AudioHandle PlayVoice(const std::string& name, float volume = 1.0f);
    bool Stop(AudioHandle handle);
    bool Pause(AudioHandle handle);
    bool Resume(AudioHandle handle);
    void StopBGM();
    void PauseBGM();
    void ResumeBGM();
    void StopCategory(AudioCategory category);
    void StopAll();

    void SetMasterVolume(float volume);
    void SetCategoryVolume(AudioCategory category, float volume);
    float GetMasterVolume() const { return masterVolume_; }
    float GetCategoryVolume(AudioCategory category) const;
    bool IsPlaying(AudioHandle handle) const;
    size_t GetPlayingCount() const { return playingAudios_.size(); }

    // 旧API互換
    SoundData SoundLoadFile(const std::string& filename);
    void SoundUnload(SoundData* soundData);
    void SoundPlayWave(const SoundData& soundData);

private:
    struct AudioAsset {
        std::string filename;
        AudioCategory category = AudioCategory::SE;
        std::shared_ptr<SoundData> data;
    };
    struct PlayingAudio {
        AudioHandle handle;
        IXAudio2SourceVoice* voice = nullptr;
        std::shared_ptr<const SoundData> data;
        AudioCategory category = AudioCategory::SE;
        float volume = 1.0f;
        bool paused = false;
    };

    void EnsureInitialized();
    AudioHandle PlayData(std::shared_ptr<const SoundData> data, AudioCategory category, bool loop, float volume);
    PlayingAudio* FindPlaying(AudioHandle handle);
    const PlayingAudio* FindPlaying(AudioHandle handle) const;
    void DestroyPlaying(PlayingAudio& playing);
    void ApplyVolume(PlayingAudio& playing);
    static float ClampVolume(float volume);

    static std::unique_ptr<SoundManager> instance_;
    Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
    IXAudio2MasteringVoice* masterVoice_ = nullptr;
    std::unordered_map<std::string, AudioAsset> assets_;
    std::vector<PlayingAudio> playingAudios_;
    AudioHandle currentBGM_ {};
    uint32_t nextHandleId_ = 1;
    uint32_t nextGeneration_ = 1;
    float masterVolume_ = 1.0f;
    float categoryVolumes_[3] = { 1.0f, 1.0f, 1.0f };
    bool isInitialized_ = false;
};
