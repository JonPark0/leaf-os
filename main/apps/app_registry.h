/*
 * Registry of built-in apps for the home grid.  Phase 2: a static table.
 * Later phases merge SD-card manifests and per-app "visible when" predicates
 * (e.g. the file manager only when a card is inserted).
 */
#pragma once

#include "app.h"
#include "i18n.h"

namespace apps {

struct BuiltinApp {
    const char* id;
    core::i18n::Msg name;      // grid label — resolved via tr() at build time
    const char* icon;          // name in gfx::uiIcons(); nullptr => no icon
    App* (*make)();             // heap-allocated; kernel takes ownership
    bool (*visible)();          // nullptr => always visible
};

const BuiltinApp* builtinApps(int& count);

}  // namespace apps
