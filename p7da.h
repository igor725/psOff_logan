#pragma once
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#include "p7d.h"
#include "third_party/json.hpp"

#include <filesystem>
#include <memory>

class P7DumpAnalyser: public P7Dump {
  struct /* Flags */ {
    // psOff specific
    bool _processTypeGuessed : 1 = false;
    bool _isChildprocess     : 1 = false;
    bool _isGpuPicked        : 1 = false;

    // Hints
    bool _inputNotFoundHint : 1 = false;
    bool _nvidiaHint        : 1 = false;
    bool _hintTrophyKey     : 1 = false;

    // Game engines
    bool _unityEngineDetected  : 1 = false;
    bool _cryEngineDetected    : 1 = false;
    bool _unrealEngineDetected : 1 = false;
    bool _phyreEngineDetected  : 1 = false;
    bool _gmakerEngineDetected : 1 = false;

    // SDKs
    bool _fmodSdkDetected   : 1 = false;
    bool _monoSdkDetected   : 1 = false;
    bool _dialogSdkDetected : 1 = false;

    // Problems
    bool _shaderGenTodo : 1 = false;
    bool _vkValidation  : 1 = false;
  };

  public:
  P7DumpAnalyser(std::filesystem::path const& fpath): P7Dump(fpath) {}

  virtual ~P7DumpAnalyser() = default;

  void render(StreamStorage& stream, TraceLineData const& tsd, p7string const& out) override final;

  void run();

  private:
  nlohmann::json m_jsonInfo;
};

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

EXPORT std::unique_ptr<P7Dump> createAnalyser(std::filesystem::path const& fpath);
