// Copyright (c) 2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <util/settings.h>

#include <univalue.h>

namespace util {
namespace {

struct Source {
    SettingsSpan span;
    bool forced = false;
    bool config_file = false;
    bool config_file_top_level = false;

    Source(SettingsSpan span) : span(span) {}
    Source& SetForced() { forced = true; return *this; }
    Source& SetConfigFile(bool top_level) { config_file = true; config_file_top_level = top_level; return *this; }
};

template <typename Fn>
static void MergeSettings(const Settings& settings, const std::string& section, const std::string& name, Fn&& fn)
{
    if (auto* value = FindKey(settings.forced_settings, name)) {
        fn(Source(SettingsSpan(value)).SetForced());
    }
    if (auto* value = FindKey(settings.command_line_options, name)) {
        fn(Source(SettingsSpan(*value)));
    }
    if (!section.empty()) {
        if (auto* map = FindKey(settings.ro_config, section)) {
            if (auto* value = FindKey(*map, name)) {
                fn(Source(SettingsSpan(*value)).SetConfigFile(false));
            }
        }
    }
    SettingsSpan span;
    if (auto* map = FindKey(settings.ro_config, "")) {
        if (auto* value = FindKey(*map, name)) {
            span = SettingsSpan(*value);
        }
    }
    fn(Source(span).SetConfigFile(true));
}
} // namespace

SettingsValue GetSetting(const Settings& settings,
    const std::string& section,
    const std::string& name,
    bool ignore_default_section_config,
    bool skip_negated_command_line)
{
    SettingsValue result;
    MergeSettings(settings, section, name, [&](Source source) {
        // Weird behavior preserved for backwards compatibility: Apply negated
        // setting in otherwise ignored sections. A negated value in the top
        // level section is applied to network specific options, even though
        // non-negated values there would be ignored.
        const bool never_ignore_negated_setting = source.span.last_negated();

        // Weird behavior preserved for backwards compatibility: Take first
        // assigned value instead of last. In general, later settings take
        // precedence over early settings, but for backwards compatibility in
        // the config file the precedence is reversed.
        const bool reverse_precedence = source.config_file;

        // Skip settings in top-level config section if requested.
        if (ignore_default_section_config && source.config_file_top_level && !never_ignore_negated_setting) return;

        // Skip negated command line settings.
        if (skip_negated_command_line && source.span.last_negated()) return;

        //! Stick with highest priority value, if already set.
        if (!result.isNull()) return;

        if (!source.span.empty()) {
            result = reverse_precedence ? source.span.begin()[0] : source.span.end()[-1];
        } else if (source.span.last_negated()) {
            result = false;
        }
    });
    return result;
}

std::vector<SettingsValue> GetListSetting(const Settings& settings,
    const std::string& section,
    const std::string& name,
    bool ignore_default_section_config)
{
    std::vector<SettingsValue> result;
    bool prev_negated = false;
    bool prev_negated_empty = false;
    MergeSettings(settings, section, name, [&](Source source) {
        // Skip settings in top-level config section if requested.
        if (ignore_default_section_config && source.config_file_top_level) return;

        if (!source.span.empty()) {
            // Weird behavior preserved for backwards compatibility: Apply
            // config file settings even if negated on command line. Negating a
            // setting on command line will discard earlier settings on the
            // command line and settings in the config file, unless the negated
            // command line value is followed by non-negated value, in which
            // case config file settings will be brought back from the dead (but
            // earlier command line settings will still be discarded).
            const bool add_zombie_config_values = source.config_file && !prev_negated_empty;

            if (!prev_negated || add_zombie_config_values) {
                for (const auto& value : source.span) {
                    if (value.isArray()) {
                        result.insert(result.end(), value.getValues().begin(), value.getValues().end());
                    } else {
                        result.push_back(value);
                    }
                }
            }
        }

        prev_negated |= source.span.negated() > 0 || source.forced;
        prev_negated_empty |= source.span.last_negated() && result.empty();
    });
    return result;
}

bool HasIgnoredDefaultSectionConfigValue(const Settings& settings, const std::string& section, const std::string& name)
{
    bool has_ignored = true;
    MergeSettings(settings, section, name, [&](Source source) {
        // If top-level config value is unset, or any other value is set,
        // then no top-level setting is being ignored.
        has_ignored &= source.config_file_top_level ? !source.span.empty() : source.span.empty();
    });
    return has_ignored;
}

SettingsSpan::SettingsSpan(const std::vector<SettingsValue>& vec) : SettingsSpan(vec.data(), vec.size()) {}
const SettingsValue* SettingsSpan::begin() const { return data + negated(); }
const SettingsValue* SettingsSpan::end() const { return data + size; }
bool SettingsSpan::empty() const { return size == 0 || last_negated(); }
bool SettingsSpan::last_negated() const { return size > 0 && data[size - 1].isFalse(); }
size_t SettingsSpan::negated() const
{
    for (size_t i = size; i > 0; --i) {
        if (data[i - 1].isFalse()) return i; // Return number of negated values (position of last false value)
    }
    return 0;
}

} // namespace util
