#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#include "json.hpp"
#include "p7d.h"

#include <codecvt>
#include <cstdio>
#include <locale>
#include <string_view>

std::string toUTF8(std::basic_string<char16_t> const& source) {
  std::string result;

  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convertor;
  result = convertor.to_bytes(source);

  return result;
}

auto toUTF8(std::basic_string_view<char16_t> source) {
  return toUTF8(std::basic_string<char16_t>(source));
}

class P7DumpAnalyser: public P7Dump {
  struct /* Flags */ {
    // psOff specific
    bool _processTypeGuessed : 1 = false;
    bool _isChildprocess     : 1 = false;
    bool _isGpuPicked        : 1 = false;

    // Hints
    bool _inputNotFoundHint : 1 = false;
    bool _nvidiaHint        : 1 = false;

    // Game engines
    bool _unityEngineDetected  : 1 = false;
    bool _cryEngineDetected    : 1 = false;
    bool _unrealEngineDetected : 1 = false;
    bool _phyreEngineDetected  : 1 = false;

    // SDKs
    bool _fmodSdkDetected : 1 = false;
    bool _monoSdkDetected : 1 = false;

    // Problems
    bool _shaderGenTodo : 1 = false;
    bool _vkValidation  : 1 = false;
  };

  public:
  P7DumpAnalyser(std::string_view fname): P7Dump(fname) {}

  void render(StreamStorage& stream, TraceLineData const& tsd, p7string const& out) override final {
    if (!_processTypeGuessed) {
      _processTypeGuessed = true;
      if ((_isChildprocess = (m_processName == u"psOff_tunnel.exe")) == true) { // Prepare child process things
        m_jsonInfo = {
            {"type", "child-process"},
            {
                "labels",
                nlohmann::json::array(),
            },
            {"title_name", "Unnamed"},
            {"title_id", "CUSA00000"},
            {"title_neo", false},
        };
      } else { // Prepare main process things
        m_jsonInfo = {
            {"type", "main-process"},
            {
                "labels",
                nlohmann::json::array(),
            },
            {
                "hints",
                nlohmann::json::array(),
            },
            {"user-gpu", "UNDETECTED"},
            {"user-lang", "UNDETECTED"},
        };
      }
    }

    auto& mod = stream.modules[tsd.modid];

    if (_isChildprocess) { // Handle child logs
      if (stream.info.name.contains(u"TTY")) {
      } else {
        if (mod.name.contains("pthread")) {
          if (!_unityEngineDetected) {
            if (out.contains(u"UnityWorker")) _unityEngineDetected = true;
            // if (!_unityEngineDetected)
          }
          if (!_phyreEngineDetected) {
            if (out.contains(u"PhyreEngine")) _phyreEngineDetected = true;
          }
          if (!_fmodSdkDetected) {
            if (out.contains(u"FMOD mixer")) _fmodSdkDetected = true;
          }
        } else if (mod.name.contains("libSceKernel")) {
          if (!_monoSdkDetected) {
            if (out.contains(u".mono/config")) _monoSdkDetected = true;
          }
        } else if (mod.name.contains("Kernel")) {
          if (out.starts_with(u"psOff.")) {
            auto value = std::basic_string_view<char16_t>(out.c_str() + out.find_first_of('=') + 2);
            if (out.contains(u".isNeo = "))
              m_jsonInfo["title_neo"] = value == u"1";
            else if (out.contains(u".app.id = "))
              m_jsonInfo["title_id"] = toUTF8(value);
            else if (out.contains(u".app.title = "))
              m_jsonInfo["title_name"] = toUTF8(value);
          }
        }
      }
    } else { // Handle main logs
      if (out.contains(u"Language switched to ")) m_jsonInfo["user-lang"] = toUTF8(std::basic_string_view<char16_t>(out.c_str() + out.find(u" to ") + 4));
      if (!_isGpuPicked && out.contains(u"Selected GPU:")) {
        _nvidiaHint            = out.contains(u"NVIDIA") || out.contains(u"nvidia");
        m_jsonInfo["user-gpu"] = toUTF8(std::basic_string_view<char16_t>(out.c_str() + out.find_first_of(u':') + 1));
      }
      if (!_inputNotFoundHint && out.contains(u"No pad with specified name was found")) _inputNotFoundHint = true;
      if (!_shaderGenTodo && (mod.name == "sb2spirv") && out.contains(u"todo")) _shaderGenTodo = true;
      if (!_vkValidation && (mod.name == "videoout") && out.contains(u"Validation Error: ")) _vkValidation = true;
    }
  }

  void run() {
    P7Dump::run();

    auto& labels = m_jsonInfo["labels"];
    if (_isChildprocess) {
      if (_unityEngineDetected) labels.push_back("engine-unity");
      if (_unrealEngineDetected) labels.push_back("engine-unreal");
      if (_cryEngineDetected) labels.push_back("engine-cry");
      if (_phyreEngineDetected) labels.push_back("engine-phyre");
      if (_fmodSdkDetected) labels.push_back("sdk-fmod");
      if (_monoSdkDetected) labels.push_back("sdk-mono");
    } else {
      auto& hints = m_jsonInfo["hints"];
      if (_inputNotFoundHint) hints.push_back("One of your users has the input device set incorrectly, if you can't control the app, this could be the cause");
      if (_nvidiaHint)
        hints.push_back("You are using an NVIDIA graphics card, these cards have many issues on our emulator that may not be present on AMD cards");
      if (_vkValidation) labels.push_back("graphics");
      if (_shaderGenTodo) labels.push_back("shader-gen");
    }

    printf("%s", m_jsonInfo.dump(4, ' ', true).c_str());
  }

  private:
  nlohmann::json m_jsonInfo;
};

int main(int argc, char* argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <p7d file path>", argv[0]);
    return 1;
  }

  P7DumpAnalyser an(argv[1]);
  an.run();

  return 0;
}
