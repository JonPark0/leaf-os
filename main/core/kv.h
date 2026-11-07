/*
 * Shared "key = value" text helpers. The SD language packs, SD app manifests
 * (app.txt) and per-app state files (state.txt) all use the same simple format:
 * one "key = value" per line, blank lines ignored, "#"-prefixed lines are
 * metadata / comments that the caller handles.
 */
#pragma once

#include <string>

namespace core::kv {

// Strip leading/trailing ASCII whitespace (and control chars <= ' ').
std::string trim(const std::string& s);

// Split on the first '='. Returns false (key/val untouched-ish) when there is
// no '=' or the key trims to empty. Both sides are trimmed.
bool split(const std::string& line, std::string& key, std::string& val);

}  // namespace core::kv
