#include "p7d.h"

#include <any>
#include <bit>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>

bool P7Dump::run() {
  uint64_t header = 0;
  io_read(&header, sizeof(header));
  if (header == P7D_HDR_BE.raw)
    m_endian = std::endian::big;
  else if (header == P7D_HDR_LE.raw)
    m_endian = std::endian::little;
  else
    throw std::runtime_error("P7Dump: Invalid header");

  read_endian(m_processId);
  read_endian(m_createTime);
  m_processName = fixed_string<p7string>(0x200);
  m_hostName    = fixed_string<p7string>(0x200);

  StreamInfo si;

  while (io_available() >= sizeof(si)) {
    read_endian(si);

    auto currStream = m_streams.find(si.channel);
    if (currStream == m_streams.end()) currStream = m_streams.emplace(std::make_pair((uint8_t)si.channel, StreamStorage())).first;

    while (si.size > sizeof(StreamInfo)) {
      StreamItem item;
      read_endian(item);
      if (item.size == 0) throw std::runtime_error("P7Dump: Unexpected StreamItem of 0 size!");
      si.size -= item.size;
      item.size -= 4;

      switch (item.type) {
        case 0x00: { // STREAM_TRACE
          auto actualRead = processTraceSItem(currStream->second, item);
          if (actualRead == P7D_RENDER_FAIL) return false;

          if (item.size > actualRead) {
            io_skip(item.size - actualRead);
          }
        } break;

        default: {
          io_skip(item.size);
          fprintf(stderr, "Stream %d ignored!\n", item.type);
        } break;
      }
    }
  }

  return true;
}

template <typename T>
static void insert_to_stack(std::vector<char>& stack, T const& value) {
  auto const bytes  = reinterpret_cast<const char*>(&value);
  auto const arsize = sizeof(value);
  stack.insert(stack.end(), bytes, bytes + arsize);
  auto const padding = ((-arsize) & 5);
  for (size_t i = 0; i < padding; ++i)
    stack.push_back('\0');
};

uint32_t P7Dump::processTraceSItem(StreamStorage& stream, StreamItem const& si) {
  uint32_t cread = 0;

  switch (si.subtype) {
    case 0x00: { // Stream Info
      read_endian(stream.info.time), cread += sizeof(stream.info.time);
      read_endian(stream.info.timer), cread += sizeof(stream.info.timer);
      read_endian(stream.info.timer_freq), cread += sizeof(stream.info.timer_freq);
      read_endian(stream.info.flags), cread += sizeof(stream.info.flags);
      stream.info.name = fixed_string<p7string>(0x80), cread += 0x80;
    } break;

    case 0x01: { // Description
      P7Line line;

      uint16_t lineId, numFmt;
      read_endian(lineId), cread += sizeof(lineId);
      read_endian(line.fileLine), cread += sizeof(line.fileLine);
      read_endian(line.moduleId), cread += sizeof(line.moduleId);
      read_endian(numFmt), cread += sizeof(numFmt);

      if (si.size > cread) {
        if (numFmt) {
          auto argSizeByte = numFmt * sizeof(p7argument);
          if (si.size < (cread + argSizeByte)) throw std::runtime_error("P7Dump: Corrupted file");
          line.formatInfos.reserve(numFmt);
          cread += argSizeByte;

          for (uint16_t i = 0; i < numFmt; ++i) {
            auto& data = line.formatInfos.emplace_back(std::make_pair(0, 0));
            read_endian(data.first), read_endian(data.second);
          }
        }

        if (cread < si.size) { // Read format string
          uint32_t consumed = 0;
          line.formatString = zero_string<p7string>(consumed);
          if ((cread += consumed) > si.size) throw std::runtime_error("P7Dump: Corrupted file");
        }

        if (cread < si.size) { // Read filename string
          uint32_t consumed = 0;
          line.fileName     = zero_string<std::string>(consumed);
          if ((cread += consumed) > si.size) throw std::runtime_error("P7Dump: Corrupted file");
        }

        if (cread < si.size) { // Read funcname string
          uint32_t consumed = 0;
          line.funcName     = zero_string<std::string>(consumed);
          if ((cread += consumed) > si.size) throw std::runtime_error("P7Dump: Corrupted file");
        }
      }

      stream.lines.emplace(lineId, std::move(line));
    } break;

    case 0x02: { // Data
      TraceLineData tsd;

      read_endian(tsd.id), cread += sizeof(tsd.id);
      read_endian(tsd.level), cread += sizeof(tsd.level);
      read_endian(tsd.cpu), cread += sizeof(tsd.cpu);
      read_endian(tsd.threadid), cread += sizeof(tsd.threadid);
      read_endian(tsd.sequence), cread += sizeof(tsd.sequence);
      read_endian(tsd.timer), cread += sizeof(tsd.timer);

      auto strinfo = stream.lines.find(tsd.id);
      if (strinfo == stream.lines.end()) {
        fprintf(stderr, "Failed to find format data for parsed stream item!\n");
        break;
      }

      tsd.modid = strinfo->second.moduleId;
      if (strinfo->second.formatInfos.empty()) {
        if (!render(stream, tsd, strinfo->second.formatString)) {
          return P7D_RENDER_FAIL;
        }
        break;
      }

      std::vector<char>   improvised_stack;
      std::list<std::any> improvised_storage;

      for (const auto& [aType, aSize]: strinfo->second.formatInfos) {
        switch (aType) {
          case 0x01:   // char (int8)
          case 0x02:   // char16
          case 0x03:   // int16
          case 0x04:   // int32
          case 0x05:   // int64
          case 0x07:   // pointer
          case 0x0c: { // char32
            int64_t i64;
            insert_to_stack<int64_t>(improvised_stack, read_endian(i64)), cread += sizeof(i64);
          } break;
          case 0x06: { // double
            double dbl;
            insert_to_stack<double>(improvised_stack, read_endian(dbl));
            cread += sizeof(dbl);
          } break;
          case 0x08: { // utf16 string
            p7string u16str;

            char16_t u16char;
            while (true) {
              cread += sizeof(u16char);
              if (read_endian(u16char) != u'\0') {
                u16str.push_back(u16char);
                continue;
              }

              break;
            }

            insert_to_stack<char16_t*>(improvised_stack, std::any_cast<p7string&>(improvised_storage.emplace_back(std::move(u16str))).data());
          } break;
          case 0x09: { // ascii string
            std::string astr;

            char achar;
            while (true) {
              cread += sizeof(achar);
              if (read_endian(achar) != '\0') {
                astr.push_back(achar);
                continue;
              }

              break;
            }

            insert_to_stack<char*>(improvised_stack, std::any_cast<std::string&>(improvised_storage.emplace_back(std::move(astr))).data());
          } break;
          case 0x0a: { // utf8 string
            std::u8string u8str;

            char8_t u8char;
            while (true) {
              cread += sizeof(u8char);
              if (read_endian(u8char) != '\0') {
                u8str.push_back(u8char);
                continue;
              }

              break;
            }

            insert_to_stack<char8_t*>(improvised_stack, std::any_cast<std::u8string&>(improvised_storage.emplace_back(std::move(u8str))).data());
          } break;
          case 0x0b: { // utf32 string
            std::u32string u32str;

            char32_t u32char;
            while (true) {
              cread += sizeof(u32char);
              if (read_endian(u32char) != U'\0') {
                u32str.push_back(u32char);
                continue;
              }

              break;
            }

            insert_to_stack<char32_t*>(improvised_stack, std::any_cast<std::u32string&>(improvised_storage.emplace_back(std::move(u32str))).data());
          } break;
          default: {
            throw std::runtime_error(std::format("Unknown argument: {}!", si.subtype));
          } break;
        }
      }

      int32_t len = vswprintf(nullptr, 0, (const wchar_t*)strinfo->second.formatString.c_str(), (va_list)improvised_stack.data());

      p7string outstr;
      outstr.resize(len + 1);
      vswprintf((wchar_t*)outstr.data(), outstr.size(), (const wchar_t*)strinfo->second.formatString.c_str(), (va_list)improvised_stack.data());
      if (!render(stream, tsd, outstr)) {
        return P7D_RENDER_FAIL;
      }
    } break;

    case 0x03: { // Verb packet? Wtf is this, dunno
    } break;

    case 0x04: { // Close packet, we don't really care about those
    } break;

    case 0x09: { // UTC offset packet, used for time sync
    } break;

    case 0x07: { // Module
      int16_t  mod_id;
      P7Module mod;
      read_endian(mod_id), cread += sizeof(mod_id);
      read_endian(mod.verbLevel), cread += sizeof(mod.verbLevel);
      mod.name = fixed_string<std::string>(54), cread += 54;
      stream.modules.emplace(std::make_pair(mod_id, std::move(mod)));
    } break;

    default: {
      fprintf(stderr, "Trace stream item %u:%u ignored!\n", si.subtype, si.size);
    } break;
  }

  return cread;
}
