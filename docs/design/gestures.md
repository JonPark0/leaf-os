# System gestures and quick settings

Status: design, 2026-11-08.

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

## 2. Recognising the gestures

Zones, from `ui/metrics.h`:

- **Top strip**: `y < StatusBar::kHeight`, only while the status bar is
  visible. The strip is already the system's: `Screen` keeps its touches from
  the app (docs/UI.md), so nothing changes for apps.
- **Bottom band**: `y >= kScreenH - 24`. Apps own this area today.

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
  - Finger moves 24 px or more in any other direction, or 400 ms pass: give up
    on the gesture and deliver the held Down plus the Moves so far; the rest of
    the touch flows normally.
- **Down anywhere else**: delivered as now.

The only change apps can notice: a touch that starts in the bottom 24 px
reaches them a little later (on release, or after 400 ms). Lua apps still get
`down`, `move`, `up` in order, so this stays within Lua API level 1;
`docs/APP_KIT.md` notes it.

## 3. Go home

`Kernel::goHome()` is a queued navigation like push and pop: at the top of the
next tick it pops every app above the root (each gets onPause / onDestroy),
resumes home, and refreshes once. On home it does nothing.

A Lua app's `on_hide` runs as on any pop. An update download (`app_update`) is
cancelled the same way Back cancels it.

## 4. Quick settings

`QuickSettingsApp` (`apps/app_quick/`), a `PageApp` pushed by the pull-down. It
is a normal page, not an overlay, so it reuses the stack, the header and the
widgets.

| Row | Control |
|---|---|
| Brightness | `-` / value / `+` (the same 10% steps as the side keys) |
| Wi-Fi | on / off checkbox, with the network name or "off" |
| Clear screen | runs one full refresh to remove ghosting |
| Battery and time | text: "82% · 14:05" |
| 설정 | opens the settings app |

Back, a swipe up (go home) or picking an item closes it. A pull-down while it is
already open does nothing. In the reader's full-screen mode the status bar is
hidden, so the pull-down is unavailable there until the bar is shown again (a
tap in the middle of the page).

Brightness and Wi-Fi go through the same calls the side keys and 설정 → Wi-Fi
use, so settings stay in one place.

## 5. Settings

설정 → 화면 gets "제스처 사용" (on by default). Off: the arbiter delivers every
touch unchanged, and the strip keeps its current reserved behaviour.

## 6. Tests

- Host (`test_ui`): tap in the bottom band is delivered as Down+Up on release;
  60 px up fires swipe up; a sideways move or 400 ms gives the touch back with
  the held Down first; pull down needs 60 px; nothing fires with gestures off.
- Device: both gestures on home, a settings page, the reader, the Wi-Fi
  keyboard (bottom key row still types), the weather app; quick settings rows;
  go home from three levels deep; the setting off.
