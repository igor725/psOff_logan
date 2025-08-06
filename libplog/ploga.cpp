#include "ploga.h"

#include <fstream>
#include <istream>
#include <memory>
#include <string>
#include <string_view>

static std::string_view parseLogLine(std::string const& input, PLogAnalyzer::LineInfo& info) {
  // Built-in standard regexp is slow as christmas, we can't use it here :/
  // "(.+);(.+);(T|D|I|W|E|C);(.+);(\\d+);(\\d+);(.*);(.*);(.+)"
  size_t  strpos  = 0;
  int32_t currKey = 0;

  auto inputView = std::string_view(input);
  do {
    if (input.length() <= strpos) return {};
    size_t const keyEnd = input.find(';', strpos);
    if (keyEnd == std::string::npos) return {};
    auto const keySize = keyEnd - strpos;

    switch (currKey++) {
      case 0: { // Channel
        info.channel = inputView.substr(strpos, keySize);
      } break;
      case 1: { // Module
        info.module = inputView.substr(strpos, keySize);
      } break;
      case 2: { // Level
        info.level = inputView.substr(strpos, keySize);
      } break;
      case 3: { // Timestamp
        info.timestamp = inputView.substr(strpos, keySize);
      } break;
      case 4: { // ProcessID
        info.processId = 0;
      } break;
      case 5: { // ThreadID
        info.threadId = 0;
      } break;
      case 6: { // Source
        info.source = inputView.substr(strpos, keySize);
      } break;
      case 7: { // Function
        info.func = inputView.substr(strpos, keySize);
      } break;
    }

    strpos = keyEnd + 1;
  } while (currKey < 8);

  auto out = inputView.substr(strpos);
  if (out.ends_with('\r')) out = out.substr(0, out.length() - 1);
  return out;
}

class CharArrayBuffer: public std::streambuf {
  public:
  CharArrayBuffer(const char* data, size_t dataSize) { setg(const_cast<char*>(data), const_cast<char*>(data), const_cast<char*>(data) + dataSize); }
};

PLogAnalyzer::PLogAnalyzer(const char* data, size_t dataSize) {
  CharArrayBuffer cbuff(data, dataSize);

  std::istream strm(&cbuff);
  readstream(strm);
}

PLogAnalyzer::PLogAnalyzer(std::filesystem::path const& path) {
  std::ifstream file(path);
  readstream(file);
}

void PLogAnalyzer::readstream(std::istream& stream) {
  std::string line;
  while (std::getline(stream, line)) {
    LineInfo li;

    auto out = parseLogLine(line, li);
    if (!render(li, out)) {
      break;
    }
  }

  auto& labels = m_jsonInfo["labels"];
  auto& hints  = m_jsonInfo["hints"];

  if (_isChildprocess) {
    if (_unityEngineDetected) labels.push_back("engine-unity");
    if (_unrealEngineDetected) labels.push_back("engine-unreal");
    if (_cryEngineDetected) labels.push_back("engine-cry");
    if (_phyreEngineDetected) labels.push_back("engine-phyre");
    if (_gmakerEngineDetected) labels.push_back("engine-gamemaker");
    if (_naughtyEngineDetected) labels.push_back("engine-naughty");
    if (_irrlichtEngineDetected) labels.push_back("engine-irrlicht");
    if (_exceptionDetected) labels.push_back("exception");
    if (_fmodSdkDetected) labels.push_back("sdk-fmod");
    if (_monoSdkDetected) labels.push_back("sdk-mono");
    if (_criSdkDetected) labels.push_back("sdk-criware");
    if (_havokSdkDetected) labels.push_back("sdk-havok");
    if (_wwiseSdkDetected) labels.push_back("sdk-wwise");
    if (_missingSymbolDetected) labels.push_back("missing-symbol");
  } else {
    if (_inputNotFoundHint)
      hints.push_back("One of your users has the input device set incorrectly, if you can't control the PS4 app, this could be the cause.");
    if (_nvidiaHint)
      hints.push_back("You are using an NVIDIA graphics card, these cards have many issues on our emulator that may not be present on AMD cards.");
    if (_hintAndnPatched || _hintExtrqPatched || _hintInsertqPatched) {
      std::string unsupported = "Your CPU does not support some instructions (";
      if (_hintAndnPatched) unsupported += "ANDN, ";
      if (_hintExtrqPatched) unsupported += "EXTRQ, ";
      if (_hintInsertqPatched) unsupported += "INSERTQ, ";
      hints.push_back(unsupported + ") and they have been patched");
    }
    if (_hintAjmFound) hints.push_back("This game uses hardware audio encoding/decoding");
    if (_vkValidation) labels.push_back("graphics");
    if (_shaderGenTodo) labels.push_back("shader-gen");
    if (_vkNoDevices) {
      hints.push_back("Your GPU is not supported at the moment");
      labels.push_back("badgpu");
    }
  }

  if (_hintTrophyKey)
    hints.push_back("You don't have the trophy key installed, this can cause problems in games, also you won't be able to see the list of trophies you have "
                    "received. To solve this problem, check #faq channel in on Discord Server.");
}

bool PLogAnalyzer::render(LineInfo const& lineInfo, std::string_view out) {
  if (out.empty()) return true; // Skip line rendering

  if (!_processTypeGuessed) {
    _processTypeGuessed = true;
    if ((_isChildprocess = (out == "child process")) == true) { // Prepare child process things
      m_jsonInfo = {
          {"type", "child-process"},
          {
              "labels",
              nlohmann::json::array(),
          },
          {
              "firmware",
              nlohmann::json::array(),
          },
          {
              "hints",
              nlohmann::json::array(),
          },
          {"emu_neo", false},
          {"emu_skipAjm", false},
          {"emu_skipMovies", false},
          {"emu_networking", false},
          {"emu_noElfCheck", false},
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
          {"user-gp", "UNDETECTED"},
          {"user-lang", "UNDETECTED"},
      };
    }

    return true;
  }

  if (_isChildprocess) { // Handle child logs
    if (lineInfo.module == "TTY") {
      if (!_gmakerEngineDetected && out.contains("YoYo Games PS4 Runner")) _gmakerEngineDetected = true;
      if (!_irrlichtEngineDetected && out.contains("Irrlicht Engine")) _irrlichtEngineDetected = true;
      if (!_unrealEngineDetected && out.starts_with("Additional") && out.contains(".uproject")) _unrealEngineDetected = true;
      if (!_unrealEngineDetected && out.contains("uecommandline.txt")) _unrealEngineDetected = true;
      if (!_naughtyEngineDetected && out.contains("ND File Server")) _naughtyEngineDetected = true;
      if (!_naughtyEngineDetected && out.contains("----- Switching world: from")) _naughtyEngineDetected = true;
    } else {
      if (out.starts_with("todo ")) {
        if (!_netStuffDetected && out.starts_with("todo sceNp")) _netStuffDetected = true;
        return true;
      }

      if (lineInfo.module == "pthread") {
        if (out.starts_with("--> thread")) { // Thread run log
          if (!_unityEngineDetected) {
            if (out.contains("UnityWorker")) _unityEngineDetected = true;
            if (!_unityEngineDetected && out.contains("UnityGfx")) _unityEngineDetected = true;
          }
          if (!_criSdkDetected) {
            if (out.contains("CriThread") || out.contains("CRI FS")) _criSdkDetected = true;
          }
          if (!_wwiseSdkDetected) {
            if (out.contains("Wwise")) _wwiseSdkDetected = true;
            if (!_wwiseSdkDetected && out.contains("AK::LibAudioOut")) _wwiseSdkDetected = true;
          }
          if (!_phyreEngineDetected) {
            if (out.contains("PhyreEngine")) _phyreEngineDetected = true;
          }
          if (!_fmodSdkDetected) {
            if (out.contains("FMOD mixer")) _fmodSdkDetected = true;
          }
          if (!_havokSdkDetected) {
            if (out.contains("HavokWorkerThread")) _havokSdkDetected = true;
          }
        }
      } else if (lineInfo.module == "libSceKernel") {
        if (!_monoSdkDetected) {
          // todo regex?
          if (out.contains(".mono\\config")) _monoSdkDetected = true;
          if (!_monoSdkDetected && out.contains(".mono/config")) _monoSdkDetected = true;
        }
        if (!_unityEngineDetected) {
          if (out.contains("unity default resources")) _unityEngineDetected = true;
        }
        if (!_unrealEngineDetected) {
          if (out.contains("UE3_logo.")) _unrealEngineDetected = true;
        }
      } else if (lineInfo.module == "runtime") {
        if (out.contains("Missing Symbol|")) _missingSymbolDetected = true;
      } else if (lineInfo.module == "Kernel") {
        if (out == "-> client shutdown request") {
          // Stop processing log lines after the Stop button press
          // the rest is unrelated to the game itself.
          return false;
        }
        if (out.starts_with("psOff.")) {
          auto value = out.substr(out.find_first_of('=') + 2);

          if (out.contains(".isNeo = "))
            m_jsonInfo["emu_neo"] = value == "1";
          else if (out.contains(".skipAJM = "))
            m_jsonInfo["emu_skipAjm"] = value == "1";
          else if (out.contains(".skipMovies = "))
            m_jsonInfo["emu_skipMovies"] = value == "1";
          else if (out.contains(".networking = "))
            m_jsonInfo["emu_networking"] = value == "1";
          else if (out.contains(".noElfCheck = "))
            m_jsonInfo["emu_noElfCheck"] = value == "1";
          else if (out.contains(".app.neoSupport = "))
            m_jsonInfo["title_neo"] = value == "1";
          else if (out.contains(".app.id = "))
            m_jsonInfo["title_id"] = value;
          else if (out.contains(".app.title = "))
            m_jsonInfo["title_name"] = value;
        }
      } else if (lineInfo.module == "ExceptionHandler") {
        if (!_exceptionDetected && out.starts_with("Faulty instruction:")) _exceptionDetected = true;
      } else if (lineInfo.module == "libSceSysmodule") {
        if (out.starts_with("loading id = ")) {
          if (!_dialogSdkDetected && out.contains("Dialog")) _dialogSdkDetected = true;
        }
      } else if (lineInfo.module == "libSceNpTrophy") {
        if (out == "Missing trophy key!") _hintTrophyKey = true;
      } else if (lineInfo.module == "elf_loader") {
        if (!_unityEngineDetected && out.contains("Il2CppUserAssemblies")) _unityEngineDetected = true;
        if (out.starts_with("load library[") && out.ends_with(".sprx")) {
          auto start = out.find_last_of("\\/");
          if (start == std::string_view::npos) {
            start = 0;
          } else {
            start += 1;
          }
          m_jsonInfo["firmware"].push_back(out.substr(start));
        }
      } else if (lineInfo.module == "patcher") {
        if (out.starts_with("Applying ") && out.ends_with(" patch")) {
          if (!_hintInsertqPatched && out.contains("ANDN")) _hintAndnPatched = true;
          if (!_hintInsertqPatched && out.contains("INSERTQ")) _hintInsertqPatched = true;
          if (!_hintInsertqPatched && out.contains("EXTRQ")) _hintExtrqPatched = true;
        }
      } else if (!_hintAjmFound && lineInfo.module == "Ajm::Instance") {
        _hintAjmFound = true;
      }
    }
  } else { // Handle main logs
    if (out.contains("Language switched to ")) m_jsonInfo["user-lang"] = out.substr(out.find(" to ") + 4);
    if (!_isGpuPicked && out.contains("Selected GPU:")) {
      _nvidiaHint           = out.contains("NVIDIA") || out.contains("nvidia");
      m_jsonInfo["user-gp"] = out.substr(out.find_first_of(u':') + 1);
    }
    if (!_inputNotFoundHint && out.contains("No pad with specified name was found")) _inputNotFoundHint = true;
    if (lineInfo.module == "sb2spirv") {
      if (!_shaderGenTodo && (out.contains("todo") || out.contains("Instruction missing"))) _shaderGenTodo = true;
    } else if (lineInfo.module == "videoout") {
      if (!_vkValidation && out.contains("Validation Error: ")) _vkValidation = true;
      if (!_vkNoDevices && out == "Failed to find any suitable Vulkan device") _vkNoDevices = true;
    }
  }

  return true;
}

std::string PLogAnalyzer::spit() const {
  return m_jsonInfo.dump(2, ' ', true);
}

std::unique_ptr<PLogAnalyzer> createFileAnalyser(std::filesystem::path const& fpath) {
  return std::make_unique<PLogAnalyzer>(fpath);
}

std::unique_ptr<PLogAnalyzer> createMemAnalyser(const char* memory, size_t size) {
  return std::make_unique<PLogAnalyzer>(memory, size);
}
