#include "options_parser.h"

#include <util/console.h>

#include <algorithm>
#include <vector>

namespace program {

OptionsParser::Option::Option(const std::string& option, char flag,
                              util::optional<std::string> default_value,
                              util::optional<std::string> description,
                              bool is_boolean)
    : option_(option),
      flag_(flag),
      default_value_(default_value),
      description_(description),
      is_boolean_(is_boolean) {}

bool OptionsParser::Option::IsRequired() const { return required_id_ >= 0; }

std::string OptionsParser::Option::ToString() const {
    return "--" + option_ + ", -" + flag_;
}

bool OptionsParser::Option::operator<(const Option& rhs) const {
    return flag_ < rhs.flag_;
}

bool OptionsParser::StringToBool(const std::string& str, bool def) {
    std::string lower;
    std::transform(str.begin(), str.end(), std::back_inserter(lower),
                   [](unsigned char c) { return std::tolower(c); });
    if (lower == "true") {
        return true;
    } else if (lower == "false") {
        return false;
    } else {
        return def;
    }
}

std::string OptionsParser::BoolToString(bool b) { return b ? "true" : "false"; }

util::result<int, util::error> OptionsParser::Parse(int argc,
                                                    char* argv[]) const {
    std::vector<bool> required(num_required_);
    bool seeking_help;

    int i;
    for (i = 1; i < argc; ++i) {
        std::string curr = argv[i];

        // Manually finished parsing options.
        if (curr == "--") {
            ++i;
            break;
        }

        // Invalid option.
        if (curr[0] != '-') {
            return util::error("Unknown option " + curr);
        }

        auto eq = curr.find('=');
        bool is_option = curr[1] == '-';

        // Find the corresponding Option object based on the option or flag name
        // given.
        option_set_t::iterator matched;
        if (is_option) {
            std::string option_name = curr.substr(2, eq - 2);
            matched = std::find_if(option_set_.begin(), option_set_.end(),
                                   [&option_name](const Option& option) {
                                       return option_name == option.option_;
                                   });
        } else {
            char flag_char = curr[1];
            matched = std::find_if(option_set_.begin(), option_set_.end(),
                                   [&flag_char](const Option& option) {
                                       return flag_char == option.flag_;
                                   });
        }

        // Failed to match.
        if (matched == option_set_.end()) {
            return util::error("Unknown option " + curr.substr(0, eq));
        }

        if (matched->flag_ == 'h') {
            seeking_help = true;
        }

        if (matched->IsRequired()) {
            required[matched->required_id_] = true;
        }

        // Parse the value accordingly.
        util::result<void, util::error> result;
        if (is_option && eq != std::string::npos) {
            result = matched->parser(curr.substr(eq + 1));
        } else if (matched->is_boolean_) {
            result = matched->parser("");
        } else {
            if (i == argc - 1) {
                return util::error("Missing value for option " +
                                   matched->ToString());
            }
            result = matched->parser(argv[++i]);
        }

        RETURN_IF_ERROR(result);
    }

    // Failed to get all required arguments.
    if (!seeking_help && std::any_of(required.begin(), required.end(),
                                     [](bool b) { return !b; })) {
        return util::error("Missing 1 or more required arguments");
    }

    return i;
}

void OptionsParser::PrintOptions() const {
    const auto& max =
        std::max_element(option_set_.begin(), option_set_.end(),
                         [](const Option& a, const Option& b) {
                             return a.option_.size() < b.option_.size();
                         });

    auto spaces = max->option_.size() + 10;

    for (const auto& option : option_set_) {
        util::nolog::console::stream(option.ToString());
        if (option.description_.has_value()) {
            util::nolog::console::stream(
                std::string(spaces - option.option_.size() - 6, ' '));
            if (option.IsRequired()) {
                util::nolog::console::stream("[REQUIRED] ");
            }
            util::nolog::console::stream(option.description_.value());
        }

        if (option.default_value_.has_value()) {
            util::nolog::console::stream(
                util::manip::endl, std::string(spaces, ' '),
                "Default = ", option.default_value_.value());
        }
        util::nolog::console::stream(util::manip::endl);
    }
}

}  // namespace program