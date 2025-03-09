#pragma once
#define JSON_NO_IO
#define JSON_HAS_CPP_20

#include "p7d.h"
#include "third_party/json.hpp"

#include <memory>

class P7DumpAnalyser: public P7Dump {
  struct /* Flags */ {
    // psOff specific
    bool _processTypeGuessed : 1 = false;
    bool _isChildprocess     : 1 = false;
    bool _isGpuPicked        : 1 = false;

    // Hints
    bool _inputNotFoundHint  : 1 = false;
    bool _nvidiaHint         : 1 = false;
    bool _hintTrophyKey      : 1 = false;
    bool _hintAndnPatched    : 1 = false;
    bool _hintInsertqPatched : 1 = false;
    bool _hintExtrqPatched   : 1 = false;

    // Game engines
    bool _unityEngineDetected  : 1 = false;
    bool _cryEngineDetected    : 1 = false;
    bool _unrealEngineDetected : 1 = false;
    bool _phyreEngineDetected  : 1 = false;
    bool _gmakerEngineDetected : 1 = false;

    // SDKs
    bool _fmodSdkDetected   : 1 = false;
    bool _monoSdkDetected   : 1 = false;
    bool _criSdkDetected    : 1 = false;
    bool _wwiseSdkDetected  : 1 = false;
    bool _dialogSdkDetected : 1 = false;

    // Problems
    bool _shaderGenTodo     : 1 = false;
    bool _vkValidation      : 1 = false;
    bool _exceptionDetected : 1 = false;
  };

  public:
  P7DumpAnalyser() = default;

  virtual ~P7DumpAnalyser() = default;

  void render(StreamStorage& stream, TraceLineData const& tsd, p7string const& out) override final;

  std::string spit() const override final;

  void run() override final;

  private:
  nlohmann::json m_jsonInfo;
};

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

EXPORT std::unique_ptr<P7Dump> createFileAnalyser(std::filesystem::path const& fpath);
EXPORT std::unique_ptr<P7Dump> createMemAnalyser(void* memory, size_t size);
