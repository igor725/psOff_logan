#pragma once

#include <algorithm>
#include <bit>
#include <cstdint>
#include <list>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

union P7Header {
  uint8_t  data[8];
  uint64_t raw;
};

constexpr P7Header P7D_HDR_LE      = {.data = {0xa6, 0x2c, 0xf3, 0xec, 0x71, 0xac, 0xd2, 0x45}};
constexpr P7Header P7D_HDR_BE      = {.data = {0x45, 0xd2, 0xac, 0x71, 0xec, 0xf3, 0x2c, 0xa6}};
constexpr uint32_t P7D_RENDER_FAIL = (uint32_t)-1;

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

  P7Dump() = default;

  virtual ~P7Dump() = default;

  virtual size_t io_available() const = 0;

  virtual void io_read(void* buffer, size_t nread) = 0;

  virtual void io_skip(size_t nbytes) = 0;

  virtual bool run();

  virtual bool render(StreamStorage& stream, TraceLineData const& tsd, p7string const& out) = 0;

  virtual std::string spit() const = 0;

  static inline bool check_header(uint64_t header) { return P7D_HDR_BE.raw == header || P7D_HDR_LE.raw == header; }

  private:
  template <typename T>
  struct AlignedBytes {
    alignas(T) unsigned char data[sizeof(T)];
  };

  template <typename T>
  constexpr T swap_endian(T value) noexcept {
    static_assert(std::is_trivially_copyable_v<T>, "value is not regular type");

    AlignedBytes<T> bytes = std::bit_cast<AlignedBytes<T>>(value);
    std::reverse(std::begin(bytes.data), std::end(bytes.data));
    return std::bit_cast<T>(bytes);
  }

  template <typename T>
  constexpr T& read_endian(T& buf) {
    io_read(&buf, sizeof(buf));
    if ((sizeof(T) > 1) && (m_endian != std::endian::native)) buf = swap_endian(buf);
    return buf;
  }

  template <typename T>
  T zero_string(uint32_t& consumed) {
    T temp;

    while (io_available() > 0) {
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

    io_skip(bytesize);
    return std::move(temp);
  }

  uint32_t processTraceSItem(StreamStorage& ss, StreamItem const& pi);

  std::unordered_map<uint8_t, StreamStorage> m_streams;

  protected:
  std::endian m_endian;
  uint32_t    m_processId  = 0;
  uint64_t    m_createTime = 0;

  p7string m_processName, m_hostName;
};
