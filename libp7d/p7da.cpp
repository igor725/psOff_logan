#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#define _LIBCPP_DISABLE_DEPRECATION_WARNINGS

#include "p7da.h"

#include "p7exceptions.h"

#include <codecvt>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <locale>
#include <memory>
#include <string_view>

namespace {
std::string toUTF8(std::basic_string<char16_t> const& source) {
  std::string result;

  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convertor;
  result = convertor.to_bytes(source);

  return result;
}

auto toUTF8(std::basic_string_view<char16_t> source) {
  return toUTF8(std::basic_string<char16_t>(source));
}
} // namespace

bool P7DumpAnalyser::render(StreamStorage& stream, TraceLineData const& tsd, p7string const& out) {
  if (!_processTypeGuessed) {
    _processTypeGuessed = true;
    if ((_isChildprocess = (m_processName == u"psOff_tunnel.exe")) == true) { // Prepare child process things
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
          {"user-gpu", "UNDETECTED"},
          {"user-lang", "UNDETECTED"},
      };
    }
  }

  auto& mod = stream.modules[tsd.modid];

  if (_isChildprocess) { // Handle child logs
    if (stream.info.name.contains(u"tty")) {
      if (!_gmakerEngineDetected && out.contains(u"YoYo Games PS4 Runner")) _gmakerEngineDetected = true;
      if (!_irrlichtEngineDetected && out.contains(u"Irrlicht Engine")) _irrlichtEngineDetected = true;
      if (!_unrealEngineDetected && out.starts_with(u"Additional") && out.contains(u".uproject")) _unrealEngineDetected = true;
      if (!_unrealEngineDetected && out.contains(u"uecommandline.txt")) _unrealEngineDetected = true;
      if (!_naughtyEngineDetected && out.contains(u"ND File Server")) _naughtyEngineDetected = true;
      if (!_naughtyEngineDetected && out.contains(u"----- Switching world: from")) _naughtyEngineDetected = true;
    } else {
      if (out.starts_with(u"todo ")) {
        if (!_netStuffDetected && out.starts_with(u"todo sceNp")) _netStuffDetected = true;
        return true;
      }

      if (mod.name == "pthread") {
        if (out.starts_with(u"--> thread")) { // Thread run log
          if (!_unityEngineDetected) {
            if (out.contains(u"UnityWorker")) _unityEngineDetected = true;
            if (!_unityEngineDetected && out.contains(u"UnityGfx")) _unityEngineDetected = true;
          }
          if (!_criSdkDetected) {
            if (out.contains(u"CriThread") || out.contains(u"CRI FS")) _criSdkDetected = true;
          }
          if (!_wwiseSdkDetected) {
            if (out.contains(u"Wwise")) _wwiseSdkDetected = true;
            if (!_wwiseSdkDetected && out.contains(u"AK::LibAudioOut")) _wwiseSdkDetected = true;
          }
          if (!_phyreEngineDetected) {
            if (out.contains(u"PhyreEngine")) _phyreEngineDetected = true;
          }
          if (!_fmodSdkDetected) {
            if (out.contains(u"FMOD mixer")) _fmodSdkDetected = true;
          }
          if (!_havokSdkDetected) {
            if (out.contains(u"HavokWorkerThread")) _havokSdkDetected = true;
          }
        }
      } else if (mod.name == "libSceKernel") {
        if (!_monoSdkDetected) {
          // todo regex?
          if (out.contains(u".mono\\config")) _monoSdkDetected = true;
          if (!_monoSdkDetected && out.contains(u".mono/config")) _monoSdkDetected = true;
        }
        if (!_unityEngineDetected) {
          if (out.contains(u"unity default resources")) _unityEngineDetected = true;
        }
        if (!_unrealEngineDetected) {
          if (out.contains(u"UE3_logo.")) _unrealEngineDetected = true;
        }
      } else if (mod.name == "Kernel") {
        if (out.starts_with(u"psOff.")) {
          auto value = std::basic_string_view<char16_t>(out.c_str() + out.find_first_of('=') + 2);

          if (out.contains(u".isNeo = "))
            m_jsonInfo["emu_neo"] = value == u"1";
          else if (out.contains(u".skipAJM = "))
            m_jsonInfo["emu_skipAjm"] = value == u"1";
          else if (out.contains(u".skipMovies = "))
            m_jsonInfo["emu_skipMovies"] = value == u"1";
          else if (out.contains(u".networking = "))
            m_jsonInfo["emu_networking"] = value == u"1";
          else if (out.contains(u".noElfCheck = "))
            m_jsonInfo["emu_noElfCheck"] = value == u"1";
          else if (out.contains(u".app.neoSupport = "))
            m_jsonInfo["title_neo"] = value == u"1";
          else if (out.contains(u".app.id = "))
            m_jsonInfo["title_id"] = toUTF8(value);
          else if (out.contains(u".app.title = "))
            m_jsonInfo["title_name"] = toUTF8(value);
        }
      } else if (mod.name == "ExceptionHandler") {
        if (!_exceptionDetected && out.starts_with(u"Faulty instruction:")) _exceptionDetected = true;
      } else if (mod.name == "libSceSysmodule") {
        if (out.starts_with(u"loading id = ")) {
          if (!_dialogSdkDetected && out.contains(u"Dialog")) _dialogSdkDetected = true;
        }
      } else if (mod.name == "libSceNpTrophy") {
        if (out == u"Missing trophy key!") _hintTrophyKey = true;
      } else if (mod.name == "elf_loader") {
        if (!_unityEngineDetected && out.contains(u"Il2CppUserAssemblies")) _unityEngineDetected = true;
        if (out.starts_with(u"load library[") && out.ends_with(u".sprx")) {
          auto start = out.find_last_of(u"\\/");
          if (start == p7string::npos) {
            start = 0;
          } else {
            start += 1;
          }
          m_jsonInfo["firmware"].push_back(toUTF8(std::basic_string_view<char16_t>(out.c_str()).substr(start)));
        }
      } else if (mod.name == "patcher") {
        if (out.starts_with(u"Applying ") && out.ends_with(u" patch")) {
          if (!_hintInsertqPatched && out.contains(u"ANDN")) _hintAndnPatched = true;
          if (!_hintInsertqPatched && out.contains(u"INSERTQ")) _hintInsertqPatched = true;
          if (!_hintInsertqPatched && out.contains(u"EXTRQ")) _hintExtrqPatched = true;
        }
      } else if (!_hintAjmFound && mod.name == "Ajm::Instance") {
        _hintAjmFound = true;
      }
    }
  } else { // Handle main logs
    if (out.contains(u"Language switched to ")) m_jsonInfo["user-lang"] = toUTF8(std::basic_string_view<char16_t>(out.c_str() + out.find(u" to ") + 4));
    if (!_isGpuPicked && out.contains(u"Selected GPU:")) {
      _nvidiaHint            = out.contains(u"NVIDIA") || out.contains(u"nvidia");
      m_jsonInfo["user-gpu"] = toUTF8(std::basic_string_view<char16_t>(out.c_str() + out.find_first_of(u':') + 1));
    }
    if (!_inputNotFoundHint && out.contains(u"No pad with specified name was found")) _inputNotFoundHint = true;
    if (mod.name == "sb2spirv") {
      if (!_shaderGenTodo && (out.contains(u"todo") || out.contains(u"Instruction missing"))) _shaderGenTodo = true;
    } else if (mod.name == "videoout") {
      if (!_vkValidation && out.contains(u"Validation Error: ")) _vkValidation = true;
      if (!_vkNoDevices && out == u"Failed to find any suitable Vulkan device") _vkNoDevices = true;
    }
  }

  return true;
}

bool P7DumpAnalyser::run() {
  if (P7Dump::run()) {
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

    return true;
  }

  return false;
}

std::string P7DumpAnalyser::spit() const {
  return m_jsonInfo.dump(2, ' ', true);
}

class P7DumpFileAnalyser: public P7DumpAnalyser {
  public:
  P7DumpFileAnalyser(std::filesystem::path const& fpath): P7DumpAnalyser(), m_file(fpath, std::ios::in | std::ios::binary) {
    m_file.seekg(0, std::ios::end);
    m_fileSize = m_file.tellg();
    m_file.seekg(0, std::ios::beg);
  };

  size_t io_available() const override final {
    auto cpos = m_file.tellg();
    if (cpos > m_fileSize) return 0ull;
    return m_fileSize - cpos;
  }

  void io_read(void* buffer, size_t nread) override final { m_file.read((char*)buffer, nread); }

  void io_skip(size_t nbytes) override final { m_file.seekg(nbytes, std::ios::cur); }

  private:
  std::ios::pos_type   m_fileSize;
  mutable std::fstream m_file;
};

std::unique_ptr<P7Dump> createFileAnalyser(std::filesystem::path const& fpath) {
  return std::make_unique<P7DumpFileAnalyser>(fpath);
}

class P7DumpMemAnalyser: public P7DumpAnalyser {
  public:
  P7DumpMemAnalyser(void* memory, size_t size): P7DumpAnalyser(), m_memPtr(memory), m_memSize(size), m_memCurPos(0) {};

  size_t io_available() const override final {
    if (m_memCurPos > m_memSize) return 0ull;
    return m_memSize - m_memCurPos;
  }

  void io_read(void* buffer, size_t nread) override final {
    if ((m_memCurPos + nread) > m_memSize) throw P7DumpNotEnoughBufferSpaceException(m_memSize - m_memCurPos, nread, false);
    std::memcpy(buffer, (char*)m_memPtr + m_memCurPos, nread);
    m_memCurPos += nread;
  }

  void io_skip(size_t nbytes) override final {
    if ((m_memCurPos + nbytes) > m_memSize) throw P7DumpNotEnoughBufferSpaceException(m_memSize - m_memCurPos, nbytes, true);
    m_memCurPos += nbytes;
  }

  private:
  void*  m_memPtr;
  size_t m_memSize;
  size_t m_memCurPos;
};

std::unique_ptr<P7Dump> createMemAnalyser(void* memory, size_t size) {
  return std::make_unique<P7DumpMemAnalyser>(memory, size);
}
