#ifndef PROGRAM_OPTIONS_PARSER_
#define PROGRAM_OPTIONS_PARSER_

#include <util/error.h>
#include <util/optional.h>
#include <util/result.h>
#include <util/validate.h>

#include <set>
#include <sstream>
#include <string>

namespace program {

/**
 * @brief Parser for command-line options of the format `--test`/`-t`.
 *
 */
class OptionsParser {
   private:
    template <typename T>
    using validate_func_t = util::validate::validate_func<T>;

    template <typename In, typename Out>
    using convert_func_t = std::function<Out(const In&)>;

    using parse_func_t =
        std::function<util::result<void, util::error>(const std::string&)>;

   public:
    /**
     * @brief Type of a single option to be parsed.
     *
     */
    class Option {
       public:
        bool operator<(const Option& rhs) const;

        /**
         * @brief Creates a string of the format `"--option, -o"`.
         *
         * @return std::string Option name and flag
         */
        std::string ToString() const;

       private:
        Option(const std::string& option, char flag,
               util::optional<std::string> default_value,
               util::optional<std::string> description, bool is_boolean);

        bool IsRequired() const;

        std::string option_;
        char flag_;
        util::optional<std::string> default_value_;
        util::optional<std::string> description_;
        parse_func_t parser;
        bool is_boolean_ = false;
        int required_id_ = -1;

        friend class OptionsParser;
    };

   private:
    using option_set_t = std::set<Option>;

    /**
     * @brief Converts a string to a boolean value.
     *
     * @param str String
     * @param def Default value if conversion fails
     * @return true
     * @return false
     */
    static bool StringToBool(const std::string& str, bool def);

    /**
     * @brief Converts a boolean value to a string.
     *
     * @param b Boolean value
     * @return std::string `"true"` or `"false"`
     */
    static std::string BoolToString(bool b);

    template <typename In, typename Out = In>
    util::result<void, util::error> AddOptionImpl(
        const std::string& option, char flag, Out* const& destination,
        const util::optional<In>& default_value,
        const util::optional<std::string>& description,
        const util::optional<validate_func_t<In>>& validate,
        const util::optional<convert_func_t<In, Out>>& converter,
        bool required) {
        // Convert and assign default value.
        if (default_value.has_value()) {
            *destination = converter.has_value()
                               ? converter.value()(default_value.value())
                               : static_cast<Out>(default_value.value());
        }

        // Store default value as a string.
        util::optional<std::string> default_value_str;
        if (default_value.has_value()) {
            std::stringstream strm;
            strm << default_value.value();
            default_value_str = strm.str();
        }

        OptionsParser::Option new_option(option, flag, default_value_str,
                                         description, false);

        // This option is required, assign a new ID.
        if (required) {
            new_option.required_id_ = num_required_++;
        }

        // Create the parser.
        std::string full_string = new_option.ToString();
        new_option.parser =
            [destination, validate, converter, full_string](
                const std::string& str) -> util::result<void, util::error> {
            // Read input value and validate.
            std::stringstream strm;
            strm << str;
            In val;
            if (!(strm >> val) ||
                (validate.has_value() && !(validate.value())(val))) {
                return util::error("Invalid value for option " + full_string);
            }

            // Save to destination.
            *destination = converter.has_value() ? converter.value()(val)
                                                 : static_cast<Out>(val);

            return util::ok;
        };

        option_set_.insert(std::move(new_option));
        return util::ok;
    }

    // Specialization for boolean options.
    template <typename Out>
    util::result<void, util::error> AddOptionImpl(
        const std::string& option, char flag, Out* const& destination,
        const util::optional<bool>& default_value,
        const util::optional<std::string>& description,
        const util::optional<validate_func_t<bool>>& validate,
        const util::optional<convert_func_t<bool, Out>>& converter,
        bool required) {
        // Convert and assign default value.
        bool def = default_value.has_value() ? default_value.value() : false;
        *destination = converter.has_value() ? converter.value()(def)
                                             : static_cast<Out>(def);

        OptionsParser::Option new_option(option, flag, BoolToString(def),
                                         description, true);

        // This option is required, assign a new ID.
        if (required) {
            new_option.required_id_ = num_required_++;
        }

        // Create the parser.
        new_option.parser =
            [destination, validate, converter, option,
             flag](const std::string& str) -> util::result<void, util::error> {
            bool b = StringToBool(str, true);
            *destination = converter.has_value() ? converter.value()(b)
                                                 : static_cast<Out>(b);
            return util::ok;
        };

        option_set_.insert(std::move(new_option));
        return util::ok;
    }

   public:
    /**
     * @brief Adds a new option to the parser.
     *
     * @tparam In In type to parse
     * @tparam Out Out type to convert to
     * @param option Option name (`--option`)
     * @param flag Flag name (`-f`)
     * @param destination Memory destination
     * @param default_value Default value
     * @param description Description
     * @param validate Validation function
     * @param converter Conversion function from In to Out
     */
    template <typename In, typename Out = In>
    util::result<void, util::error> AddOption(
        const std::string& option, char flag, Out* const& destination,
        const util::optional<In>& default_value,
        const util::optional<std::string>& description,
        const util::optional<validate_func_t<In>>& validate,
        const util::optional<convert_func_t<In, Out>>& converter) {
        return AddOptionImpl(option, flag, destination, default_value,
                             description, validate, converter, false);
    }

    /**
     * @brief Adds a new required option to the parser.
     *
     * @tparam In In type to parse
     * @tparam Out Out type to convert to
     * @param option Option name (`--option`)
     * @param flag Flag name (`-f`)
     * @param destination Memory destination
     * @param default_value Default value
     * @param description Description
     * @param validate Validation function
     * @param converter Conversion function from In to Out
     */
    template <typename In, typename Out = In>
    util::result<void, util::error> AddOptionRequired(
        const std::string& option, char flag, Out* const& destination,
        const util::optional<In>& default_value,
        const util::optional<std::string>& description,
        const util::optional<validate_func_t<In>>& validate,
        const util::optional<convert_func_t<In, Out>>& converter) {
        return AddOptionImpl(option, flag, destination, default_value,
                             description, validate, converter, true);
    }

    /**
     * @brief Parses the command-line options.
     *
     * Returns the index after the last one read.
     *
     * If all arguments were read, will return argc.
     *
     * @param argc Argument count for `argv`
     * @param argv Argument vector
     * @return util::result<int, util::Error> Index after the last argument
     * processed, or error
     */
    util::result<int, util::error> Parse(int argc, char* argv[]) const;

    /**
     * @brief Prints the options and their descriptions to the console.
     *
     */
    void PrintOptions() const;

   private:
    option_set_t option_set_;
    int num_required_ = 0;
};

}  // namespace program

#endif  // PROGRAM_OPTIONS_PARSER_