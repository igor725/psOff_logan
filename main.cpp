#include "zipconf.h"

#include <iostream>
#define NOMINMAX

#include "libp7d/p7d.h"
#include "libp7d/p7da.h"

#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <format>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <winhttp.h>
#include <zip.h>

enum LogAnExitCodes : int32_t {
  Success,

  // Internal errors
  ArgumentFail,
  PathConversion,
  NoAnalyser,
  BufferFail,
  _InternalErrorsEnd = 100,

  // HTTP-related
  UrlParse,
  HttpOpen,
  HttpConnect,
  HttpRequest,
  HttpRequestSend,
  HttpResponse,
  HttpHeaders,
  HttpRetryFail,
  HttpTooMuch,
  _HttpErrorsEnd = 200,

  // Zip-related
  ZipSource,
  ZipOpen,
  ZipNoFile,
  ZipUnexpected,
  _ZipErrorsEnd = 300,
};

static void runAnalyser(std::unique_ptr<P7Dump> const& analyser) {
  try {
    if (analyser->run()) {
      printf("%s", analyser->spit().c_str());
      return;
    }

    printf("analyser->render() call finished with error!\n");
  } catch (std::exception const& ex) {
    fprintf(stderr, "P7Dump exception: %s\n", ex.what());
  }
}

int32_t main(int32_t argc, char* argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <p7d file path> [--noblock]", argv[0]);
    return LogAnExitCodes::ArgumentFail;
  }

  if (auto argLink = std::string_view(argv[1]); !argLink.empty()) {
    std::unique_ptr<P7Dump> analyser;
    std::vector<char>       growingdata, unpdata;

    if (argLink.starts_with("http")) {
      int32_t need = MultiByteToWideChar(CP_UTF8, 0, argLink.data(), -1, nullptr, 0);
      if (need <= 0) {
        fprintf(stderr, "Failed to determine widechar length of URL\n");
        return LogAnExitCodes::PathConversion;
      }

      std::wstring wideUrl;
      wideUrl.resize(need);
      if (MultiByteToWideChar(CP_UTF8, 0, argLink.data(), -1, wideUrl.data(), need) != need) {
        fprintf(stderr, "Failed to convert URL, stack corruption?\n");
        return LogAnExitCodes::PathConversion;
      }
      wideUrl.pop_back();

      constexpr size_t maxDomainSize = 256;
      constexpr size_t maxPathSize   = 512;

      char buffer[(maxDomainSize + maxPathSize) * sizeof(wchar_t)];

      URL_COMPONENTSW uc = {
          .dwStructSize      = sizeof(URL_COMPONENTSW),
          .lpszScheme        = nullptr,
          .dwSchemeLength    = 0,
          .lpszHostName      = (wchar_t*)buffer,
          .dwHostNameLength  = maxDomainSize,
          .lpszUserName      = nullptr,
          .dwUserNameLength  = 0,
          .lpszPassword      = nullptr,
          .dwPasswordLength  = 0,
          .lpszUrlPath       = (wchar_t*)(buffer + maxDomainSize * sizeof(wchar_t)),
          .dwUrlPathLength   = maxPathSize,
          .lpszExtraInfo     = nullptr,
          .dwExtraInfoLength = 0,
      };

      if (WinHttpCrackUrl(wideUrl.c_str(), wideUrl.length(), 0, &uc) != TRUE) {
        fprintf(stderr, "Failed to parse URL: %lu\n", GetLastError());
        return LogAnExitCodes::UrlParse;
      }

      HINTERNET hSess = WinHttpOpen(L"psOff_logan/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

      if (hSess == NULL) {
        fprintf(stderr, "Failed to build HTTP session: %lu\n", GetLastError());
        return LogAnExitCodes::HttpOpen;
      }

      DWORD redirPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
      WinHttpSetOption(hSess, WINHTTP_OPTION_REDIRECT_POLICY, &redirPolicy, sizeof(redirPolicy));

      HINTERNET hConn = WinHttpConnect(hSess, uc.lpszHostName, uc.nPort, 0);
      if (hConn == NULL) {
        fprintf(stderr, "Failed to connect to server: %lu\n", GetLastError());
        return LogAnExitCodes::HttpConnect;
      }

      DWORD orflags = WINHTTP_FLAG_REFRESH;
      if (uc.nScheme == INTERNET_SCHEME_HTTPS) orflags |= WINHTTP_FLAG_SECURE;

      HINTERNET hRequ = WinHttpOpenRequest(hConn, L"GET", uc.lpszUrlPath, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, orflags);
      if (hRequ == NULL) {
        fprintf(stderr, "Failed to open request to server: %lu\n", GetLastError());
        return LogAnExitCodes::HttpRequest;
      }

      if (!WinHttpSendRequest(hRequ, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        fprintf(stderr, "Failed to send request to server: %lu\n", GetLastError());
        return LogAnExitCodes::HttpRequestSend;
      }

      if (!WinHttpReceiveResponse(hRequ, NULL)) {
        fprintf(stderr, "Failed to receive response from server: %lu\n", GetLastError());
        return LogAnExitCodes::HttpResponse;
      }

      size_t csize;
      DWORD  csizeLen = sizeof(csize);
      if (!WinHttpQueryHeaders(hRequ, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER64, WINHTTP_HEADER_NAME_BY_INDEX, &csize, &csizeLen,
                               WINHTTP_NO_HEADER_INDEX)) {
        DWORD error = GetLastError();
        if (error != ERROR_WINHTTP_HEADER_NOT_FOUND) {
          fprintf(stderr, "Failed to receive response length from server: %lu\n", GetLastError());
          return LogAnExitCodes::HttpHeaders;
        }

        csize = 0;
      }

      std::memset(buffer, 0, sizeof(buffer));

      void*  outdata     = nullptr;
      size_t outdatasize = 0;
      DWORD  availdata   = 0;
      DWORD  downloaded  = 0;

      if ((csize == 0) || (csize > sizeof(buffer))) { // Growing case
        growingdata.reserve(csize);

        do {
          WinHttpQueryDataAvailable(hRequ, &availdata);
          if (availdata == 0) break;
          availdata = std::min((DWORD)sizeof(buffer), availdata); // Shrink to our buffer size

          uint32_t retries = 0;
        retry_grow:
          downloaded = 0;
          if (!WinHttpReadData(hRequ, buffer, availdata, &downloaded)) {
            if (++retries > 15) {
              fprintf(stderr, "Server read failed after 15 retries!\n");
              return LogAnExitCodes::HttpRetryFail;
            }

            if (downloaded == 0) {
              fprintf(stderr, "Server read failed, retrying in 5 seconds...\n");
              std::this_thread::sleep_for(std::chrono::seconds(5));
              goto retry_grow;
            }
          }

          if ((csize > 0) && ((growingdata.size() + downloaded) > csize)) {
            fprintf(stderr, "Server sent way more data than it should!\n");
            return LogAnExitCodes::HttpTooMuch;
          }

          growingdata.insert(growingdata.end(), std::begin(buffer), std::begin(buffer) + downloaded);
        } while (true);

        outdata     = growingdata.data();
        outdatasize = growingdata.size();
      } else { // Static case
        char*  bufpos  = buffer;
        size_t bufleft = sizeof(buffer);

        do {
          WinHttpQueryDataAvailable(hRequ, &availdata);
          if (availdata == 0) break;
          if (bufleft < availdata) {
            fprintf(stderr, "Server sent way more data than it should!\n");
            return LogAnExitCodes::HttpTooMuch;
          }

          uint32_t retries = 0;
        retry:
          downloaded = 0;
          if (!WinHttpReadData(hRequ, bufpos, availdata, &downloaded)) {
            if (++retries > 15) {
              fprintf(stderr, "Server read failed after 15 retries!\n");
              return LogAnExitCodes::HttpRetryFail;
            }

            if (downloaded == 0) {
              fprintf(stderr, "Server read failed, retrying in 5 seconds...\n");
              std::this_thread::sleep_for(std::chrono::seconds(5));
              goto retry;
            }
          }

          if (downloaded > 0) {
            bufpos += downloaded;
            bufleft -= downloaded;
          }
        } while (bufleft > 0);

        outdata     = buffer;
        outdatasize = sizeof(buffer) - bufleft;
      }

      if (outdata != nullptr && outdatasize > 0) {
        if (std::memcmp(outdata, "PK", 2) == 0) { // Most likely Zip archive, handle it
          zip_error_t   zerr;
          zip_source_t* zsrc;
          zip_t*        zarc;

          zip_error_init(&zerr);
          if ((zsrc = zip_source_buffer_create(outdata, outdatasize, 0, &zerr)) == nullptr) {
            fprintf(stderr, "Failed to create zip source: %s\n", zip_error_strerror(&zerr));
            zip_error_fini(&zerr);
            return LogAnExitCodes::ZipSource;
          }

          if ((zarc = zip_open_from_source(zsrc, 0, &zerr)) == nullptr) {
            fprintf(stderr, "Failed to open zip: %s\n", zip_error_strerror(&zerr));
            zip_source_free(zsrc);
            zip_error_fini(&zerr);
            return LogAnExitCodes::ZipOpen;
          }

          zip_error_fini(&zerr);

          struct MenuEntry {
            zip_int64_t  index;
            zip_uint64_t size;
            std::wstring name;
          };

          std::vector<MenuEntry> files;

          zip_int64_t num_files = zip_get_num_entries(zarc, 0);
          for (zip_int64_t i = 0; i < num_files; ++i) {
            zip_stat_t sb;
            if (zip_stat_index(zarc, i, 0, &sb) < 0) {
              fprintf(stderr, "Failed to stat zip file#%lld: %s\n", i, zip_strerror(zarc));
              continue;
            }

            if (sb.size == 0) {
              fprintf(stderr, "File %s skipped: %s\n", sb.name, "it's empty");
              continue;
            }
            if (!std::string_view(sb.name).ends_with(".p7d")) {
              fprintf(stderr, "File %s skipped: %s\n", sb.name, "not a .p7d file");
              continue;
            }

            zip_file_t* zf = zip_fopen_index(zarc, i, 0);
            if (zf == nullptr) {
              fprintf(stderr, "File %s skipped: %s\n", sb.name, zip_strerror(zarc));
              continue;
            }

            uint64_t header;
            if (zip_fread(zf, &header, sizeof(header)) != sizeof(header)) {
              fprintf(stderr, "File %s skipped: %s\n", sb.name, zip_strerror(zarc));
              zip_fclose(zf);
              continue;
            }

            if (!P7Dump::check_header(header)) {
              fprintf(stderr, "File %s skipped: %s\n", sb.name, "not a p7d file");
              zip_fclose(zf);
              continue;
            }

            char p7header[0x0c + 0x200 /* time + process name in header */];
            if (zip_fread(zf, p7header, sizeof(p7header)) != sizeof(p7header)) { // Read whole p7d header
              fprintf(stderr, "File %s skipped: %s\n", sb.name, zip_strerror(zarc));
              zip_fclose(zf);
              continue;
            }

            files.emplace_back(i, sb.size, (wchar_t*)(p7header + 0x0c /* timedata offset => start of process name */));
            zip_fclose(zf);
          }

          if (files.empty()) {
            zip_close(zarc);
            zip_source_close(zsrc);
            fprintf(stderr, "No p7d files found in zip archive!\n");
            return LogAnExitCodes::ZipNoFile;
          }

          auto unpackZipFile = [&unpdata, zarc, zsrc](MenuEntry& lf) {
            zip_file_t* zf = zip_fopen_index(zarc, lf.index, 0);

            if (zf == nullptr) {
              fprintf(stderr, "Failed to open zip file: %s\n", zip_strerror(zarc));
              zip_close(zarc);
              zip_source_close(zsrc);
              return LogAnExitCodes::ZipUnexpected;
            }

            unpdata.resize(lf.size);
            if (zip_fread(zf, unpdata.data(), unpdata.size()) != lf.size) {
              fprintf(stderr, "Failed to read zip file: %s\n", zip_strerror(zarc));
              zip_close(zarc);
              zip_source_close(zsrc);
              return LogAnExitCodes::ZipUnexpected;
            }

            zip_fclose(zf);
            return LogAnExitCodes::Success;
          };

          if (files.size() > 1) { // Entering interactive mode
            while (true) {
              std::cout << "\x1b[0;0H\x1b[2J0. [Exit]" << std::endl;
              for (auto it = files.begin(); it != files.end(); ++it) {
                std::wcout << std::format(L"{}. {} ({} bytes)", std::distance(files.begin(), it) + 1, it->name, it->size) << std::endl;
              }

              std::cout << std::endl << "Enter index: ";
              int32_t index = 0;
              std::wcin >> index;
              getchar(); // Skip newline
              if (index == 0) break;
              if (index > files.size()) continue;
              if (unpackZipFile(files[index - 1]) != LogAnExitCodes::Success) {
              }
              std::cout << "\x1b[0;0H\x1b[2J";
              runAnalyser(createMemAnalyser(unpdata.data(), unpdata.size()));
              unpdata.clear();
              std::cout << std::endl << "Press enter to go back...";
              while (getchar() != '\n')
                ;
            }

            zip_close(zarc);
            zip_source_close(zsrc);
            return LogAnExitCodes::Success;
          } else {
            if (unpackZipFile(files.front()) != LogAnExitCodes::Success) {
              zip_close(zarc);
              zip_source_close(zsrc);
              return LogAnExitCodes::ZipUnexpected;
            }
            outdata     = unpdata.data();
            outdatasize = unpdata.size();
            zip_close(zarc);
            zip_source_close(zsrc);
          }
        }

        analyser = createMemAnalyser(outdata, outdatasize);
      } else {
        fprintf(stderr, "Invalid output buffer!\n");
        return LogAnExitCodes::BufferFail;
      }
    } else if (auto fpath = std::filesystem::path(argLink); std::filesystem::exists(fpath)) {
      analyser = createFileAnalyser(argv[1]);
    }

    if (analyser != nullptr) {
      runAnalyser(analyser);
    } else {
      fprintf(stderr, "P7Dump fail: No suitable analyser found for specified link\n");
      return LogAnExitCodes::NoAnalyser;
    }
  }

  if (argc == 2 || std::string_view(argv[2]) != "--noblock") {
    while (true)
      std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  return LogAnExitCodes::Success;
}
