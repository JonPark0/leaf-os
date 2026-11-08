#include "check.h"
#include "firmware_feed.h"

using namespace ota;

namespace {

constexpr size_t kSlot = 0x280000;

Feed goodFeed()
{
    Feed f;
    f.version = "v0.3.0";
    f.size = 2346560;
    f.sha256 = std::string(64, 'a');
    f.file = "leaf-os-v0.3.0.bin";
    f.notes = "https://github.com/JonPark0/leaf-os/releases/tag/v0.3.0";
    return f;
}

}  // namespace

TEST(version_parses_tag_and_dev_builds)
{
    FirmwareVersion v;
    EXPECT_TRUE(parseFirmwareVersion("v0.2.0", v));
    EXPECT_EQ(v.major, 0);
    EXPECT_EQ(v.minor, 2);
    EXPECT_EQ(v.patch, 0);
    EXPECT_TRUE(!v.dev);

    EXPECT_TRUE(parseFirmwareVersion("1.10.3", v));
    EXPECT_EQ(v.minor, 10);

    EXPECT_TRUE(parseFirmwareVersion("v0.2.0-3-gabc1234-dirty", v));
    EXPECT_TRUE(v.dev);
    EXPECT_EQ(v.patch, 0);
}

TEST(version_rejects_garbage)
{
    FirmwareVersion v;
    EXPECT_TRUE(!parseFirmwareVersion("", v));
    EXPECT_TRUE(!parseFirmwareVersion("v1.2", v));
    EXPECT_TRUE(!parseFirmwareVersion("v1.2.", v));
    EXPECT_TRUE(!parseFirmwareVersion("v1..2", v));
    EXPECT_TRUE(!parseFirmwareVersion("v1.2.3-", v));
    EXPECT_TRUE(!parseFirmwareVersion("v1.2.3x", v));
    EXPECT_TRUE(!parseFirmwareVersion("v123456.0.0", v));
    EXPECT_TRUE(!parseFirmwareVersion("919d3a5", v));
}

TEST(update_offered_only_when_newer)
{
    EXPECT_TRUE(updateAvailable("v0.2.0", "v0.3.0"));
    EXPECT_TRUE(updateAvailable("v0.2.9", "v0.10.0"));
    EXPECT_TRUE(updateAvailable("v0.9.9", "v1.0.0"));
    EXPECT_TRUE(!updateAvailable("v0.3.0", "v0.3.0"));
    EXPECT_TRUE(!updateAvailable("v0.4.0", "v0.3.0"));
    EXPECT_TRUE(!updateAvailable("v0.3.1", "v0.3.0"));
}

TEST(update_dev_builds_and_unreadable_versions)
{
    EXPECT_TRUE(updateAvailable("v0.3.0-2-gabc1234", "v0.3.0"));
    EXPECT_TRUE(updateAvailable("v0.3.0-dirty", "v0.3.0"));
    EXPECT_TRUE(!updateAvailable("v0.3.1-2-gabc1234", "v0.3.0"));
    EXPECT_TRUE(updateAvailable("919d3a5-dirty", "v0.3.0"));
    EXPECT_TRUE(!updateAvailable("v0.2.0", "v0.3.0-rc1"));
    EXPECT_TRUE(!updateAvailable("v0.2.0", "latest"));
}

TEST(feed_accepts_a_good_entry)
{
    std::string why;
    EXPECT_TRUE(validateFeed(goodFeed(), kSlot, why));
}

TEST(feed_rejects_bad_fields)
{
    std::string why;
    Feed f = goodFeed(); f.version = "v0.3.0-rc1";
    EXPECT_TRUE(!validateFeed(f, kSlot, why));
    f = goodFeed(); f.size = kSlot + 1;
    EXPECT_TRUE(!validateFeed(f, kSlot, why));
    EXPECT_STREQ(why, "too large");
    f = goodFeed(); f.size = 100;
    EXPECT_TRUE(!validateFeed(f, kSlot, why));
    f = goodFeed(); f.sha256 = std::string(63, 'a');
    EXPECT_TRUE(!validateFeed(f, kSlot, why));
    f = goodFeed(); f.sha256 = std::string(64, 'A');
    EXPECT_TRUE(!validateFeed(f, kSlot, why));
    f = goodFeed(); f.file = "../leaf.bin";
    EXPECT_TRUE(!validateFeed(f, kSlot, why));
    f = goodFeed(); f.file = "a/b.bin";
    EXPECT_TRUE(!validateFeed(f, kSlot, why));
    f = goodFeed(); f.file = "";
    EXPECT_TRUE(!validateFeed(f, kSlot, why));
}
