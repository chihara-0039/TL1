#include "Sound.h"
#include <cassert>
#include <cstring>
#include <Windows.h>
#include <combaseapi.h>
#include <algorithm>

using namespace Microsoft::WRL;

namespace {
void LogSoundError(const char* message, HRESULT result = S_OK) {
    OutputDebugStringA("[Sound] ");
    OutputDebugStringA(message);
    if (FAILED(result)) {
        char buffer[64]{};
        sprintf_s(buffer, " HRESULT=0x%08X", static_cast<unsigned int>(result));
        OutputDebugStringA(buffer);
    }
    OutputDebugStringA("\n");
}

float ClampVolume(float volume) {
    return std::clamp(volume, 0.0f, 1.0f);
}
}

std::wstring Sound::ConvertString(const std::string& str) {
    if (str.empty()) {
        return std::wstring();
    }

    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (sizeNeeded == 0) {
        sizeNeeded = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, nullptr, 0);
        if (sizeNeeded == 0) {
            LogSoundError("Failed to convert file path.");
            return std::wstring();
        }

        std::wstring result(sizeNeeded - 1, L'\0');
        MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, &result[0], sizeNeeded);
        return result;
    }

    std::wstring result(sizeNeeded - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], sizeNeeded);
    return result;
}

void Sound::Initialize() {
    HRESULT result;

    // Windows Media Foundationの初期化（ローカルファイル用途）
    result = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    assert(SUCCEEDED(result));
    if (FAILED(result)) {
        LogSoundError("MFStartup failed.", result);
        return;
    }

    // XAudio2エンジンのインスタンスを生成
    result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
    assert(SUCCEEDED(result));
    if (FAILED(result)) {
        LogSoundError("XAudio2Create failed.", result);
        return;
    }

    // マスターボイスを生成
    result = xAudio2->CreateMasteringVoice(&masterVoice);
    assert(SUCCEEDED(result));
    if (FAILED(result)) {
        LogSoundError("CreateMasteringVoice failed.", result);
        xAudio2.Reset();
    }
}

void Sound::Finalize() {

    // BGMを先に停止
    BGMStop();

    // SourceVoiceを先に停止・破棄
    StopAllSoundEffects();

    // MasterVoiceを破棄
    if (masterVoice) {
        masterVoice->DestroyVoice();
        masterVoice = nullptr;
    }

    // XAudio2を解放
    xAudio2.Reset();

    // 音声データ解放
    for (SoundData& soundData : soundDatas) {
        SoundUnload(&soundData);
    }
    soundDatas.clear();

    HRESULT result = MFShutdown();
    assert(SUCCEEDED(result));
    if (FAILED(result)) {
        LogSoundError("MFShutdown failed.", result);
    }
}


Sound::SoundData Sound::SoundLoadFile(const std::string& filename) {
    HRESULT result;

    // returnする為の音声データ
    SoundData soundData = {};

    // フルパスをワイド文字列に変換
    std::wstring filePathW = ConvertString(filename);
    if (filePathW.empty()) {
        LogSoundError("SoundLoadFile skipped because file path is empty.");
        return soundData;
    }

    // SourceReader作成
    ComPtr<IMFSourceReader> pReader;
    result = MFCreateSourceReaderFromURL(filePathW.c_str(), nullptr, &pReader);
    assert(SUCCEEDED(result));
    if (FAILED(result)) {
        LogSoundError(("Failed to open sound file: " + filename).c_str(), result);
        return soundData;
    }

    // PCM形式にフォーマット指定する
    ComPtr<IMFMediaType> pPCMType;
    result = MFCreateMediaType(&pPCMType);
    assert(SUCCEEDED(result));
    if (FAILED(result)) {
        LogSoundError("MFCreateMediaType failed.", result);
        return soundData;
    }

    result = pPCMType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    assert(SUCCEEDED(result));
    if (FAILED(result)) {
        LogSoundError("Failed to set audio major type.", result);
        return soundData;
    }

    result = pPCMType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    assert(SUCCEEDED(result));
    if (FAILED(result)) {
        LogSoundError("Failed to set audio subtype.", result);
        return soundData;
    }

    result = pReader->SetCurrentMediaType(
        (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
        nullptr,
        pPCMType.Get()
    );
    assert(SUCCEEDED(result));
    if (FAILED(result)) {
        LogSoundError("Failed to set current media type.", result);
        return soundData;
    }

    // 実際にセットされたメディアタイプを取得
    ComPtr<IMFMediaType> pOutType;
    result = pReader->GetCurrentMediaType(
        (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
        &pOutType
    );
    assert(SUCCEEDED(result));
    if (FAILED(result)) {
        LogSoundError("Failed to get current media type.", result);
        return soundData;
    }

    // Waveフォーマットを取得
    WAVEFORMATEX* waveFormat = nullptr;
    UINT32 waveFormatSize = 0;
    result = MFCreateWaveFormatExFromMFMediaType(
        pOutType.Get(),
        &waveFormat,
        &waveFormatSize
    );
    assert(SUCCEEDED(result));
    if (FAILED(result) || waveFormat == nullptr) {
        LogSoundError("Failed to create WAVEFORMATEX from media type.", result);
        return soundData;
    }

    // SoundDataに格納
    soundData.wfex = *waveFormat;

    // 生成したWaveフォーマットを解放
    CoTaskMemFree(waveFormat);

    // PCMデータのバッファを構築
    while (true) {
        ComPtr<IMFSample> pSample;
        DWORD streamIndex = 0;
        DWORD flags = 0;
        LONGLONG llTimeStamp = 0;

        // サンプルを読み込む
        result = pReader->ReadSample(
            MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            0,
            &streamIndex,
            &flags,
            &llTimeStamp,
            &pSample
        );
        assert(SUCCEEDED(result));
        if (FAILED(result)) {
            LogSoundError("ReadSample failed.", result);
            soundData = {};
            return soundData;
        }

        // ストリームの末尾に達したら抜ける
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
            break;
        }

        // サンプルがない場合は次へ
        if (!pSample) {
            continue;
        }

        ComPtr<IMFMediaBuffer> pBuffer;

        // サンプルに含まれるサウンドデータのバッファを取得
        result = pSample->ConvertToContiguousBuffer(&pBuffer);
        assert(SUCCEEDED(result));
        if (FAILED(result)) {
            LogSoundError("ConvertToContiguousBuffer failed.", result);
            soundData = {};
            return soundData;
        }

        BYTE* pData = nullptr;
        DWORD maxLength = 0;
        DWORD currentLength = 0;

        // バッファをロック
        result = pBuffer->Lock(&pData, &maxLength, &currentLength);
        assert(SUCCEEDED(result));
        if (FAILED(result) || pData == nullptr) {
            LogSoundError("Media buffer lock failed.", result);
            soundData = {};
            return soundData;
        }

        // 読み込んだPCMデータを末尾に追加
        soundData.buffer.insert(
            soundData.buffer.end(),
            pData,
            pData + currentLength
        );

        // バッファをアンロック
        result = pBuffer->Unlock();
        assert(SUCCEEDED(result));
        if (FAILED(result)) {
            LogSoundError("Media buffer unlock failed.", result);
            soundData = {};
            return soundData;
        }
    }

    if (!soundData.IsValid()) {
        LogSoundError(("Sound file has no playable audio data: " + filename).c_str());
        return soundData;
    }

    // 読み込んだ音声データを保持
    soundDatas.push_back(soundData);

    return soundData;
}

void Sound::SoundUnload(SoundData* soundData) {
    if (!soundData) {
        return;
    }
    soundData->buffer.clear();
    soundData->wfex = {};
}

void Sound::SoundPlay(const SoundData& soundData, float volume) {
    HRESULT result;

    CleanupFinishedSoundEffects();

    if (!xAudio2 || !soundData.IsValid()) {
        LogSoundError("SoundPlay skipped because sound data or XAudio2 is invalid.");
        return;
    }

    volume = ClampVolume(volume);

    // 波形フォーマットを元にSourceVoiceの生成
    IXAudio2SourceVoice* pSourceVoice = nullptr;
    result = xAudio2->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
    assert(SUCCEEDED(result));
    if (FAILED(result)) {
        LogSoundError("CreateSourceVoice failed for sound effect.", result);
        return;
    }

    //音量設定
    result = pSourceVoice->SetVolume(volume);
    assert(SUCCEEDED(result));
    if (FAILED(result)) {
        LogSoundError("SetVolume failed for sound effect.", result);
        pSourceVoice->DestroyVoice();
        return;
    }

    // 再生する波形データの設定
    XAUDIO2_BUFFER buf{};
    buf.pAudioData = soundData.buffer.data();
    buf.AudioBytes = static_cast<UINT32>(soundData.buffer.size());
    buf.Flags = XAUDIO2_END_OF_STREAM;

    // 波形データの再生
    result = pSourceVoice->SubmitSourceBuffer(&buf);
    assert(SUCCEEDED(result));
    if (FAILED(result)) {
        LogSoundError("SubmitSourceBuffer failed for sound effect.", result);
        pSourceVoice->DestroyVoice();
        return;
    }

    result = pSourceVoice->Start();
    assert(SUCCEEDED(result));
    if (FAILED(result)) {
        LogSoundError("Start failed for sound effect.", result);
        pSourceVoice->DestroyVoice();
        return;
    }

    // ★終了時に破棄できるように保持
    sourceVoices.push_back(pSourceVoice);
}

void Sound::BGMPlay(const SoundData& soundData, float volume)
{

    HRESULT result;

    // すでに鳴っているBGMを止める
    BGMStop();

    if (!xAudio2 || !soundData.IsValid()) {
        LogSoundError("BGMPlay skipped because sound data or XAudio2 is invalid.");
        return;
    }
    volume = ClampVolume(volume);
    bgmVolume_ = volume;

    result = xAudio2->CreateSourceVoice(&bgmSourceVoice, &soundData.wfex);
    assert(SUCCEEDED(result));
    if (FAILED(result)) {
        LogSoundError("CreateSourceVoice failed for BGM.", result);
        bgmSourceVoice = nullptr;
        return;
    }

    result = bgmSourceVoice->SetVolume(volume);
    assert(SUCCEEDED(result));
    if (FAILED(result)) {
        LogSoundError("SetVolume failed for BGM.", result);
        BGMStop();
        return;
    }

    XAUDIO2_BUFFER buf{};
    buf.pAudioData = soundData.buffer.data();
    buf.AudioBytes = static_cast<UINT32>(soundData.buffer.size());
    buf.Flags = XAUDIO2_END_OF_STREAM;

    // 無限ループ
    buf.LoopCount = XAUDIO2_LOOP_INFINITE;

    result = bgmSourceVoice->SubmitSourceBuffer(&buf);
    assert(SUCCEEDED(result));
    if (FAILED(result)) {
        LogSoundError("SubmitSourceBuffer failed for BGM.", result);
        BGMStop();
        return;
    }

    result = bgmSourceVoice->Start();
    assert(SUCCEEDED(result));
    if (FAILED(result)) {
        LogSoundError("Start failed for BGM.", result);
        BGMStop();
        return;
    }
}

void Sound::BGMStop() {
    if (bgmSourceVoice) {
        bgmSourceVoice->Stop();
        bgmSourceVoice->FlushSourceBuffers();
        bgmSourceVoice->DestroyVoice();
        bgmSourceVoice = nullptr;
    }

}

void Sound::BGMPause() {
    if (bgmSourceVoice) {
        HRESULT result = bgmSourceVoice->Stop();
        assert(SUCCEEDED(result));
        if (FAILED(result)) {
            LogSoundError("BGMPause failed.", result);
        }
    }
}

void Sound::BGMResume() {
    if (bgmSourceVoice) {
        HRESULT result = bgmSourceVoice->Start();
        assert(SUCCEEDED(result));
        if (FAILED(result)) {
            LogSoundError("BGMResume failed.", result);
        }
    }
}

void Sound::SetBGMVolume(float volume) {
    bgmVolume_ = ClampVolume(volume);
    if (bgmSourceVoice) {
        HRESULT result = bgmSourceVoice->SetVolume(bgmVolume_);
        assert(SUCCEEDED(result));
        if (FAILED(result)) {
            LogSoundError("SetBGMVolume failed.", result);
        }
    }
}

void Sound::StopAllSoundEffects() {
    for (IXAudio2SourceVoice* sourceVoice : sourceVoices) {
        if (sourceVoice) {
            sourceVoice->Stop();
            sourceVoice->FlushSourceBuffers();
            sourceVoice->DestroyVoice();
        }
    }
    sourceVoices.clear();
}

void Sound::CleanupFinishedSoundEffects() {
    for (auto it = sourceVoices.begin(); it != sourceVoices.end();) {
        IXAudio2SourceVoice* sourceVoice = *it;
        if (!sourceVoice) {
            it = sourceVoices.erase(it);
            continue;
        }

        XAUDIO2_VOICE_STATE state{};
        sourceVoice->GetState(&state);
        if (state.BuffersQueued == 0) {
            sourceVoice->DestroyVoice();
            it = sourceVoices.erase(it);
            continue;
        }

        ++it;
    }
}
