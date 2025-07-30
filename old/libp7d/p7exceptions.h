#pragma once

#include <exception>
#include <format>
#include <string>

class P7DumpInvalidHeaderException: public std::exception {
  public:
  P7DumpInvalidHeaderException() {}

  const char* what() const final { return "P7Dump: Invalid header"; }
};

class P7DumpNotEnoughBufferSpaceException: public std::exception {
  public:
  P7DumpNotEnoughBufferSpaceException(size_t avail, size_t required, bool isSkip) {
    m_str = std::format("P7Dump: Failed to {} {} bytes, only {} is available", isSkip ? "skip" : "read", required, avail);
  }

  const char* what() const final { return m_str.c_str(); }

  private:
  std::string m_str;
};

class P7DumpBrokenStreamItemException: public std::exception {
  public:
  P7DumpBrokenStreamItemException(uint32_t size, uint32_t subtype, uint32_t type) {
    m_str = std::format("P7Dump: Failed to validate StreamItem(size: {}, type: {}, subtype: {})", size, type, subtype);
  }

  const char* what() const final { return m_str.c_str(); }

  private:
  std::string m_str;
};

class P7DumpCorruptedItemException: public std::exception {
  public:
  P7DumpCorruptedItemException(const char* itemName) { m_str = std::format("P7Dump: Corrupted stream item data for {}", itemName); }

  const char* what() const final { return m_str.c_str(); }

  private:
  std::string m_str;
};

class P7DumpUnknownArgumentException: public std::exception {
  public:
  P7DumpUnknownArgumentException(uint32_t subtype) { m_str = std::format("Unknown argument: {}", subtype); }

  const char* what() const final { return m_str.c_str(); }

  private:
  std::string m_str;
};
