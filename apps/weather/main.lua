-- 날씨 / Weather -- sample SD app for the PaperMono-Lite Lua kit.
--
-- A small dashboard: condition icon, big current temperature, then the
-- day's high / low. Tap the bottom row to cycle preset cities; tap
-- "새로고침" to refetch. For another city, edit  .state.txt  on a PC:
--     location = <city>
-- The last reading is cached in .state.txt so it still shows offline.

local PRESETS = {"Seoul", "Tokyo", "Busan", "New York", "London", "Paris", "Berlin"}

local KO = sys.lang() == "ko"
local function tr(ko, en) return KO and ko or en end

-- Preset display names (the query still uses the English key above).
local LOC_KO = {
  Seoul = "서울", Tokyo = "도쿄", Busan = "부산", ["New York"] = "뉴욕",
  London = "런던", Paris = "파리", Berlin = "베를린",
}
local function loc_name(q) return (KO and LOC_KO[q]) or q end

-- ---- WMO weather code -> (icon name, short text) -------------------------
local function code_icon(c, day)
  c = tonumber(c) or -1
  if c == 0 then return day and "sun" or "moon" end
  if c <= 2 then return day and "cloud-sun" or "cloud-moon" end
  if c == 3 then return "cloud" end
  if c == 45 or c == 48 then return "cloud-fog" end
  if c >= 71 and c <= 77 then return "snow" end
  if c >= 85 and c <= 86 then return "snow" end
  if c >= 95 then return "bolt" end
  if c >= 51 and c <= 82 then return "cloud-rain" end
  return "cloud"
end

local function code_text(c)
  c = tonumber(c) or -1
  if c == 0 then return tr("맑음", "Clear") end
  if c <= 2 then return tr("대체로 맑음", "Mostly clear") end
  if c == 3 then return tr("흐림", "Overcast") end
  if c == 45 or c == 48 then return tr("안개", "Fog") end
  if c >= 51 and c <= 57 then return tr("이슬비", "Drizzle") end
  if c >= 61 and c <= 67 then return tr("비", "Rain") end
  if c >= 71 and c <= 77 then return tr("눈", "Snow") end
  if c >= 80 and c <= 82 then return tr("소나기", "Rain showers") end
  if c >= 85 and c <= 86 then return tr("눈보라", "Snow showers") end
  if c >= 95 then return tr("뇌우", "Thunderstorm") end
  return "--"
end

-- ---- view state --------------------------------------------------------
local W = {
  loc  = PRESETS[1],
  msg  = tr("Wi-Fi 연결 후 새로고침", "Connect Wi-Fi, then refresh"),
  temp = nil, hi = nil, lo = nil, cond = nil, icon = "cloud", when = nil,
}

local function clear_reading()
  W.temp, W.hi, W.lo, W.cond, W.when = nil, nil, nil, nil, nil
  W.icon = "cloud"
end

-- ---- big 7-segment number ("-12°") -----------------------------------
local SEG = {
  ["0"] = "abcdef", ["1"] = "bc",    ["2"] = "abged", ["3"] = "abgcd",
  ["4"] = "fgbc",   ["5"] = "afgcd", ["6"] = "afgedc", ["7"] = "abc",
  ["8"] = "abcdefg",["9"] = "abcfgd", ["-"] = "g",
}

local function draw_digit(x, y, ch, dw, dh, th)
  local segs = SEG[ch]
  if not segs then return end
  local half = (dh - 3 * th) / 2
  local function on(s) return segs:find(s, 1, true) ~= nil end
  if on("a") then g.rect(x + th,        y,                 dw - 2 * th, th, 3) end
  if on("f") then g.rect(x,             y + th,            th,          half, 3) end
  if on("b") then g.rect(x + dw - th,   y + th,            th,          half, 3) end
  if on("g") then g.rect(x + th,        y + th + half,     dw - 2 * th, th, 3) end
  if on("e") then g.rect(x,             y + 2 * th + half, th,          half, 3) end
  if on("c") then g.rect(x + dw - th,   y + 2 * th + half, th,          half, 3) end
  if on("d") then g.rect(x + th,        y + dh - th,       dw - 2 * th, th, 3) end
end

-- width of a rendered number string, given digit metrics
local function num_width(s, dw, gap, th)
  local w = 0
  for i = 1, #s do
    local ch = s:sub(i, i)
    if ch == "-" then w = w + dw * 0.6 + gap
    else w = w + dw + gap end
  end
  return w - gap + th * 3      -- + degree ring
end

local function draw_number(cx, y, s, dw, dh, th)
  local gap = math.floor(dw * 0.28)
  local x = math.floor(cx - num_width(s, dw, gap, th) / 2)
  for i = 1, #s do
    local ch = s:sub(i, i)
    if ch == "-" then
      g.rect(x, y + math.floor((dh - th) / 2), math.floor(dw * 0.6), th, 3)
      x = x + math.floor(dw * 0.6) + gap
    else
      draw_digit(x, y, ch, dw, dh, th)
      x = x + dw + gap
    end
  end
  -- degree ring, top-right of the last digit
  local r = th * 2
  g.rect(x, y, r, th, 3)
  g.rect(x, y, th, r, 3)
  g.rect(x + r - th, y, th, r, 3)
  g.rect(x, y + r - th, r, th, 3)
end

-- ---- data ------------------------------------------------------------
local function fetch(loc)
  W.loc = loc
  if not sys.wifi() then
    W.msg = tr("Wi-Fi 연결 필요", "Wi-Fi not connected")
    return
  end
  W.msg = tr("불러오는 중...", "Loading...")

  local lat = store.get("lat_" .. loc)
  local lon = store.get("lon_" .. loc)
  if not lat or not lon then
    local body, st = http.get(
      "https://geocoding-api.open-meteo.com/v1/search?count=1&name=" .. loc)
    if not body or st ~= 200 then W.msg = tr("검색 실패", "Lookup failed"); return end
    local gj = json.decode(body)
    local r = gj and gj.results and gj.results[1]
    if not r then W.msg = tr("도시를 찾을 수 없음", "City not found"); return end
    lat, lon = tostring(r.latitude), tostring(r.longitude)
    store.set("lat_" .. loc, lat)
    store.set("lon_" .. loc, lon)
  end

  local url = "https://api.open-meteo.com/v1/forecast?timezone=auto&forecast_days=1"
      .. "&latitude=" .. lat .. "&longitude=" .. lon
      .. "&current=temperature_2m,weather_code,is_day"
      .. "&daily=temperature_2m_max,temperature_2m_min"
  local body, st = http.get(url)
  if not body or st ~= 200 then W.msg = tr("날씨 조회 실패", "Fetch failed"); return end
  local d = json.decode(body)
  if not d or not d.current then W.msg = tr("응답 오류", "Bad response"); return end

  local day = tonumber(d.current.is_day) ~= 0
  W.temp = d.current.temperature_2m
  W.cond = code_text(d.current.weather_code)
  W.icon = code_icon(d.current.weather_code, day)
  W.hi = d.daily and d.daily.temperature_2m_max and d.daily.temperature_2m_max[1]
  W.lo = d.daily and d.daily.temperature_2m_min and d.daily.temperature_2m_min[1]
  local c = sys.clock()
  W.when = string.format("%02d:%02d", c.h, c.mi)
  W.msg = nil

  store.set("last_loc",  loc)
  store.set("last_temp", tostring(W.temp))
  store.set("last_cond", W.cond)
  store.set("last_icon", W.icon)
  store.set("last_hi",   W.hi and tostring(W.hi) or "")
  store.set("last_lo",   W.lo and tostring(W.lo) or "")
  store.set("last_when", W.when)
end

local function load_cache()
  W.loc = store.get("location", PRESETS[1])
  local lt = store.get("last_temp")
  if lt and store.get("last_loc", "") == W.loc then
    W.temp = tonumber(lt)
    W.cond = store.get("last_cond", "")
    W.icon = store.get("last_icon", "cloud")
    W.hi   = tonumber(store.get("last_hi", ""))
    W.lo   = tonumber(store.get("last_lo", ""))
    W.when = store.get("last_when", "")
    W.msg  = nil
  else
    clear_reading()
  end
end

local function preset_index()
  for k, v in ipairs(PRESETS) do if v == W.loc then return k end end
  return 1
end

-- ---- lifecycle -----------------------------------------------------
function on_show()
  load_cache()
  if sys.wifi() then fetch(W.loc) end
  return true
end

function on_draw()
  g.clear()
  local w = g.w()
  local cx = w // 2

  -- location
  local name = loc_name(W.loc or "--")
  g.text(cx - g.textw(name) // 2, 104, name)

  -- condition icon (96 px) centred
  g.icon(cx - 48, 150, (W.icon or "cloud") .. "@96")

  if W.msg then
    g.text(cx - g.textw(W.msg) // 2, 300, W.msg)
  else
    -- big current temperature
    local tnum = tostring(math.floor((W.temp or 0) + 0.5))
    draw_number(cx, 270, tnum, 68, 120, 16)

    -- condition words
    local ct = W.cond or ""
    g.text(cx - g.textw(ct) // 2, 424, ct)

    -- high / low
    if W.hi and W.lo then
      local hilo = string.format("%s %d\xC2\xB0    %s %d\xC2\xB0",
        tr("최고", "H"), math.floor(W.hi + 0.5),
        tr("최저", "L"), math.floor(W.lo + 0.5))
      g.text(cx - g.textw(hilo) // 2, 424 + g.line_h() + 8, hilo)
    end
    if W.when then
      local u = tr("갱신 ", "updated ") .. W.when
      g.text(cx - g.textw(u) // 2, 424 + 2 * (g.line_h() + 8), u)
    end
  end

  -- bottom controls
  local h = g.h()
  local refresh = tr("\xEC\x83\x88\xEB\xA1\x9C\xEA\xB3\xA0\xEC\xB9\xA8", "refresh")
  g.text(cx - g.textw(refresh) // 2, h - 132, refresh)
  g.rect(0, h - 92, w, 2, 3)
  local nav = tr("\xE2\x97\x80        \xEC\xA7\x80\xEC\x97\xAD        \xE2\x96\xB6",
                 "\xE2\x97\x80        city        \xE2\x96\xB6")
  g.text(cx - g.textw(nav) // 2, h - 60, nav)
end

function on_touch(x, y, phase)
  if phase ~= "up" then return false end
  local h, w = g.h(), g.w()

  if y >= h - 92 then                       -- city cycle
    local i = preset_index()
    if x < w // 2 then i = (i - 2) % #PRESETS + 1
    else               i = i % #PRESETS + 1 end
    W.loc = PRESETS[i]
    store.set("location", W.loc)
    clear_reading()
    W.msg = tr("불러오는 중...", "Loading...")
    fetch(W.loc)
    return true
  end
  if y >= h - 150 and y < h - 92 then       -- refresh
    fetch(W.loc)
    return true
  end
  return false
end
