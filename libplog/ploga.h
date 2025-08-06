#pragma once

#include "third_party/json.hpp"

#include <filesystem>
#include <istream>

class PLogAnalyzer {
  public:
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
    bool _hintAjmFound       : 1 = false;

    // Game engines
    bool _unityEngineDetected    : 1 = false;
    bool _cryEngineDetected      : 1 = false;
    bool _unrealEngineDetected   : 1 = false;
    bool _phyreEngineDetected    : 1 = false;
    bool _gmakerEngineDetected   : 1 = false;
    bool _naughtyEngineDetected  : 1 = false;
    bool _irrlichtEngineDetected : 1 = false;

    // SDKs
    bool _fmodSdkDetected   : 1 = false;
    bool _monoSdkDetected   : 1 = false;
    bool _criSdkDetected    : 1 = false;
    bool _havokSdkDetected  : 1 = false;
    bool _wwiseSdkDetected  : 1 = false;
    bool _dialogSdkDetected : 1 = false;

    // Problems
    bool _shaderGenTodo         : 1 = false;
    bool _vkValidation          : 1 = false;
    bool _exceptionDetected     : 1 = false;
    bool _netStuffDetected      : 1 = false;
    bool _vkNoDevices           : 1 = false;
    bool _missingSymbolDetected : 1 = false;
  };

  struct LineInfo {
    std::string_view channel;
    std::string_view module;
    std::string_view level;
    std::string_view timestamp;
    std::string_view source;
    std::string_view func;

    uint32_t processId;
    uint32_t threadId;
  };

  PLogAnalyzer() {}

  PLogAnalyzer(std::filesystem::path const& path);
  PLogAnalyzer(const char* data, size_t dataSize);

  void readstream(std::istream& stream);
  bool render(LineInfo const& lineInfo, std::string_view out);

  std::string spit() const;

  private:
  nlohmann::json m_jsonInfo;
};

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

EXPORT std::unique_ptr<PLogAnalyzer> createFileAnalyser(std::filesystem::path const& fpath);
EXPORT std::unique_ptr<PLogAnalyzer> createStreamAnalyser(std::istream& stream);
EXPORT std::unique_ptr<PLogAnalyzer> createMemAnalyser(const char* memory, size_t size);
