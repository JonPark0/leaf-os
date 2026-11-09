# System gestures and quick settings

Status: design, 2026-11-08; revised after review, 2026-11-09.

## Goal

Two system-wide swipes that work on every screen, plus the quick settings page
the first one opens:

| Gesture | Starts in | Action |
|---|---|---|
| Pull down | the status bar strip | open 빠른 설정 (quick settings) |
| Swipe up | the bottom edge band | go home (close every app above home) |

Nothing moves with the finger: e-paper cannot animate, so a gesture acts once,
when the finger lifts, with one refresh. Taps everywhere keep working as now.

Out of scope: horizontal swipes (back), per-app gestures, long press.

## 1. Where it plugs in

`Kernel::stepInput` turns touch samples into Down / Move / Up and hands them to
`Screen::handleTouch`. A **gesture arbiter** sits between the two:

```
hal::touch -> Kernel::stepInput -> ui::GestureArbiter -> Screen::handleTouch
                                        |
                                        +-> Kernel: openQuickSettings() / goHome()
```

The arbiter is host-clean (`ui/gesture.{h,cpp}`): it takes `(x, y, phase,
time)` and returns what to do: deliver these touch events to the screen, or
fire a gesture. `tests/test_ui` covers it.

The kernel stays free of app types: a pull-down runs a callback that
`main.cpp` sets (`Kernel::onPullDown`), which pushes quick settings unless it is
already on top. Swipe up calls `Kernel::goHome()`. The kernel reads the
"gestures" setting and the status bar's visibility at each touch-down.

## 2. Recognising the gestures

Zones, from `ui/metrics.h`:

- **Top strip**: `y < StatusBar::kHeight`, only while the status bar is
  visible. The strip is already the system's: `Screen` keeps its touches from
  the app (docs/UI.md), so nothing changes for apps.
- **Bottom band**: `y >= kScreenH - 40`. Apps own this area today. The touch
  controller reports y no further down than 795, so a 24 px band would leave a
  20 px target; 40 px leaves 36 px.

Rules:

- **Down in the top strip**: never delivered (as now). On Up, if the finger
  moved down at least 60 px and less than 60 px sideways, fire *pull down*;
  otherwise nothing.
- **Down in the bottom band**: held back, not delivered yet.
  - Finger moves up 60 px or more (less than 60 px sideways), then lifts: fire
    *swipe up*. The app never sees this touch.
  - Finger lifts without that: deliver the held Down and then the Up, so the
    app sees an ordinary tap. All widgets act on release (Button, Checkbox,
    ListView, Keyboard, AppTile, the reader), so the result is the same.
  - Finger moves 24 px or more the other way (down, or more sideways than
    up), or 400 ms pass before it has moved up 8 px: give up on the gesture and
    deliver the held Down plus the Moves so far; the rest of the touch flows
    normally. Once the finger has moved up 8 px there is no time limit, so a
    slow swipe still counts.
- **Down anywhere else**: delivered as now.

The only change apps can notice: a touch that starts in the bottom 40 px
reaches them a little later (on release, or once it is clearly not a swipe
up). No widget changes its look on Down, so nothing shows the delay. Lua apps
still get `down`, `move`, `up` in order, so this stays within Lua API level 1;
`docs/APP_KIT.md` notes it.

A `TextView` page swipe that starts in the band goes home instead of turning
the page (60 px up beats its 48 px). The band is narrow; a swipe from higher up
turns the page as before.

## 3. Go home

`Kernel::goHome()` is a queued navigation like push and pop: at the top of the
next tick it closes every app above the root, top first (the top one gets
onPause, each gets onDestroy; the ones below were paused when they were
covered), resumes home, and refreshes once. A push queued in the same tick is
dropped. On home it does nothing.

A Lua app's `on_hide` runs as on any pop. An update download (`app_update`) is
cancelled the same way Back cancels it.

## 4. Quick settings

`QuickSettingsApp` (`apps/app_quick/`), a `PageApp` pushed by the pull-down. It
is a normal page, not an overlay, so it reuses the stack, the header and the
widgets.

| Row | Control | Afterwards |
|---|---|---|
| Brightness | `-` / value / `+` (the same 10% steps as the side keys) | stays open |
| Wi-Fi | on / off checkbox, with the network name or "off" | stays open |
| Clear screen | one full refresh of the screen underneath, to remove ghosting | closes |
| Battery and time | text: "배터리 82% · 14:05" | |
| 설정 | opens the settings app | closes |

Back or a swipe up (go home) also closes it. Clearing the quick settings page
itself would be pointless, so Clear screen closes it first and the full refresh
lands on the screen below. A pull-down while it is already open does nothing. In the reader's full-screen mode the status bar is
hidden, so the pull-down is unavailable there until the bar is shown again (a
tap in the middle of the page).

Brightness and Wi-Fi go through the same calls the side keys and 설정 → Wi-Fi
use, so settings stay in one place.

## 5. Settings

The 설정 list gets a row "제스처: 켜짐 / 꺼짐" (on by default); a tap toggles it.
설정 → 화면 is due for a layout rework, so the switch does not go there. Off:
the arbiter delivers every touch unchanged, and the strip keeps its current
reserved behaviour.

## 6. Tests

- Host (`test_ui`): tap in the bottom band is delivered as Down+Up on release;
  60 px up fires swipe up; a sideways or downward move, or 400 ms without an
  upward start, gives the touch back with the held Down first; a slow swipe
  still fires; pull down needs 60 px; nothing fires with gestures off.
- Device: both gestures on home, a settings page, the reader, the Wi-Fi
  keyboard (bottom key row still types), the weather app; quick settings rows;
  go home from three levels deep; the setting off.
