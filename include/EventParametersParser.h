/*******************************************************************************

  Copyright (c) 2017, Honda Research Institute Europe GmbH.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.

  3. Neither the name of the copyright holder nor the names of its
     contributors may be used to endorse or promote products derived from
     this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY EXPRESS OR
  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
  IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY DIRECT, INDIRECT,
  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
  OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

#ifndef EVENTPARAMETERSPARSER_H
#define EVENTPARAMETERSPARSER_H

#include "EventSystemUtils.h"

#include <vector>
#include <stdexcept>
#include <type_traits>
#include <tuple>
#include <sstream>
#include <array>

namespace ES {

/**
 * @brief Generic types of event arguments
 */
enum class ParameterType {
  /// @brief A string, no conversion is necessary
  STRING,
  /// @brief Truth value
  BOOL,
  /// @brief Integral number
  INT,
  /// @brief Floating point number
  DOUBLE,

  /// @brief Used for types that cannot be created from a string.
  UNSUPPORTED
};

/**
 * @brief Converts strings to parameter values.
 *
 * All parameter parsers must be specializations of this template struct.
 *
 * Every specialization must define:
 * - A static constexpr field 'valueType' of type ParameterType.
 * - A static method parse, taking a std::string as argument and returning the parsed value.
 *
 * The parse method should throw std::invalid_argument if the string is malformed.
 *
 * The default specialization has a valueType of UNSUPPORTED, and it's parse method throws std::logic_error.
 *
 * @tparam T parse result type
 * @tparam SFINAE unused, but may be used to enable conditional specializations.
 */
template<typename T, typename SFINAE = void>
struct ParameterValueParser {
  /**
   * @brief Type code for T.
   *
   * This is unsupported for the default specialization since we cannot parse arbitrary values.
   */
  static constexpr ParameterType valueType = ParameterType::UNSUPPORTED;

  /**
   * @brief Convert the given string to a value of type T.
   *
   * For the default specialization, this method throws std::logic_error.
   *
   * @param stringValue string to parse
   * @return parsed value
   * @throws std::invalid_argument if the passed string cannot be parsed to a value of type T.
   * @throws std::logic_error if valueType is UNSUPPORTED, thus the parse can never succeed.
   */
  static T parse(const std::string& stringValue) {
    throw std::logic_error("Cannot parse an unsupported parameter type.");
  }
};

// use template specialization to define parsers for types

/// @brief ParameterValueParser for std::string parameters
template<>
struct ParameterValueParser<std::string> {
  static constexpr ParameterType valueType = ParameterType::STRING;

  static std::string parse(const std::string& stringValue) {
    return stringValue;
  }
};

/// @brief ParameterValueParser for bool parameters
template<>
struct ParameterValueParser<bool> {
  static constexpr ParameterType valueType = ParameterType::BOOL;



  static bool parse(const std::string& stringValue) {
    // supports "true" and "false" in arbitrary case
    if (matches(stringValue, "true")) {
      return true;
    }
    if (matches(stringValue, "false")) {
      return false;
    }
    throw std::invalid_argument("Illegal boolean value");
  }

private:
  // a helper for lower-case string comparison.
  static bool matches(const std::string& value, const char* expected) {
    if (value.size() != strlen(expected))
      return false;

    for (int i = 0; i < value.size(); ++i) {
      if (tolower(value[i]) != expected[i]) {
        return false;
      }
    }
    return true;
  }
};

/// @brief ParameterValueParser for integral number parameters
template<typename T>
struct ParameterValueParser<T, typename std::enable_if<std::is_integral<T>::value>::type> {
  static constexpr ParameterType valueType = ParameterType::INT;

  static int parse(const std::string& stringValue) {
    std::istringstream is(stringValue);
    int result;
    if(!(is >> result)) {
      // invalid input format
      throw std::invalid_argument("Illegal integer value");
    }
    return result;
  }
};

/// @brief ParameterValueParser for floating point number parameters
template<typename T>
struct ParameterValueParser<T, typename std::enable_if<std::is_floating_point<T>::value>::type> {
  static constexpr ParameterType valueType = ParameterType::DOUBLE;

  static double parse(const std::string& stringValue) {
      std::istringstream is(stringValue);
      double result;
      if(!(is >> result)) {
        // invalid input format
        throw std::invalid_argument("Illegal integer value");
      }
      return result;
    }
};

// forward define event queue and handler collection, they are required for the forward-declared call methods.
class EventQueue;

template<typename... Args>
class SubscriberCollection;

/**
 * @brief Interface for argument type introspection of a generic event.
 *
 * Allows to query argument count and types. Additionally, it provides
 * methods to call or queue events using arguments parsed from strings.
 */
class EventParametersParserBase {
public:
  virtual ~EventParametersParserBase() = default;

  /**
   * @brief Checks if all arguments can be parsed from strings.
   */
  bool canParseArgs() const {
    for (unsigned int idx = 0; idx < getParameterCount(); ++idx) {
      if (getParameterType(idx) == ParameterType::UNSUPPORTED) {
        return false;
      }
    }
    return true;
  }

  /**
   * @brief Get the number of parameters.
   */
  virtual unsigned int getParameterCount() const = 0;

  /**
   * @brief Get the type of a parameter.
   *
   * @param parameterIdx parameter index
   */
  virtual ParameterType getParameterType(unsigned int parameterIdx) const = 0;

  /**
   * @brief Call the event immediately, using the given string arguments.
   *
   * @param parameterStrings argument string values. The number of entries must match getParameterCount().
   *
   * @throws std::invalid_argument if the parameter count is wrong, or if a parameter string is malformed.
   * @throws std::logic_error if canParseArgs() returns false.
   */
  virtual void callEvent(const std::vector<std::string>& parameterStrings) const = 0;

  /**
   * @brief Enqueue the event, using the given string arguments.
   *
   * @param queue queue to put the event on.
   * @param parameterStrings argument string values. The number of entries must match getParameterCount().
   *
   * @throws std::invalid_argument if the parameter count is wrong, or if a parameter string is malformed.
   * @throws std::logic_error if canParseArgs() returns false.
   */
  virtual void enqueueEvent(EventQueue* queue, const std::vector<std::string>& parameterStrings) const = 0;
};


/**
 * @brief EventParametersParserBase implementation for a specific set of argument types.
 */
template <typename... Args>
class EventParametersParser : public EventParametersParserBase {
private:
  // number of parameters
  static constexpr std::size_t paramCount = sizeof...(Args);

  // parse the event arguments into a tuple
  std::tuple<Args...> parseArgs(const std::vector<std::string>& parameterStrings) const {
    // check vector length
    if (parameterStrings.size() != paramCount) {
      std::ostringstream os;
      os << "Wrong number event arguments, expected " << paramCount << " but got " << parameterStrings.size();
      throw std::invalid_argument(os.str());
    }
    return parseArgsImpl(parameterStrings, detail::make_index_sequence<paramCount> {});
  }
  // helper, includes the required index sequence
  template<std::size_t... I>
  std::tuple<Args...> parseArgsImpl(
      const std::vector<std::string>& parameterStrings, detail::index_sequence<I...>
  ) const {
    return std::make_tuple(
        ParameterValueParser<Args>::parse(parameterStrings.at(I))...);
  }

  // handler collection this parser was created for
  // not owned
  SubscriberCollection<Args...>* subscriberCollection;

  // allow constructor access to subscriber collection
  friend class SubscriberCollection<Args...>;
  EventParametersParser(SubscriberCollection<Args...>* subscriberCollection) :
      subscriberCollection(subscriberCollection) {}

public:
  virtual ~EventParametersParser() = default;

  // introspection
  virtual unsigned int getParameterCount() const {
    return paramCount;
  }
  virtual ParameterType getParameterType(unsigned int parameterIdx) const {
    // parameter types.
    // use std::array since it supports a length of zero for parameterless methods.
    static std::array<ParameterType, paramCount> paramTypes = { ParameterValueParser<Args>::valueType... };
    return paramTypes[parameterIdx];
  }

  // the call operators are defined later, since they depend on the SubscriberCollection/EventQueue implementation.
  virtual void callEvent(const std::vector<std::string>& parameterStrings) const;
  virtual void enqueueEvent(EventQueue* queue, const std::vector<std::string>& parameterStrings) const;

};

}  // namespace ES

#endif /* EVENTPARAMETERSPARSER_H */
