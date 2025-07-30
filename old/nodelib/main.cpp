#include "libp7d/p7d.h"
#include "libp7d/p7da.h"

#include <cstddef>
#include <cstdio>
#include <memory>
#include <napi.h>

Napi::Value MemAnalyze(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1) {
    Napi::TypeError::New(env, "Expected one argument").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Value arg = info[0];
  if (!arg.IsArrayBuffer()) {
    Napi::TypeError::New(env, "Argument must be an ArrayBuffer").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::ArrayBuffer arrayBuffer = arg.As<Napi::ArrayBuffer>();
  void*             data        = arrayBuffer.Data();
  size_t            length      = arrayBuffer.ByteLength();

  std::unique_ptr<P7Dump> analyzer = createMemAnalyser(data, length);

  try {
    if (analyzer->run()) {
      std::string  ret       = analyzer->spit();
      Napi::Object global    = env.Global();
      Napi::Value  jsonValue = global.Get("JSON");
      if (!jsonValue.IsObject()) {
        Napi::Error::New(env, "JSON not found").ThrowAsJavaScriptException();
        return env.Null();
      }
      Napi::Object json    = jsonValue.As<Napi::Object>();
      Napi::Value  parseFn = json.Get("parse");
      if (!parseFn.IsFunction()) {
        Napi::Error::New(env, "JSON.parse not found").ThrowAsJavaScriptException();
        return env.Null();
      }
      Napi::Function parse      = parseFn.As<Napi::Function>();
      Napi::String   jsonString = Napi::String::New(env, ret);
      Napi::Value    result     = parse.Call(json, {jsonString});
      return result;
    } else {
      Napi::Error::New(env, "Analyzer run failed").ThrowAsJavaScriptException();
      return env.Null();
    }
  } catch (const std::exception& ex) {
    Napi::Error::New(env, ex.what()).ThrowAsJavaScriptException();
    return env.Null();
  }
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set("meman", Napi::Function::New(env, MemAnalyze));
  return exports;
}

NODE_API_MODULE(psOff_logan, Init)
