#pragma once
#include <fstream>
#include <stdint.h>
#include <vector>

namespace grapho {

inline std::vector<uint8_t>
ReadPath(const std::string& path)
{
  std::ifstream ifs(path, std::ios::binary | std::ios::ate);
  if (!ifs) {
    return {};
  }
  auto pos = ifs.tellg();
  auto size = pos;
  std::vector<uint8_t> buffer(size);
  ifs.seekg(0, std::ios::beg);
  ifs.read((char*)buffer.data(), pos);
  return buffer;
}

inline std::u8string
StringFromPath(const std::string& path)
{
  auto bytes = ReadPath(path);
  if (bytes.empty()) {
    return {};
  }
  return { (const char8_t*)bytes.data(),
           (const char8_t*)bytes.data() + bytes.size() };
}

inline std::string
join_path(const std::string& dir, const std::string& file)
{
  auto dst = dir;
  if (dir.ends_with("/") || file.starts_with("/")) {
    return dir + file;
  } else {
    return dir + "/" + file;
  }
}

}
