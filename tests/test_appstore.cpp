#include "catalog.h"
#include "check.h"
#include "manifest.h"
#include "url.h"

using namespace appstore;

namespace {

Version ver(const char* s)
{
    Version v;
    EXPECT_TRUE(parseVersion(s, v));
    return v;
}

const std::string kSha(64, 'a');

CatalogEntry goodEntry()
{
    CatalogEntry e;
    e.m = parseManifest("name = 날씨\nversion = 1.2.0\napi = 1\nnet = api.example.com\n");
    e.m.id = "weather";
    e.files = {{"app.txt", 100, kSha}, {"main.lua", 2000, kSha}, {"icon.m5i", 300, kSha}};
    e.size = 2400;
    return e;
}

}  // namespace

TEST(manifest_reads_all_keys)
{
    const AppManifest m = parseManifest(
        "# comment\n"
        "name    = 날씨\r\n"
        "name_en = Weather\n"
        "summary = 오늘의 날씨\n"
        "summary_en = Today\n"
        "author  = JonPark0\n"
        "icon    = cloud\n"
        "entry   = app.lua\n"
        "order   = 20\n"
        "version = 1.10.3\n"
        "api     = 2\n"
        "net     = API.Open-Meteo.com, geocoding-api.open-meteo.com ,\n");
    EXPECT_STREQ(m.name, "날씨");
    EXPECT_STREQ(m.name_en, "Weather");
    EXPECT_STREQ(m.summary, "오늘의 날씨");
    EXPECT_STREQ(m.summary_en, "Today");
    EXPECT_STREQ(m.author, "JonPark0");
    EXPECT_STREQ(m.icon, "cloud");
    EXPECT_STREQ(m.entry, "app.lua");
    EXPECT_EQ(m.order, 20);
    EXPECT_TRUE(m.has_version);
    EXPECT_EQ(m.version.minor, 10);
    EXPECT_EQ(m.api, 2);
    EXPECT_EQ(m.net.size(), 2u);
    EXPECT_STREQ(m.net[0], "api.open-meteo.com");
    EXPECT_STREQ(m.net[1], "geocoding-api.open-meteo.com");
}

TEST(manifest_defaults)
{
    const AppManifest m = parseManifest("name = x\napi = zero\norder = -5\nversion = 1.0\n");
    EXPECT_STREQ(m.entry, "main.lua");
    EXPECT_EQ(m.api, 1);
    EXPECT_EQ(m.order, 100);
    EXPECT_TRUE(!m.has_version);
    EXPECT_STREQ(m.version_str, "1.0");
    EXPECT_TRUE(m.net.empty());
}

TEST(version_parse_and_compare)
{
    Version v;
    EXPECT_TRUE(!parseVersion("1.0", v));
    EXPECT_TRUE(!parseVersion("1.0.0.0", v));
    EXPECT_TRUE(!parseVersion("v1.0.0", v));
    EXPECT_TRUE(!parseVersion("1..0", v));
    EXPECT_TRUE(!parseVersion("1.0.0 ", v));
    EXPECT_TRUE(compareVersion(ver("1.10.0"), ver("1.9.0")) > 0);
    EXPECT_TRUE(compareVersion(ver("2.0.0"), ver("1.99.99")) > 0);
    EXPECT_TRUE(compareVersion(ver("1.0.1"), ver("1.0.1")) == 0);
    EXPECT_TRUE(compareVersion(ver("0.9.9"), ver("1.0.0")) < 0);
}

TEST(id_rules)
{
    EXPECT_TRUE(validId("weather"));
    EXPECT_TRUE(validId("my-app-2"));
    EXPECT_TRUE(!validId(""));
    EXPECT_TRUE(!validId("-app"));
    EXPECT_TRUE(!validId("Weather"));
    EXPECT_TRUE(!validId("a_b"));
    EXPECT_TRUE(!validId(".hidden"));
    EXPECT_TRUE(!validId(std::string(33, 'a')));
}

TEST(host_rules)
{
    EXPECT_TRUE(validHost("api.open-meteo.com"));
    EXPECT_TRUE(validHost("localhost"));
    EXPECT_TRUE(!validHost(""));
    EXPECT_TRUE(!validHost("API.example.com"));
    EXPECT_TRUE(!validHost("a..b"));
    EXPECT_TRUE(!validHost(".a"));
    EXPECT_TRUE(!validHost("-a.com"));
    EXPECT_TRUE(!validHost("a-.com"));
    EXPECT_TRUE(!validHost("*.example.com"));
    EXPECT_TRUE(!validHost("a.com:80"));
}

TEST(path_rules)
{
    EXPECT_TRUE(validRelPath("main.lua"));
    EXPECT_TRUE(validRelPath("lib/util.lua"));
    EXPECT_TRUE(validRelPath("a/b/c.txt"));
    EXPECT_TRUE(!validRelPath("a/b/c/d.txt"));
    EXPECT_TRUE(!validRelPath("../x.lua"));
    EXPECT_TRUE(!validRelPath("a/../x.lua"));
    EXPECT_TRUE(!validRelPath("/main.lua"));
    EXPECT_TRUE(!validRelPath("a//b"));
    EXPECT_TRUE(!validRelPath("a/"));
    EXPECT_TRUE(!validRelPath(".state.txt"));
    EXPECT_TRUE(!validRelPath("lib/.hidden"));
    EXPECT_TRUE(!validRelPath("a\\b.lua"));
    EXPECT_TRUE(!validRelPath("한글.lua"));
}

TEST(url_host)
{
    EXPECT_STREQ(urlHost("https://API.open-meteo.com:443/v1?x=1"), "api.open-meteo.com");
    EXPECT_STREQ(urlHost("http://a.com"), "a.com");
    EXPECT_STREQ(urlHost("http://a.com?q=/x"), "a.com");
    EXPECT_STREQ(urlHost("http://good.com@evil.com/"), "evil.com");
    EXPECT_STREQ(urlHost("ftp://a.com/"), "");
    EXPECT_STREQ(urlHost("a.com/x"), "");
}

TEST(host_allowed)
{
    const std::vector<std::string> net = {"api.open-meteo.com"};
    EXPECT_TRUE(hostAllowed(net, "https://api.open-meteo.com/v1/forecast"));
    EXPECT_TRUE(hostAllowed(net, "https://API.OPEN-METEO.COM/"));
    EXPECT_TRUE(!hostAllowed(net, "https://api.open-meteo.com.evil.com/"));
    EXPECT_TRUE(!hostAllowed(net, "https://api.open-meteo.com@evil.com/"));
    EXPECT_TRUE(!hostAllowed(net, "https://evil.com/?h=api.open-meteo.com"));
    EXPECT_TRUE(!hostAllowed({}, "https://api.open-meteo.com/"));
}

TEST(entry_validation)
{
    std::string why;
    EXPECT_TRUE(validateEntry(goodEntry(), why));

    CatalogEntry e = goodEntry();
    e.files.push_back({"../evil.lua", 1, kSha});
    EXPECT_TRUE(!validateEntry(e, why));

    e = goodEntry();
    e.files.push_back({".state.txt", 1, kSha});
    EXPECT_TRUE(!validateEntry(e, why));

    e = goodEntry();
    e.files[1].sha256 = "xyz";
    EXPECT_TRUE(!validateEntry(e, why));

    e = goodEntry();
    e.files[1].size = kMaxFileBytes + 1;
    EXPECT_TRUE(!validateEntry(e, why));

    e = goodEntry();
    e.files.erase(e.files.begin());   // no app.txt
    EXPECT_TRUE(!validateEntry(e, why));

    e = goodEntry();
    e.files.erase(e.files.begin() + 1);   // no main.lua
    EXPECT_TRUE(!validateEntry(e, why));

    e = goodEntry();
    e.files.push_back({"main.lua", 1, kSha});
    EXPECT_TRUE(!validateEntry(e, why));

    e = goodEntry();
    e.m.id = "Bad";
    EXPECT_TRUE(!validateEntry(e, why));

    e = goodEntry();
    e.m.net.push_back("*.evil.com");
    EXPECT_TRUE(!validateEntry(e, why));
}

TEST(install_status)
{
    const CatalogEntry e = goodEntry();   // 1.2.0, api 1
    EXPECT_TRUE(status(e, nullptr, 1) == Status::NotInstalled);

    AppManifest old = parseManifest("version = 1.1.9\n");
    EXPECT_TRUE(status(e, &old, 1) == Status::UpdateAvailable);

    AppManifest same = parseManifest("version = 1.2.0\n");
    EXPECT_TRUE(status(e, &same, 1) == Status::Installed);

    AppManifest newer = parseManifest("version = 2.0.0\n");
    EXPECT_TRUE(status(e, &newer, 1) == Status::Installed);

    AppManifest sideloaded = parseManifest("name = x\n");
    EXPECT_TRUE(status(e, &sideloaded, 1) == Status::UpdateAvailable);

    CatalogEntry future = goodEntry();
    future.m.api = 2;
    EXPECT_TRUE(status(future, nullptr, 1) == Status::NeedsFirmware);
    EXPECT_TRUE(status(future, &old, 1) == Status::NeedsFirmware);
    EXPECT_TRUE(status(future, &same, 1) == Status::Installed);
}

TEST(new_hosts_on_update)
{
    CatalogEntry e = goodEntry();
    e.m.net = {"a.com", "b.com"};
    EXPECT_EQ(newHosts(e, nullptr).size(), 2u);

    AppManifest old = parseManifest("net = a.com\n");
    const auto added = newHosts(e, &old);
    EXPECT_EQ(added.size(), 1u);
    EXPECT_STREQ(added[0], "b.com");

    AppManifest both = parseManifest("net = b.com, a.com\n");
    EXPECT_TRUE(newHosts(e, &both).empty());
}
