/*
 * Tiny UI string table. One entry per user-visible literal; `tr()` returns the
 * string for the active language. An optional SD "language pack" table is
 * consulted first (per-entry, nullptr falls through) — that is how additional
 * languages load without a firmware rebuild (see docs/ROADMAP.md).
 *
 * Keep the X-macro list the single source of truth: the enum and both built-in
 * tables are generated from it, so they can never drift.
 */
#pragma once

namespace core::i18n {

enum Lang { Ko = 0, En = 1 };

// X(NAME, korean, english)
#define I18N_MESSAGES(X)                                                        \
    X(BACK,               "← 뒤로",              "← Back")                       \
    X(SAVE,               "저장",                "Save")                         \
    X(CANCEL,             "취소",                "Cancel")                       \
    X(CLOSE,              "닫기",                "Close")                        \
    X(CONNECT,            "연결",                "Connect")                      \
    X(DELETE,             "삭제",                "Delete")                       \
    X(DEFAULT,            "기본",                "Default")                      \
    X(SHOW,               "표시",                "Show")                         \
    X(HIDE,               "숨김",                "Hide")                         \
    /* app names / titles */                                                    \
    X(APP_BOOKS,          "책",                  "Books")                        \
    X(APP_FILES,          "파일",                "Files")                        \
    X(APP_SETTINGS,       "설정",                "Settings")                     \
    X(APP_STORE,          "앱",                  "Apps")                        \
    X(APP_CLOCK,          "시계",                "Clock")                        \
    X(APP_SKETCH,         "그림",                "Draw")                         \
    X(APP_MEMO,           "메모",                "Memo")                         \
    /* status bar */                                                            \
    X(BRIGHTNESS_FMT,     "밝기 %d%%",           "Brightness %d%%")              \
    /* bookshelf */                                                             \
    X(BOOK_RECENT,        "읽던 책",             "Reading")                      \
    X(BOOK_CONTINUE,      "이어읽기",            "Continue")                     \
    X(BOOK_NO_SD,         "SD 카드를 넣어주세요.", "Insert an SD card.")           \
    X(BOOK_NONE,          "/sd 또는 /sd/Books 에 책을 넣어주세요.",                \
                          "Put books in /sd or /sd/Books.")                     \
    /* files */                                                                 \
    X(FILE_EMPTY,         "(빈 폴더)",           "(empty)")                      \
    X(FILE_UNREADABLE,    "(폴더를 열 수 없음)", "(cannot open folder)")         \
    X(FILE_PARENT,        "../  상위 폴더",      "../  parent folder")           \
    /* reader */                                                                \
    X(READER_OPEN_FAIL,   "책을 열 수 없습니다.", "Cannot open this book.")       \
    X(READER_IMG,         "[그림]",              "[image]")                      \
    X(READ_TITLE,         "읽기 설정",           "Reading")                      \
    X(READ_FONT_SIZE,     "글자 크기",           "Text size")                    \
    X(READ_MARGIN,        "여백",                "Margin")                       \
    X(READ_LINE_GAP,      "줄 간격",             "Line spacing")                 \
    /* settings list */                                                         \
    X(SET_DISPLAY,        "화면 / 글꼴",         "Display & Fonts")              \
    X(SET_WIFI_FMT,       "Wi-Fi: %s",           "Wi-Fi: %s")                    \
    X(SET_WIFI_UNSET,     "미설정",              "not set")                      \
    X(SET_WIFI_OFF,       "꺼짐",                "off")                          \
    X(LUA_LOADING,        "불러오는 중...",        "Loading...")                     \
    /* app catalog */                                                           \
    X(LUA_NEEDS_FIRMWARE, "이 앱은 Lua API %d가 필요합니다.\n펌웨어를 업데이트해 주세요. (현재 %d)", \
                          "This app needs Lua API %d.\nUpdate the firmware. (This one has %d.)") \
    X(STORE_REFRESH,      "새로고침",            "Refresh")                     \
    X(STORE_LOADING,      "앱 목록을 불러오는 중...", "Loading the app list...")    \
    X(STORE_LOAD_FAIL,    "앱 목록을 불러오지 못했습니다: %s",                 \
                          "Couldn't load the app list: %s")                     \
    X(STORE_NO_WIFI,      "Wi-Fi에 연결해 주세요", "Connect to Wi-Fi")          \
    X(STORE_NET_FAIL,     "인터넷에 연결할 수 없습니다", "Can't reach the internet") \
    X(STORE_NO_SD,        "SD 카드를 넣어 주세요", "Insert an SD card")         \
    X(STORE_INSTALLED,    "설치됨",              "Installed")                   \
    X(STORE_UPDATE,       "업데이트",            "Update")                      \
    X(STORE_NEEDS_FW,     "펌웨어 필요",         "Needs firmware")              \
    X(STORE_NOT_LISTED,   "목록에 없음",         "Not listed")                  \
    X(STORE_INSTALL,      "설치",                "Install")                     \
    X(STORE_DELETE_AGAIN, "한 번 더 눌러 삭제",  "Tap again to delete")         \
    X(STORE_DOWNLOADING,  "받는 중 %d/%d",       "Downloading %d/%d")           \
    X(STORE_INSTALL_FAIL, "설치하지 못했습니다: %s", "Install failed: %s")      \
    X(STORE_INSTALL_OK,   "설치했습니다",        "Installed")                   \
    X(STORE_DELETE_OK,    "삭제했습니다",        "Deleted")                     \
    X(STORE_DELETE_FAIL,  "삭제하지 못했습니다", "Couldn't delete")             \
    X(STORE_AUTHOR,       "만든 사람: %s",       "Author: %s")                  \
    X(STORE_SIZE,         "크기: %u KB",         "Size: %u KB")                 \
    X(STORE_NET,          "인터넷 접속:",        "Internet access:")            \
    X(STORE_NET_NONE,     "인터넷 접속 안 함",   "No internet access")          \
    X(STORE_NET_NEW,      "(새로 추가)",         "(new)")                       \
    /* firmware update */                                                       \
    X(SET_UPDATE,         "펌웨어 업데이트",     "Firmware update")              \
    X(UPD_CURRENT,        "현재 버전: %s",       "Installed: %s")                \
    X(UPD_LATEST,         "최신 버전: %s",       "Latest: %s")                   \
    X(UPD_UNKNOWN,        "확인 전",             "not checked")                  \
    X(UPD_CHECK,          "확인",                "Check")                        \
    X(UPD_INSTALL,        "업데이트",            "Update")                       \
    X(UPD_CANCEL,         "취소",                "Cancel")                       \
    X(UPD_CHECKING,       "확인하는 중...",        "Checking...")                    \
    X(UPD_UP_TO_DATE,     "최신 버전입니다",     "Up to date")                   \
    X(UPD_AVAILABLE,      "새 버전이 있습니다",  "A new version is available")   \
    X(UPD_DOWNLOADING,    "받는 중 %d%%",        "Downloading %d%%")             \
    X(UPD_RESTARTING,     "다시 시작합니다...",    "Restarting...")                  \
    X(UPD_CHECK_FAIL,     "확인하지 못했습니다: %s", "Check failed: %s")         \
    X(UPD_FAIL,           "업데이트하지 못했습니다: %s", "Update failed: %s")    \
    X(UPD_CANCELLED,      "업데이트를 취소했습니다", "Update cancelled")         \
    X(UPD_LOW_BATTERY,    "배터리를 30% 이상 충전해 주세요", "Charge the battery to 30% or more") \
    X(UPD_ROLLED_BACK,    "%s 업데이트가 시작되지 않아 되돌렸습니다", "%s failed to start and was rolled back") \
    X(UPD_NOTES,          "변경 내용:",          "Release notes:")               \
    X(UPD_KEEP_ON,        "업데이트 중에는 화면을 나가지 마세요", "Stay on this screen while updating") \
    X(SET_TIME,           "시간 설정",           "Date & Time")                  \
    X(SET_ABOUT,          "정보",                "About")                        \
    X(SET_SYSINFO,        "시스템 상태",         "System status")                \
    X(SET_LANGUAGE,       "언어 / Language",     "Language")                     \
    X(SET_POWER,          "전원",                "Power")                        \
    X(SET_RESTART_APPLY,  "재시작하여 적용",     "Restart to apply")             \
    X(SET_GESTURES_FMT,   "제스처: %s",         "Gestures: %s")                  \
    X(SET_ON,             "켜짐",               "on")                            \
    X(SET_OFF,            "꺼짐",               "off")                           \
    /* system status page */                                                    \
    X(SYS_BATTERY,        "배터리",              "Battery")                      \
    X(SYS_STORAGE,        "SD 카드",             "SD card")                      \
    X(SYS_MEMORY,         "메모리",              "Memory")                       \
    X(SYS_UPTIME,         "가동 시간",           "Uptime")                       \
    X(SYS_USED,           "사용",                "used")                         \
    X(SYS_FREE,           "여유",                "free")                         \
    X(SYS_NONE,           "없음",                "none")                         \
    X(SYS_UPTIME_FMT,     "%d시간 %d분",         "%dh %dm")                      \
    /* display page */                                                          \
    X(DISP_BRIGHTNESS,    "밝기",                "Brightness")                   \
    X(DISP_UI_FAMILY,     "시스템 글꼴",         "System font")                  \
    X(DISP_UI_SIZE,       "시스템 크기",         "System size")                  \
    X(DISP_READ_FAMILY,   "읽기 글꼴",           "Reader font")                  \
    X(DISP_READ_SIZE,     "읽기 크기",           "Reader size")                  \
    X(DISP_TOUCH_PAD,     "터치 여백",           "Touch padding")                \
    X(DISP_BATT_GAUGE,    "배터리 게이지",       "Battery gauge")                \
    X(DISP_BRIGHT_GAUGE,  "밝기 게이지",         "Brightness gauge")             \
    X(DISP_RESTORE,       "기본값 복원",         "Restore defaults")             \
    X(DISP_RESTART_NOTE,  "시스템 글꼴·언어는 재시작 후 적용됩니다.",              \
                          "System font & language apply after a restart.")      \
    X(FAMILY_DEFAULT,     "기본",                "Default")                      \
    X(FAMILY_GOTHIC,      "고딕",                "Gothic")                       \
    X(FAMILY_MYEONGJO,    "명조",                "Myeongjo")                     \
    X(FAMILY_PEN,         "손글씨",              "Pen")                          \
    X(FAMILY_SANS,        "고딕",                "Sans")                         \
    X(FAMILY_SERIF,       "명조",                "Serif")                        \
    /* power page */                                                            \
    X(PWR_TITLE,          "전원",                "Power")                        \
    X(PWR_RESTART,        "재시작",              "Restart")                      \
    X(PWR_SHUTDOWN,       "전원 끄기",           "Power off")                    \
    X(PWR_CONFIRM,        "한 번 더 눌러 확인",  "Tap again to confirm")         \
    X(PWR_NO_OFF,         "USB 연결 중에는 전원을 끌 수 없습니다.",                \
                          "Can't power off while on USB.")                      \
    /* time page */                                                             \
    X(TIME_YEAR,          "년",                  "Year")                         \
    X(TIME_MONTH,         "월",                  "Month")                        \
    X(TIME_DAY,           "일",                  "Day")                          \
    X(TIME_HOUR,          "시",                  "Hour")                         \
    X(TIME_MIN,           "분",                  "Min")                          \
    X(TIME_NTP,           "NTP 동기화",          "NTP sync")                     \
    X(TIME_WIFI_DOWN,     "Wi-Fi 연결 안 됨",    "Wi-Fi not connected")          \
    X(TIME_NTP_WAIT,      "NTP 동기화 중...",      "NTP sync...")                    \
    X(TIME_SYNC_OK_FMT,   "동기화 완료 — %04d-%02d-%02d %02d:%02d",              \
                          "Synced — %04d-%02d-%02d %02d:%02d")                  \
    X(TIME_SYNC_TIMEOUT,  "동기화 실패 (시간 초과)", "Sync failed (timeout)")      \
    X(TIME_CLOCK_UNSET,   "시계가 설정되지 않음 — 값을 맞추고 저장하세요",          \
                          "Clock not set — adjust the values and save")         \
    X(TIME_SAVED,         "저장됨",              "Saved")                        \
    X(TIME_SAVE_FAIL,     "저장 실패 (I2C)",     "Save failed (I2C)")            \
    /* wi-fi page */                                                            \
    X(WIFI_TITLE,         "Wi-Fi 설정",          "Wi-Fi")                        \
    X(WIFI_ENABLE,        "Wi-Fi 사용",          "Wi-Fi enabled")                \
    X(WIFI_DISABLED,      "Wi-Fi 꺼짐",          "Wi-Fi is off")                 \
    X(WIFI_CONNECTING,    "연결 중...",            "Connecting...")                  \
    X(WIFI_CONNECTED,     "연결됨",              "Connected")                    \
    X(WIFI_FAILED,        "연결 실패",           "Failed")                       \
    X(WIFI_OFF,           "연결 안 됨",          "Not connected")                \
    X(WIFI_RESCAN,        "다시 검색",           "Rescan")                       \
    X(WIFI_SAVED_DBM_FMT, "저장됨 · %ddBm",      "Saved · %ddBm")                \
    X(WIFI_SAVED,         "저장됨",              "Saved")                        \
    X(WIFI_PASSWORD,      "비밀번호",            "Password")                     \
    X(WIFI_PW_PROMPT_FMT, "'%s' 비밀번호",       "'%s' password")               \
    X(WIFI_SCANNING,      "검색 중...",            "Scanning...")                    \
    X(WIFI_SCANNING_FULL, "Wi-Fi 검색 중...",      "Scanning Wi-Fi...")             \
    X(WIFI_NO_NETWORKS,   "(검색된 네트워크 없음)", "(no networks found)")         \
    /* about page */                                                            \
    X(ABOUT_VERSION,      "버전",                "Version")                      \
    X(ABOUT_HW,           "하드웨어",            "Hardware")                     \
    X(ABOUT_FW,           "프레임워크",          "Frameworks")                   \
    X(ABOUT_FONT_HDR,     "글꼴",                "Fonts")                        \
    X(ABOUT_FONT_LINE,    "나눔 글꼴 · Noto Sans/Serif CJK",                     \
                          "Nanum · Noto Sans/Serif CJK")                        \
    X(ABOUT_FONT_LICENSE, "SIL Open Font License 1.1",                          \
                          "SIL Open Font License 1.1")                          \
    X(ABOUT_ICON_HDR,     "아이콘",              "Icons")                        \
    X(ABOUT_MINIZ,        "EPUB 압축 해제: ESP32-S3 ROM miniz",                  \
                          "EPUB inflate: ESP32-S3 ROM miniz")                   \
    X(ABOUT_OSS_NOTE,     "오픈소스 고지 전문: docs/THIRD_PARTY.md",             \
                          "Full OSS notices: docs/THIRD_PARTY.md")              \
    /* language page */                                                         \
    X(LANG_TITLE,         "언어 / Language",     "Language")                     \
    X(LANG_KOREAN,        "한국어",              "한국어")                       \
    X(LANG_ENGLISH,       "English",             "English")                     \
    X(LANG_NOTE,          "언어는 재시작 후 적용됩니다.",                          \
                          "Language applies after a restart.")

enum Msg {
#define X(name, ko, en) name,
    I18N_MESSAGES(X)
#undef X
    MSG_COUNT
};

void setLang(Lang l);
Lang lang();

// table must have MSG_COUNT entries; entries may be nullptr (fall through to the
// built-in string). Not copied — must outlive all tr() calls. nullptr clears.
void setPack(const char* const* table);

const char* tr(Msg m);

// "BACK" -> BACK etc.; MSG_COUNT if unknown. Used when loading an SD text pack.
Msg msgFromName(const char* name);
const char* msgName(Msg m);   // reverse, for dumping a pack template
const char* builtinEn(Msg m); // built-in English, ignoring any pack

}  // namespace core::i18n
