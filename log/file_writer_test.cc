#include "file_writer.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "absl/random/random.h"
#include "absl/strings/cord.h"
#include "absl/time/time.h"

ABSL_DECLARE_FLAG(std::string, tests_test_util_temp_dir);

namespace witnesskvs::log {
namespace {

std::string GetTempFilename() {
  absl::Time now = absl::Now();
  std::string filename =
      absl::GetFlag(FLAGS_tests_test_util_temp_dir) + "/file_writer_test.";
  filename.append(absl::StrCat(absl::ToUnixMicros(now)));
  return filename;
}

absl::Cord getLargeCord(int rounds = 128) {
  absl::Cord cord;
  absl::BitGen gen;
  for (int i = 0; i < rounds; i++) {
    std::string tempstr;
    uint32_t len = absl::Uniform(absl::IntervalClosed, gen, 1, 4096);
    for (uint32_t j = 0; j < len; j++) {
      uint32_t c = absl::Uniform(absl::IntervalClosed, gen, 32, 126);
      tempstr.append(1, (char)c);
    }
    cord.Append(tempstr);
  }
  cord.Append("This should be the end...");
  return cord;
}

TEST(FileWriterTest, Smoke) {
  std::string filename = GetTempFilename();
  {
    FileWriter file_writer(filename);
    absl::Cord cord;
    cord.Append("Some message!");
    cord.Append("1234SecondMessage in cord.");
    file_writer.Write(cord);
    file_writer.Flush();
  }
  ASSERT_TRUE(std::filesystem::remove(std::filesystem::path(filename)));
}

TEST(FileWriterTest, Large) {
  std::string filename = GetTempFilename();
  {
    FileWriter file_writer(filename);
    absl::Cord cord = getLargeCord();
    file_writer.Write(cord);
    file_writer.Flush();
    std::string str;
    {
      std::ifstream fs(filename.c_str());
      ASSERT_TRUE(fs.good());
      str = std::string(std::istreambuf_iterator<char>{fs}, {});
    }
    EXPECT_EQ(cord, str);
    EXPECT_EQ(cord.size(), file_writer.bytes_written());
  }
  ASSERT_TRUE(std::filesystem::remove(std::filesystem::path(filename)));
}

TEST(FileWriterTest, LargMultiCord) {
  std::string filename = GetTempFilename();
  {
    FileWriter file_writer(filename);
    absl::Cord cord1 = getLargeCord();
    absl::Cord cord2 = getLargeCord(64);
    file_writer.Write(cord1);
    file_writer.Write(cord2);
    file_writer.Flush();
    std::string str;
    {
      std::ifstream fs(filename.c_str());
      ASSERT_TRUE(fs.good());
      str = std::string(std::istreambuf_iterator<char>{fs}, {});
    }
    absl::Cord combined_cord = cord1;
    combined_cord.Append(cord2);
    EXPECT_EQ(combined_cord, str);
    EXPECT_EQ(combined_cord.size(), file_writer.bytes_written());
  }
  ASSERT_TRUE(std::filesystem::remove(std::filesystem::path(filename)));
}

// TODO microbenchmark to watch flush cycles and timing.

}  // namespace
}  // namespace witnesskvs::log
