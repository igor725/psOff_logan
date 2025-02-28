#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <list>
#include <string>
#include <unordered_map>

constexpr uint64_t P7D_HDR_LE = 5031273339032906918ull;
constexpr uint64_t P7D_HDR_BE = 11974213706116289093ull;

class P7Dump {
  public:
  using p7string   = std::basic_string<char16_t>;
  using p7argument = std::pair<uint8_t, uint8_t>;

  struct P7Line {
    uint16_t fileLine;
    uint16_t moduleId;

    std::vector<p7argument> formatInfos;
    p7string                formatString;
    std::string             fileName;
    std::string             funcName;
  };

  struct P7Module {
    uint32_t    verbLevel;
    std::string name;
  };

  private:
  union StreamInfo {
    struct {
      uint32_t size    : 27;
      uint32_t channel : 5;
    };

    uint32_t _raw;
  };

  struct StreamItem {
    uint32_t type    : 5;
    uint32_t subtype : 5;
    uint32_t size    : 22;
  };

  struct TraceStreamInfo {
    uint64_t time;
    uint64_t timer;
    uint64_t timer_freq;
    uint64_t flags;
    p7string name;
  };

  public:
  struct TraceLineData {
    uint16_t id;
    uint16_t modid;
    uint8_t  level;
    uint8_t  cpu;
    uint32_t threadid;
    uint32_t sequence;
    uint64_t timer;
  };

  struct StreamStorage {
    TraceStreamInfo info;

    std::unordered_map<uint16_t, P7Line>   lines;
    std::unordered_map<uint16_t, P7Module> modules;
    std::list<p7string>                    formatted;
  };

  P7Dump(std::filesystem::path const& fpath);

  virtual ~P7Dump() = default;

  void run();

  virtual void render(StreamStorage& stream, TraceLineData const& tsd, p7string const& out) = 0;

  private:
  template <typename T>
  T& read(T& buf) {
    m_file.read((char*)&buf, sizeof(buf));
    return buf;
  }

  template <typename T>
  T& read_endian(T& buf) {
    m_file.read((char*)&buf, sizeof(buf));
    if (m_isBigEndian) buf = std::byteswap(buf);
    return buf;
  }

  template <typename T>
  T zero_string(uint32_t& consumed) {
    T temp;

    while (!m_file.eof()) {
      typename T::value_type ch;
      read_endian(ch);
      consumed += sizeof(ch);
      if (ch == '\0') break;
      temp.push_back(ch);
    }

    return std::move(temp);
  }

  template <typename T>
  T fixed_string(uint32_t bytesize) {
    T temp;

    while (bytesize > 0) {
      typename T::value_type ch;
      read_endian(ch);
      bytesize -= sizeof(ch);
      if (ch == '\0') break;
      temp.push_back(ch);
    }

    skip(bytesize);
    return std::move(temp);
  }

  void skip(uint32_t bytes) { m_file.seekg(bytes, std::ios::cur); }

  uint32_t processTraceSItem(StreamStorage& ss, StreamItem const& pi);

  std::fstream m_file;

  std::unordered_map<uint8_t, StreamStorage> m_streams;

  protected:
  bool     m_isBigEndian = false;
  uint32_t m_processId   = 0;
  uint64_t m_createTime  = 0;

  p7string m_processName, m_hostName;
};
