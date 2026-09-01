#include "SoundManager.h"

#include <algorithm>
#include <format>

#include "Engine/StringUtility/StringUtility.h"

std::unique_ptr<SoundManager> SoundManager::instance_ = nullptr;

namespace {
size_t CategoryIndex(AudioCategory category) { return static_cast<size_t>(category); }
void AudioLog(const std::string& message) { OutputDebugStringA(("[Audio] " + message + "\n").c_str()); }
}

SoundManager::SoundManager(ConstructorKey)
{
}
SoundManager::~SoundManager() { Finalize(); }

SoundManager* SoundManager::GetInstance()
{
    if (!instance_) { instance_ = std::make_unique<SoundManager>(ConstructorKey {}); }
    return instance_.get();
}

void SoundManager::Initialize()
{
    if (isInitialized_) { return; }
    HRESULT result = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    if (FAILED(result)) { AudioLog("MFStartup failed"); return; }
    result = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(result)) { AudioLog("XAudio2Create failed"); MFShutdown(); return; }
    result = xAudio2_->CreateMasteringVoice(&masterVoice_);
    if (FAILED(result)) { AudioLog("CreateMasteringVoice failed"); xAudio2_.Reset(); MFShutdown(); return; }
    isInitialized_ = true;
}

void SoundManager::EnsureInitialized() { if (!isInitialized_) { Initialize(); } }

void SoundManager::Update()
{
    if (!isInitialized_) { return; }
    for (auto it = playingAudios_.begin(); it != playingAudios_.end();) {
        XAUDIO2_VOICE_STATE state {};
        it->voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
        if (!it->paused && state.BuffersQueued == 0) {
            if (it->handle == currentBGM_) { currentBGM_ = {}; }
            DestroyPlaying(*it);
            it = playingAudios_.erase(it);
        } else { ++it; }
    }
}

bool SoundManager::Load(const std::string& name, const std::string& filename, AudioCategory category)
{
    if (name.empty() || filename.empty()) { AudioLog("Load rejected: empty name or filename"); return false; }
    if (assets_.contains(name)) { return true; }
    SoundData loaded = SoundLoadFile(filename);
    if (!loaded.IsValid()) { return false; }
    assets_.emplace(name, AudioAsset { filename, category, std::make_shared<SoundData>(std::move(loaded)) });
    return true;
}

bool SoundManager::Unload(const std::string& name) { return assets_.erase(name) > 0; }
void SoundManager::UnloadAll() { assets_.clear(); }
bool SoundManager::IsLoaded(const std::string& name) const { return assets_.contains(name); }

AudioHandle SoundManager::Play(const std::string& name, bool loop, float volume)
{
    const auto it = assets_.find(name);
    if (it == assets_.end()) { AudioLog("Sound is not loaded: " + name); return {}; }
    return PlayData(it->second.data, it->second.category, loop, volume);
}

AudioHandle SoundManager::PlayBGM(const std::string& name, float volume)
{
    const auto it = assets_.find(name);
    if (it == assets_.end()) { AudioLog("BGM is not loaded: " + name); return {}; }
    StopBGM();
    currentBGM_ = PlayData(it->second.data, AudioCategory::BGM, true, volume);
    return currentBGM_;
}

AudioHandle SoundManager::PlaySE(const std::string& name, float volume)
{
    const auto it = assets_.find(name);
    if (it == assets_.end()) { AudioLog("SE is not loaded: " + name); return {}; }
    return PlayData(it->second.data, AudioCategory::SE, false, volume);
}

AudioHandle SoundManager::PlayVoice(const std::string& name, float volume)
{
    const auto it = assets_.find(name);
    if (it == assets_.end()) { AudioLog("Voice is not loaded: " + name); return {}; }
    return PlayData(it->second.data, AudioCategory::Voice, false, volume);
}

AudioHandle SoundManager::PlayData(std::shared_ptr<const SoundData> data, AudioCategory category, bool loop, float volume)
{
    EnsureInitialized();
    if (!isInitialized_ || !data || !data->IsValid()) { AudioLog("Invalid audio data"); return {}; }
    IXAudio2SourceVoice* voice = nullptr;
    HRESULT result = xAudio2_->CreateSourceVoice(&voice, &data->wfex);
    if (FAILED(result)) { AudioLog("CreateSourceVoice failed"); return {}; }

    XAUDIO2_BUFFER buffer {};
    buffer.pAudioData = data->buffer.data();
    buffer.AudioBytes = static_cast<UINT32>(data->buffer.size());
    buffer.Flags = XAUDIO2_END_OF_STREAM;
    buffer.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;
    result = voice->SubmitSourceBuffer(&buffer);
    if (FAILED(result)) { AudioLog("SubmitSourceBuffer failed"); voice->DestroyVoice(); return {}; }

    AudioHandle handle { nextHandleId_++, nextGeneration_++ };
    if (nextHandleId_ == 0) { nextHandleId_ = 1; }
    if (nextGeneration_ == 0) { nextGeneration_ = 1; }
    PlayingAudio playing { handle, voice, std::move(data), category, ClampVolume(volume), false };
    ApplyVolume(playing);
    result = voice->Start();
    if (FAILED(result)) { AudioLog("SourceVoice Start failed"); voice->DestroyVoice(); return {}; }
    playingAudios_.push_back(std::move(playing));
    return handle;
}

bool SoundManager::Stop(AudioHandle handle)
{
    for (auto it = playingAudios_.begin(); it != playingAudios_.end(); ++it) {
        if (it->handle == handle) {
            if (handle == currentBGM_) { currentBGM_ = {}; }
            DestroyPlaying(*it);
            playingAudios_.erase(it);
            return true;
        }
    }
    return false;
}

bool SoundManager::Pause(AudioHandle handle)
{
    PlayingAudio* playing = FindPlaying(handle);
    if (!playing || playing->paused) { return playing != nullptr; }
    if (FAILED(playing->voice->Stop())) { return false; }
    playing->paused = true;
    return true;
}

bool SoundManager::Resume(AudioHandle handle)
{
    PlayingAudio* playing = FindPlaying(handle);
    if (!playing || !playing->paused) { return playing != nullptr; }
    if (FAILED(playing->voice->Start())) { return false; }
    playing->paused = false;
    return true;
}

void SoundManager::StopBGM() { if (currentBGM_.IsValid()) { Stop(currentBGM_); } }
void SoundManager::PauseBGM() { Pause(currentBGM_); }
void SoundManager::ResumeBGM() { Resume(currentBGM_); }

void SoundManager::StopCategory(AudioCategory category)
{
    for (auto it = playingAudios_.begin(); it != playingAudios_.end();) {
        if (it->category == category) {
            if (it->handle == currentBGM_) { currentBGM_ = {}; }
            DestroyPlaying(*it);
            it = playingAudios_.erase(it);
        } else { ++it; }
    }
}

void SoundManager::StopAll()
{
    for (PlayingAudio& playing : playingAudios_) { DestroyPlaying(playing); }
    playingAudios_.clear();
    currentBGM_ = {};
}

void SoundManager::SetMasterVolume(float volume)
{
    masterVolume_ = ClampVolume(volume);
    for (PlayingAudio& playing : playingAudios_) { ApplyVolume(playing); }
}

void SoundManager::SetCategoryVolume(AudioCategory category, float volume)
{
    categoryVolumes_[CategoryIndex(category)] = ClampVolume(volume);
    for (PlayingAudio& playing : playingAudios_) { if (playing.category == category) { ApplyVolume(playing); } }
}

float SoundManager::GetCategoryVolume(AudioCategory category) const { return categoryVolumes_[CategoryIndex(category)]; }
bool SoundManager::IsPlaying(AudioHandle handle) const { return FindPlaying(handle) != nullptr; }

SoundManager::PlayingAudio* SoundManager::FindPlaying(AudioHandle handle)
{
    const auto it = std::find_if(playingAudios_.begin(), playingAudios_.end(), [handle](const PlayingAudio& p) { return p.handle == handle; });
    return it == playingAudios_.end() ? nullptr : &*it;
}

const SoundManager::PlayingAudio* SoundManager::FindPlaying(AudioHandle handle) const
{
    const auto it = std::find_if(playingAudios_.begin(), playingAudios_.end(), [handle](const PlayingAudio& p) { return p.handle == handle; });
    return it == playingAudios_.end() ? nullptr : &*it;
}

void SoundManager::DestroyPlaying(PlayingAudio& playing)
{
    if (playing.voice) {
        playing.voice->Stop();
        playing.voice->FlushSourceBuffers();
        playing.voice->DestroyVoice();
        playing.voice = nullptr;
    }
    playing.data.reset();
}

void SoundManager::ApplyVolume(PlayingAudio& playing)
{
    playing.voice->SetVolume(masterVolume_ * categoryVolumes_[CategoryIndex(playing.category)] * playing.volume);
}

float SoundManager::ClampVolume(float volume) { return std::clamp(volume, 0.0f, 1.0f); }

SoundData SoundManager::SoundLoadFile(const std::string& filename)
{
    EnsureInitialized();
    if (!isInitialized_) { return {}; }
    SoundData soundData {};
    Microsoft::WRL::ComPtr<IMFSourceReader> reader;
    HRESULT result = MFCreateSourceReaderFromURL(StringUtility::ConvertString(filename).c_str(), nullptr, &reader);
    if (FAILED(result)) { AudioLog("Failed to load: " + filename); return {}; }

    Microsoft::WRL::ComPtr<IMFMediaType> pcmType;
    result = MFCreateMediaType(&pcmType);
    if (SUCCEEDED(result)) { result = pcmType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio); }
    if (SUCCEEDED(result)) { result = pcmType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM); }
    if (SUCCEEDED(result)) { result = reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pcmType.Get()); }
    if (FAILED(result)) { AudioLog("PCM conversion failed: " + filename); return {}; }

    Microsoft::WRL::ComPtr<IMFMediaType> outputType;
    WAVEFORMATEX* format = nullptr;
    result = reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &outputType);
    if (SUCCEEDED(result)) { result = MFCreateWaveFormatExFromMFMediaType(outputType.Get(), &format, nullptr); }
    if (FAILED(result) || !format) { AudioLog("Wave format failed: " + filename); return {}; }
    soundData.wfex = *format;
    CoTaskMemFree(format);

    for (;;) {
        Microsoft::WRL::ComPtr<IMFSample> sample;
        DWORD flags = 0;
        result = reader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, nullptr, &sample);
        if (FAILED(result)) { AudioLog("Sample read failed: " + filename); return {}; }
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) { break; }
        if (!sample) { continue; }
        Microsoft::WRL::ComPtr<IMFMediaBuffer> mediaBuffer;
        if (FAILED(sample->ConvertToContiguousBuffer(&mediaBuffer))) { return {}; }
        BYTE* bytes = nullptr;
        DWORD length = 0;
        if (FAILED(mediaBuffer->Lock(&bytes, nullptr, &length))) { return {}; }
        soundData.buffer.insert(soundData.buffer.end(), bytes, bytes + length);
        mediaBuffer->Unlock();
    }
    return soundData;
}

void SoundManager::SoundUnload(SoundData* data)
{
    if (data) { data->buffer.clear(); data->wfex = {}; }
}

void SoundManager::SoundPlayWave(const SoundData& data)
{
    if (!data.IsValid()) { AudioLog("Invalid SoundPlayWave data"); return; }
    PlayData(std::make_shared<SoundData>(data), AudioCategory::SE, false, 1.0f);
}

void SoundManager::Finalize()
{
    if (!isInitialized_) { return; }
    StopAll();
    assets_.clear();
    if (masterVoice_) { masterVoice_->DestroyVoice(); masterVoice_ = nullptr; }
    xAudio2_.Reset();
    MFShutdown();
    isInitialized_ = false;
}
