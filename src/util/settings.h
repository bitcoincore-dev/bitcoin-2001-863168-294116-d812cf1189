// Copyright (c) 2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UTIL_SETTINGS_H
#define BITCOIN_UTIL_SETTINGS_H

#include <map>
#include <string>
#include <vector>

class UniValue;

namespace util {

//! Settings value type (string/integer/boolean/null variant).
//!
//! @note UniValue is used here for convenience and because it can be easily
//!       serialized in a readable format. But any other tagged union type could
//!       be substituted if there's a need to move away from UniValue.
using SettingsValue = UniValue;

//! Stored bitcoin settings. This struct combines settings from the command line
//! and a static configuration file.
struct Settings {
    std::map<std::string, std::vector<SettingsValue>> command_line_options;
    std::map<std::string, std::map<std::string, std::vector<SettingsValue>>> ro_config;
    std::map<std::string, SettingsValue> forced_settings;
};

//! Get settings value from combined sources: forced settings, command line
//! arguments, the static config file.
//!
//! @param ignore_default_section_config - ignore values set in the top-level
//!                                        section of the config file.
//! @param skip_nonpersistent - ignore non-persistent settings values (forced
//!                             settings values and values specified on the
//!                             command line). Only return settings in the
//!                             static config file.
SettingsValue GetSetting(const Settings& settings,
    const std::string& section,
    const std::string& name,
    bool ignore_default_section_config,
    bool skip_nonpersistent);

//! Get combined setting value similar to Get(), except if setting was
//! specified multiple times, return a list of all the values that were
//! specified.
std::vector<SettingsValue> GetListSetting(const Settings& settings,
    const std::string& section,
    const std::string& name,
    bool ignore_default_section_config);

//! Return true if has ignored config value in the default section.
bool HasIgnoredDefaultSectionConfigValue(const Settings& settings, const std::string& section, const std::string& name);

//! Iterable list of settings that skips negated values.
struct SettingsSpan {
    const SettingsValue* data;
    size_t size;

    SettingsSpan(const SettingsValue* data, size_t size) : data(data), size(size) {}
    SettingsSpan(const SettingsValue* value = nullptr) : SettingsSpan(value, value ? 1 : 0) {}
    SettingsSpan(const std::vector<SettingsValue>& vec);
    const SettingsValue* begin() const;
    const SettingsValue* end() const;
    bool empty() const;
    bool last_negated() const;
    size_t negated() const;
};

} // namespace util

#endif // BITCOIN_UTIL_SETTINGS_H
