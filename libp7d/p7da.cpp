#include "p7da.h"

#include <codecvt>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <locale>
#include <memory>
#include <stdexcept>
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

void P7DumpAnalyser::render(StreamStorage& stream, TraceLineData const& tsd, p7string const& out) {
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
              "hints",
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
    if (stream.info.name.contains(u"tty")) {
      if (!_gmakerEngineDetected && out.contains(u"YoYo Games PS4 Runner")) _gmakerEngineDetected = true;
      if (!_unrealEngineDetected && out.starts_with(u"Additional") && out.contains(u".uproject")) _unrealEngineDetected = true;
      if (!_unrealEngineDetected && out.contains(u"uecommandline.txt")) _unrealEngineDetected = true;
    } else {
      if (mod.name.contains("pthread")) {
        if (out.starts_with(u"--> thread")) { // Thread run log
          if (!_unityEngineDetected) {
            if (out.contains(u"UnityWorker")) _unityEngineDetected = true;
            if (!_unityEngineDetected && out.contains(u"UnityGfx")) _unityEngineDetected = true;
            if (!_criSdkDetected && (out.contains(u"CriThread") || out.contains(u"CRI FS"))) _criSdkDetected = true;
            if (!_wwiseSdkDetected && out.contains(u"Wwise")) _wwiseSdkDetected = true;
          }
          if (!_phyreEngineDetected) {
            if (out.contains(u"PhyreEngine")) _phyreEngineDetected = true;
          }
          if (!_fmodSdkDetected) {
            if (out.contains(u"FMOD mixer")) _fmodSdkDetected = true;
          }
        }
      } else if (mod.name.contains("libSceKernel")) {
        if (!_monoSdkDetected) {
          if (out.contains(u".mono/config")) _monoSdkDetected = true;
        }
        if (!_unityEngineDetected) {
          if (out.contains(u"/unity default resources")) _unityEngineDetected = true;
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
      } else if (mod.name.contains("ExceptionHandler")) {
        if (!_exceptionDetected && out.starts_with(u"Faulty instruction:")) _exceptionDetected = true;
      } else if (mod.name.contains("libSceSysmodule")) {
        if (out.starts_with(u"loading id = ")) {
          if (!_dialogSdkDetected && out.contains(u"Dialog")) _dialogSdkDetected = true;
        }
      } else if (mod.name.contains("libSceNpTrophy")) {
        if (out == u"Missing trophy key!") _hintTrophyKey = true;
      } else if (mod.name.contains(("elf_loader"))) {
        if (!_unityEngineDetected && out.contains(u"Il2CppUserAssemblies")) _unityEngineDetected = true;
      }
    }
  } else { // Handle main logs
    if (out.contains(u"Language switched to ")) m_jsonInfo["user-lang"] = toUTF8(std::basic_string_view<char16_t>(out.c_str() + out.find(u" to ") + 4));
    if (!_isGpuPicked && out.contains(u"Selected GPU:")) {
      _nvidiaHint            = out.contains(u"NVIDIA") || out.contains(u"nvidia");
      m_jsonInfo["user-gpu"] = toUTF8(std::basic_string_view<char16_t>(out.c_str() + out.find_first_of(u':') + 1));
    }
    if (!_inputNotFoundHint && out.contains(u"No pad with specified name was found")) _inputNotFoundHint = true;
    if (!_shaderGenTodo && (mod.name == "sb2spirv") && (out.contains(u"todo") || out.contains(u"Instruction missing"))) _shaderGenTodo = true;
    if (!_vkValidation && (mod.name == "videoout") && out.contains(u"Validation Error: ")) _vkValidation = true;
  }
}

void P7DumpAnalyser::run() {
  P7Dump::run();

  auto& labels = m_jsonInfo["labels"];
  auto& hints  = m_jsonInfo["hints"];

  if (_isChildprocess) {
    if (_unityEngineDetected) labels.push_back("engine-unity");
    if (_unrealEngineDetected) labels.push_back("engine-unreal");
    if (_cryEngineDetected) labels.push_back("engine-cry");
    if (_phyreEngineDetected) labels.push_back("engine-phyre");
    if (_gmakerEngineDetected) labels.push_back("engine-gamemaker");
    if (_exceptionDetected) labels.push_back("exception");
    if (_fmodSdkDetected) labels.push_back("sdk-fmod");
    if (_monoSdkDetected) labels.push_back("sdk-mono");
    if (_criSdkDetected) labels.push_back("sdk-criware");
    if (_wwiseSdkDetected) labels.push_back("sdk-wwise");
  } else {
    if (_inputNotFoundHint)
      hints.push_back("One of your users has the input device set incorrectly, if you can't control the PS4 app, this could be the cause.");
    if (_nvidiaHint)
      hints.push_back("You are using an NVIDIA graphics card, these cards have many issues on our emulator that may not be present on AMD cards.");
    if (_vkValidation) labels.push_back("graphics");
    if (_shaderGenTodo) labels.push_back("shader-gen");
  }

  if (_hintTrophyKey)
    hints.push_back("You don't have the trophy key installed, this can cause problems in games, also you won't be able to see the list of trophies you have "
                    "received. To solve this problem, check #faq channel in on Discord Server.");
}

std::string P7DumpAnalyser::spit() const {
  return m_jsonInfo.dump(2, ' ', true);
}

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/bind.h>

EMSCRIPTEN_BINDINGS(EM_libp7d) {
  emscripten::class_<P7DumpAnalyser>("P7DumpAnalyser")
      .constructor<std::filesystem::path const&>()
      .function("run", &P7DumpAnalyser::run)
      .function("spit", &P7DumpAnalyser::spit);
}
#else
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
    if ((m_memCurPos + nread) > m_memSize) throw std::runtime_error("Too big block read requested");
    std::memcpy(buffer, (char*)m_memPtr + m_memCurPos, nread);
    m_memCurPos += nread;
  }

  void io_skip(size_t nbytes) override final {
    if ((m_memCurPos + nbytes) > m_memSize) throw std::runtime_error("Too many bytes skip requested");
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
#endif
