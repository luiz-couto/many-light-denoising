#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "file_handler.h"
#include "config.h"
#include <algorithm>
#include <filesystem>
#include <fstream>

// -----------------------------------------------------------------------
// writePFM
//
// PFM contract the harness depends on (compare.py reads these files):
//   header = "PF\n<width> <height>\n-1.0\n"  (negative scale = little-endian)
//   body   = raw 32-bit floats, 3 per pixel, BOTTOM row first
//   values = written verbatim — no tone-map, no SPP division (caller's job)
// The row-write casts Colour* to bytes, so Colour must be 3 packed floats.
// -----------------------------------------------------------------------

// Reads the file back in FILE order (bottom row first) without undoing the flip,
// so tests can assert the on-disk layout directly.
struct PFMFileData {
  std::string type;
  int width = 0;
  int height = 0;
  float scale = 0.0f;
  std::vector<float> floats;
};

static PFMFileData readPFMFile(const std::string& path) {
  PFMFileData pfm;
  std::ifstream file(path, std::ios::binary);
  REQUIRE(file.is_open());
  file >> pfm.type >> pfm.width >> pfm.height >> pfm.scale;
  file.get();  // exactly one whitespace separates the header from the binary body
  pfm.floats.resize((size_t)pfm.width * pfm.height * 3);
  file.read(reinterpret_cast<char*>(pfm.floats.data()),
            (std::streamsize)(pfm.floats.size() * sizeof(float)));
  REQUIRE(file.gcount() == (std::streamsize)(pfm.floats.size() * sizeof(float)));
  return pfm;
}

static std::string tempOutputPath(const std::string& name) {
  return (std::filesystem::temp_directory_path() / name).string();
}

TEST_CASE("FileHandler writePFM: Colour is three tightly packed floats (layout contract)") {
  // The row write reinterprets Colour* as raw bytes — padding or an extra
  // member would silently interleave garbage into every PFM.
  REQUIRE(sizeof(Colour) == 3 * sizeof(float));
}

TEST_CASE("FileHandler writePFM: header is PF, dimensions, -1.0") {
  std::vector<Colour> buffer(2 * 3, Colour(0.0f, 0.0f, 0.0f));
  std::string path = tempOutputPath("header_test.pfm");
  FileHandler::writePFM(path, buffer, 2, 3);

  PFMFileData pfm = readPFMFile(path);
  REQUIRE(pfm.type == "PF");
  REQUIRE(pfm.width == 2);
  REQUIRE(pfm.height == 3);
  REQUIRE(pfm.scale == Catch::Approx(-1.0f));
  REQUIRE(pfm.floats.size() == 2u * 3u * 3u);
  std::filesystem::remove(path);
}

TEST_CASE("FileHandler writePFM: bottom buffer row is written first") {
  // 2x3 buffer with per-pixel identifiable values: pixel (x,y) = (x, y, 10y+x).
  // On disk, the FIRST 6 floats must be buffer row y=2, the LAST 6 row y=0.
  std::vector<Colour> buffer(2 * 3);
  for (int y = 0; y < 3; y++) {
    for (int x = 0; x < 2; x++) {
      buffer[y * 2 + x] = Colour((float)x, (float)y, (float)(10 * y + x));
    }
  }
  std::string path = tempOutputPath("flip_test.pfm");
  FileHandler::writePFM(path, buffer, 2, 3);

  PFMFileData pfm = readPFMFile(path);
  // First file row = buffer row y=2: (0,2,20), (1,2,21)
  REQUIRE(pfm.floats[0] == 0.0f);
  REQUIRE(pfm.floats[1] == 2.0f);
  REQUIRE(pfm.floats[2] == 20.0f);
  REQUIRE(pfm.floats[3] == 1.0f);
  REQUIRE(pfm.floats[4] == 2.0f);
  REQUIRE(pfm.floats[5] == 21.0f);
  // Last file row = buffer row y=0: (0,0,0), (1,0,1)
  REQUIRE(pfm.floats[12] == 0.0f);
  REQUIRE(pfm.floats[13] == 0.0f);
  REQUIRE(pfm.floats[14] == 0.0f);
  REQUIRE(pfm.floats[15] == 1.0f);
  REQUIRE(pfm.floats[16] == 0.0f);
  REQUIRE(pfm.floats[17] == 1.0f);
  std::filesystem::remove(path);
}

TEST_CASE("FileHandler writePFM: HDR and awkward float values round-trip bit-exact") {
  // Raw dump, no quantisation: exact equality (==), not Approx. Covers the
  // values PNG would destroy — huge HDR, tiny, negative, non-representable-in-8-bit.
  std::vector<Colour> buffer = {
    Colour(0.1f, 1e30f, 1e-30f),
    Colour(-3.5f, 0.0f, 1e6f),
    Colour(1234.5678f, 0.001f, 7.0f),
    Colour(0.30000001f, 255.0f, 0.5f),
  };
  std::string path = tempOutputPath("roundtrip_test.pfm");
  FileHandler::writePFM(path, buffer, 2, 2);

  PFMFileData pfm = readPFMFile(path);
  // File row 0 = buffer row y=1 (bottom-first), file row 1 = buffer row y=0.
  const Colour* fileOrder[4] = { &buffer[2], &buffer[3], &buffer[0], &buffer[1] };
  for (int i = 0; i < 4; i++) {
    REQUIRE(pfm.floats[i * 3 + 0] == fileOrder[i]->r);
    REQUIRE(pfm.floats[i * 3 + 1] == fileOrder[i]->g);
    REQUIRE(pfm.floats[i * 3 + 2] == fileOrder[i]->b);
  }
  std::filesystem::remove(path);
}

TEST_CASE("FileHandler writePFM: buffer written verbatim — no SPP division") {
  // The averaging is the CALLER's job (filmDenoised must not be divided,
  // the raw film must be). writePFM itself never touches SPP.
  std::vector<Colour> buffer(4, Colour(1.0f, 1.0f, 1.0f));
  std::string path = tempOutputPath("verbatim_test.pfm");
  FileHandler::writePFM(path, buffer, 2, 2);

  PFMFileData pfm = readPFMFile(path);
  for (float v : pfm.floats) REQUIRE(v == 1.0f);
  std::filesystem::remove(path);
}

TEST_CASE("FileHandler writePFM: unwritable path fails gracefully without crashing") {
  std::vector<Colour> buffer(4, Colour(0.0f, 0.0f, 0.0f));
  REQUIRE_NOTHROW(FileHandler::writePFM("/nonexistent_dir_pfm_test/out.pfm", buffer, 2, 2));
  REQUIRE_FALSE(std::filesystem::exists("/nonexistent_dir_pfm_test/out.pfm"));
}

// -----------------------------------------------------------------------
// savePNG
// -----------------------------------------------------------------------

TEST_CASE("FileHandler savePNG: writes a file starting with the PNG signature") {
  // 2x2 RGB, arbitrary bytes. Content correctness is stb's job; ours is the
  // wiring: right path, right dimensions, actually a PNG.
  std::vector<uint8_t> pixels = { 255, 0, 0,  0, 255, 0,
                                  0, 0, 255,  255, 255, 255 };
  std::string path = tempOutputPath("smoke_test.png");
  FileHandler::savePNG(path, pixels, 2, 2);

  std::ifstream file(path, std::ios::binary);
  REQUIRE(file.is_open());
  uint8_t magic[8] = {};
  file.read(reinterpret_cast<char*>(magic), 8);
  const uint8_t expected[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };  // \x89PNG\r\n\x1a\n
  for (int i = 0; i < 8; i++) REQUIRE(magic[i] == expected[i]);
  std::filesystem::remove(path);
}

// -----------------------------------------------------------------------
// writeJSONMetadata
//
// Light C++ checks (structure sentinels + verbatim Config echo). The real
// schema validator is json.load in compare.py's self-test — the actual
// consumer parsing actual output.
// -----------------------------------------------------------------------

static std::string readWholeFile(const std::string& path) {
  std::ifstream file(path);
  REQUIRE(file.is_open());
  return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

TEST_CASE("FileHandler writeJSONMetadata: echoes run identity and film state verbatim") {
  Film film;
  film.init(4, 3);
  film.SPP = 7;

  // Inline config values are mutable — set, write, restore.
  std::string savedScene = Config::SCENE_NAME;
  std::string savedIntegrator = Config::INTEGRATOR_NAME;
  Config::SCENE_NAME = "testscene";
  Config::INTEGRATOR_NAME = "restir";

  std::string path = tempOutputPath("metadata_test.json");
  FileHandler::writeJSONMetadata(film, path, 1.5, "20260805_120000");
  std::string json = readWholeFile(path);

  REQUIRE(json.find("\"scene\": \"testscene\"") != std::string::npos);
  REQUIRE(json.find("\"integrator\": \"restir\"") != std::string::npos);
  REQUIRE(json.find("\"timestamp\": \"20260805_120000\"") != std::string::npos);
  REQUIRE(json.find("\"width\": 4") != std::string::npos);
  REQUIRE(json.find("\"height\": 3") != std::string::npos);
  REQUIRE(json.find("\"spp\": 7") != std::string::npos);
  REQUIRE(json.find("\"render_seconds\": 1.5") != std::string::npos);

  Config::SCENE_NAME = savedScene;
  Config::INTEGRATOR_NAME = savedIntegrator;
  std::filesystem::remove(path);
}

TEST_CASE("FileHandler writeJSONMetadata: booleans serialize as true/false, not 1/0") {
  Film film;
  film.init(2, 2);

  bool savedDenoiser = Config::USE_DENOISER;
  Config::USE_DENOISER = false;

  std::string path = tempOutputPath("metadata_bool_test.json");
  FileHandler::writeJSONMetadata(film, path, 0.0, "20260805_120000");
  std::string json = readWholeFile(path);

  REQUIRE(json.find("\"use_denoiser\": false") != std::string::npos);
  REQUIRE(json.find("\"use_denoiser\": 0") == std::string::npos);

  Config::USE_DENOISER = savedDenoiser;
  std::filesystem::remove(path);
}

TEST_CASE("FileHandler writeJSONMetadata: contains all three config blocks with balanced braces") {
  // A pt run must still record the ir/restir constants (uniform schema for
  // compare.py; full provenance for reference renders).
  Film film;
  film.init(2, 2);

  std::string path = tempOutputPath("metadata_blocks_test.json");
  FileHandler::writeJSONMetadata(film, path, 0.0, "20260805_120000");
  std::string json = readWholeFile(path);

  REQUIRE(json.find("\"pt\": {") != std::string::npos);
  REQUIRE(json.find("\"ir\": {") != std::string::npos);
  REQUIRE(json.find("\"restir\": {") != std::string::npos);
  REQUIRE(json.find("\"num_light_paths\": ") != std::string::npos);
  REQUIRE(json.find("\"spatial_rounds\": ") != std::string::npos);

  long openBraces  = (long)std::count(json.begin(), json.end(), '{');
  long closeBraces = (long)std::count(json.begin(), json.end(), '}');
  REQUIRE(openBraces == 4);   // root + pt + ir + restir
  REQUIRE(openBraces == closeBraces);
  std::filesystem::remove(path);
}
