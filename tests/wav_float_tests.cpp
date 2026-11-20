// S4 (DATA priority): invalid float WAV samples must be refused at import. NaN, +/-Inf and float64
// values that overflow the float cast used to enter the engine raw and were fully registered in the
// AssetStore (red measured at a9a0384). The refusal must name the file, the offending frame and the
// kind, and leave nothing registered; valid files must import bit-identical. Fixtures are generated
// here, and the bit checks compare the read-back samples against the same patterns that were written,
// so no golden is tied to one compiler. The suite runs in Debug AND Release: engine/CMakeLists.txt
// applies /fp:fast to every MSVC config, and the validation under test is integer bit-pattern based
// on purpose (isfinite guards could be optimized away).
// Run: wav_float_tests [part-of-name]
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>
#include "support/check.hpp"
#include "core/assets.hpp"
#include "io/audio_file.hpp"

using namespace core;
namespace fs = std::filesystem;

namespace {

fs::path fixtureDir() {
  fs::path d = fs::temp_directory_path() / "core-wav-float-tests";
  fs::create_directories(d);
  return d;
}

void put16(std::vector<unsigned char>& b, uint16_t v) { b.push_back((unsigned char)(v & 0xFF)); b.push_back((unsigned char)(v >> 8)); }
void put32(std::vector<unsigned char>& b, uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back((unsigned char)((v >> (8 * i)) & 0xFF)); }
void put64(std::vector<unsigned char>& b, uint64_t v) { for (int i = 0; i < 8; ++i) b.push_back((unsigned char)((v >> (8 * i)) & 0xFF)); }
void putTag(std::vector<unsigned char>& b, const char* tag) { for (int i = 0; i < 4; ++i) b.push_back((unsigned char)tag[i]); }
void patch16(std::vector<unsigned char>& b, size_t at, uint16_t v) { b[at] = (unsigned char)(v & 0xFF); b[at + 1] = (unsigned char)(v >> 8); }
void patch32(std::vector<unsigned char>& b, size_t at, uint32_t v) { for (int i = 0; i < 4; ++i) b[at + i] = (unsigned char)((v >> (8 * i)) & 0xFF); }

uint32_t f32bits(float v) { uint32_t b; std::memcpy(&b, &v, 4); return b; }
uint64_t f64bits(double v) { uint64_t b; std::memcpy(&b, &v, 8); return b; }

// Canonical WAV: RIFF + fmt(16) + data. format 3 = IEEE float, 1 = PCM. Little-endian fields.
std::vector<unsigned char> wavBytes(int format, int bits, int channels, uint32_t rate, const std::vector<unsigned char>& data) {
  std::vector<unsigned char> b;
  putTag(b, "RIFF"); put32(b, 0); putTag(b, "WAVE");
  putTag(b, "fmt "); put32(b, 16);
  put16(b, (uint16_t)format); put16(b, (uint16_t)channels); put32(b, rate);
  put32(b, rate * (uint32_t)(bits / 8) * (uint32_t)channels);
  put16(b, (uint16_t)((bits / 8) * channels)); put16(b, (uint16_t)bits);
  putTag(b, "data"); put32(b, (uint32_t)data.size());
  b.insert(b.end(), data.begin(), data.end());
  patch32(b, 4, (uint32_t)(b.size() - 8));  // RIFF size
  return b;
}

std::string writeFixture(const std::string& name, const std::vector<unsigned char>& bytes) {
  const fs::path path = fixtureDir() / name;
  std::ofstream out(path, std::ios::binary);
  out.write((const char*)bytes.data(), (std::streamsize)bytes.size());
  out.close();
  return path.string();
}

// Exact-representable values: the float64 control casts to float without rounding, and every float32
// pattern survives the raw memcpy unchanged, in any IEEE compiler. No subnormals (kept out of the
// /fp:fast discussion entirely).
const float kBase[] = {0.0f, 0.5f, -0.25f, 0.125f, 1.0f, -1.0f, 0.75f, -0.5f};
constexpr int kFrames = 64;
constexpr uint32_t kRate = 48000;

std::vector<uint32_t> baseF32() {
  std::vector<uint32_t> p(kFrames);
  for (int i = 0; i < kFrames; ++i) p[i] = f32bits(kBase[i % 8]);
  return p;
}
std::vector<uint64_t> baseF64() {
  std::vector<uint64_t> p(kFrames);
  for (int i = 0; i < kFrames; ++i) p[i] = f64bits((double)kBase[i % 8]);
  return p;
}
std::vector<unsigned char> bytesF32(const std::vector<uint32_t>& p) {
  std::vector<unsigned char> b;
  for (uint32_t v : p) put32(b, v);
  return b;
}
std::vector<unsigned char> bytesF64(const std::vector<uint64_t>& p) {
  std::vector<unsigned char> b;
  for (uint64_t v : p) put64(b, v);
  return b;
}

// readWav must throw std::runtime_error and the message must carry every expected part (file name,
// frame index, kind). A missing throw is a failure, never a silent pass.
void checkRefused(const char* file, int line, const std::string& path, std::initializer_list<const char*> parts) {
  ++coretest::checks();
  try {
    (void)readWav(path, 1 << 20);
  } catch (const std::runtime_error& e) {
    const std::string msg = e.what();
    for (const char* part : parts)
      if (msg.find(part) == std::string::npos)
        coretest::fail(file, line, std::string("refusal message lacks '") + part + "': " + msg);
    return;
  }
  coretest::fail(file, line, "expected readWav to refuse " + path + " (no exception thrown)");
}
#define CHECK_REFUSED(path, ...) checkRefused(__FILE__, __LINE__, (path), {__VA_ARGS__})

void checkMonoBits(const AudioData& d, const std::vector<uint32_t>& expected) {
  CHECK_EQ(d.frames(), (int64_t)expected.size());
  CHECK_EQ(d.channels, 1);
  CHECK_EQ(d.sampleRate, (double)kRate);
  CHECK_EQ(d.R.size(), d.L.size());  // mono: R == L
  for (size_t i = 0; i < expected.size(); ++i) {
    CHECK_EQ(f32bits(d.L[i]), expected[i]);
    CHECK_EQ(f32bits(d.R[i]), expected[i]);
  }
}

}  // namespace

TEST("wav float S4: valid float32 imports bit-identical") {
  const auto patterns = baseF32();
  const std::string path = writeFixture("f32_sine_valid.wav", wavBytes(3, 32, 1, kRate, bytesF32(patterns)));
  const AudioData d = readWav(path, 1 << 20);
  checkMonoBits(d, patterns);
}

TEST("wav float S4: valid float64 imports bit-identical") {
  const auto patterns = baseF64();
  const std::string path = writeFixture("f64_valid.wav", wavBytes(3, 64, 1, kRate, bytesF64(patterns)));
  const AudioData d = readWav(path, 1 << 20);
  // The written doubles are exactly representable as float: the cast is exact by IEEE, so the
  // expected float bits are the same values' float patterns, not a compiler-tied golden.
  std::vector<uint32_t> expected(kFrames);
  for (int i = 0; i < kFrames; ++i) expected[i] = f32bits(kBase[i % 8]);
  checkMonoBits(d, expected);
}

TEST("wav float S4: float32 NaN refused, file and frame named") {
  auto p = baseF32();
  p[5] = 0x7FC00000u;  // quiet NaN
  const std::string path = writeFixture("f32_nan.wav", wavBytes(3, 32, 1, kRate, bytesF32(p)));
  CHECK_REFUSED(path, "f32_nan.wav", "frame 5", "NaN");
}

TEST("wav float S4: float32 +Inf refused, file and frame named") {
  auto p = baseF32();
  p[5] = 0x7F800000u;  // +Inf
  const std::string path = writeFixture("f32_posinf.wav", wavBytes(3, 32, 1, kRate, bytesF32(p)));
  CHECK_REFUSED(path, "f32_posinf.wav", "frame 5", "Infinity");
}

TEST("wav float S4: float32 -Inf refused, file and frame named") {
  auto p = baseF32();
  p[5] = 0xFF800000u;  // -Inf
  const std::string path = writeFixture("f32_neginf.wav", wavBytes(3, 32, 1, kRate, bytesF32(p)));
  CHECK_REFUSED(path, "f32_neginf.wav", "frame 5", "Infinity");
}

TEST("wav float S4: float32 signaling NaN refused") {
  auto p = baseF32();
  p[5] = 0x7F800001u;  // signaling NaN: still a NaN bit pattern, refused before any float math
  const std::string path = writeFixture("f32_snan.wav", wavBytes(3, 32, 1, kRate, bytesF32(p)));
  CHECK_REFUSED(path, "f32_snan.wav", "frame 5", "NaN");
}

TEST("wav float S4: float32 mixed non-finite refused at the first offender") {
  auto p = baseF32();
  p[3] = 0x7FC00000u;   // NaN
  p[7] = 0x7F800000u;   // +Inf
  p[11] = 0xFF800000u;  // -Inf
  const std::string path = writeFixture("f32_mixed.wav", wavBytes(3, 32, 1, kRate, bytesF32(p)));
  CHECK_REFUSED(path, "f32_mixed.wav", "frame 3", "NaN");
}

TEST("wav float S4: float64 NaN refused, file and frame named") {
  auto p = baseF64();
  p[5] = 0x7FF8000000000000ull;  // quiet NaN
  const std::string path = writeFixture("f64_nan.wav", wavBytes(3, 64, 1, kRate, bytesF64(p)));
  CHECK_REFUSED(path, "f64_nan.wav", "frame 5", "NaN");
}

TEST("wav float S4: float64 +Inf refused, file and frame named") {
  auto p = baseF64();
  p[5] = 0x7FF0000000000000ull;  // +Inf
  const std::string path = writeFixture("f64_posinf.wav", wavBytes(3, 64, 1, kRate, bytesF64(p)));
  CHECK_REFUSED(path, "f64_posinf.wav", "frame 5", "Infinity");
}

TEST("wav float S4: float64 finite overflow refused before the float cast") {
  auto p = baseF64();
  p[5] = 0x7E37E43C8800759Cull;  // +1e300: finite double, (float) cast would give +Inf
  p[9] = 0xFE37E43C8800759Cull;  // -1e300
  const std::string path = writeFixture("f64_overflow.wav", wavBytes(3, 64, 1, kRate, bytesF64(p)));
  CHECK_REFUSED(path, "f64_overflow.wav", "frame 5", "overflow");
}

TEST("wav float S4: float64 exactly FLT_MAX imports, one double ulp above is refused") {
  auto ok = baseF64();
  ok[5] = 0x47EFFFFFE0000000ull;  // FLT_MAX as a double: the cast stays finite
  const std::string goodPath = writeFixture("f64_maxfloat.wav", wavBytes(3, 64, 1, kRate, bytesF64(ok)));
  const AudioData d = readWav(goodPath, 1 << 20);
  CHECK_EQ(d.frames(), (int64_t)kFrames);
  CHECK_EQ(f32bits(d.L[5]), 0x7F7FFFFFu);  // FLT_MAX float bits
  for (int i = 0; i < kFrames; ++i)
    if (i != 5) CHECK_EQ(f32bits(d.L[i]), f32bits(kBase[i % 8]));

  auto bad = ok;
  bad[5] = 0x47EFFFFFE0000001ull;  // next double above FLT_MAX: outside float range by the import rule
  const std::string badPath = writeFixture("f64_overbit.wav", wavBytes(3, 64, 1, kRate, bytesF64(bad)));
  CHECK_REFUSED(badPath, "f64_overbit.wav", "frame 5", "overflow");
}

TEST("wav float S4: stereo NaN in the right channel names the channel") {
  const auto mono = baseF32();
  std::vector<unsigned char> data;
  for (int i = 0; i < kFrames; ++i) {
    put32(data, mono[i]);                      // left: finite
    put32(data, i == 2 ? 0x7FC00000u : mono[i]);  // right: NaN at frame 2
  }
  const std::string path = writeFixture("f32_stereo_nan_right.wav", wavBytes(3, 32, 2, kRate, data));
  CHECK_REFUSED(path, "f32_stereo_nan_right.wav", "frame 2", "right", "NaN");
}

TEST("wav float S4: header rejection still names the file (8-bit float)") {
  auto bytes = wavBytes(3, 32, 1, kRate, bytesF32(baseF32()));
  patch16(bytes, 34, 8);  // fmt bits field: float 8-bit is not a supported format
  const std::string path = writeFixture("f32_badbits.wav", bytes);
  CHECK_REFUSED(path, "f32_badbits.wav", "unsupported sample format");
}

TEST("wav float S4: PCM16 import unchanged") {
  const int16_t samples[] = {0, 16384, -16384, -32768, 32767};
  std::vector<unsigned char> data;
  for (int16_t s : samples) put16(data, (uint16_t)s);
  const std::string path = writeFixture("pcm16_control.wav", wavBytes(1, 16, 1, kRate, data));
  const AudioData d = readWav(path, 1 << 20);
  CHECK_EQ(d.frames(), (int64_t)5);
  CHECK_EQ(d.L[0], 0.0f);
  CHECK_EQ(d.L[1], 0.5f);
  CHECK_EQ(d.L[2], -0.5f);
  CHECK_EQ(d.L[3], -1.0f);
  CHECK_EQ(d.L[4], 32767 / 32768.0f);
}

TEST("wav float S4: AssetStore refuses invalid float WAV and registers nothing") {
  auto nan = baseF32();
  nan[5] = 0x7FC00000u;
  const std::string nanPath = writeFixture("store_nan.wav", wavBytes(3, 32, 1, kRate, bytesF32(nan)));
  auto over = baseF64();
  over[5] = 0x7E37E43C8800759Cull;  // +1e300
  const std::string overPath = writeFixture("store_overflow.wav", wavBytes(3, 64, 1, kRate, bytesF64(over)));

  AssetStore store;
  std::string err = "<unset>";
  CHECK(store.get(nanPath, kRate, &err) == nullptr);
  CHECK(err.find("store_nan.wav") != std::string::npos);
  CHECK_EQ(store.bytes(), (size_t)0);
  err = "<unset>";
  CHECK(store.get(nanPath, kRate, &err) == nullptr);  // second attempt: still refused, still nothing cached
  CHECK(err.find("store_nan.wav") != std::string::npos);
  CHECK_EQ(store.bytes(), (size_t)0);
  err = "<unset>";
  CHECK(store.get(overPath, kRate, &err) == nullptr);
  CHECK(err.find("store_overflow.wav") != std::string::npos);
  CHECK_EQ(store.bytes(), (size_t)0);
}

TEST("wav float S4: AssetStore still caches a valid float WAV after a refusal") {
  auto nan = baseF32();
  nan[5] = 0x7FC00000u;
  const std::string badPath = writeFixture("store_bad_then_good.wav", wavBytes(3, 32, 1, kRate, bytesF32(nan)));
  const std::string goodPath = writeFixture("store_good.wav", wavBytes(3, 32, 1, kRate, bytesF32(baseF32())));

  AssetStore store;
  std::string err;
  CHECK(store.get(badPath, kRate, &err) == nullptr);
  CHECK_EQ(store.bytes(), (size_t)0);
  const auto good = store.get(goodPath, kRate, &err);
  CHECK(good != nullptr);
  CHECK_EQ(store.bytes(), (size_t)(kFrames * 2 * sizeof(float)));  // mono: L and R, 64 frames each
  const auto again = store.get(goodPath, kRate, &err);
  CHECK(again == good);  // cached: the same buffer, not a second load
}

CORE_TEST_MAIN
