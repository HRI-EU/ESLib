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

#ifndef EVENTREGISTRY_H
#define EVENTREGISTRY_H

#include "EventSubscriberCollection.h"

#include <map>
#include <string>
#include <sstream>
#include <mutex>

namespace ES
{

/**
 * @brief EventRegistry matches events by their decayed argument types.
 *
 * This is legitimate since most events are queued. In the event queue, the arguments are stored in a tuple,
 * which applies the same operation.
 *
 * For example, this allows to define an event parameter as T, and subscribe using a const T& parameter.
 */
template<typename... Args>
using SubscriberCollectionDecay = SubscriberCollection<typename std::decay<Args>::type...>;


/**
 * @brief Stores subscriber collections for named events.
 *
 * Every event is uniquely identified by it's name.
 * Additionally, every event has a list of event argument types. The argument types do not
 * matter for the event selection. When retrieving a registered SubscriberCollection, the
 * specified argument types must match the ones the event was registered with. Due to the
 * dynamic typing involved, mismatched argument types will only be detected at runtime.
 *
 * However, since the event argument values are passed by value, the same transformations
 * will be applied to the event argument types when obtaining a SubscriberCollection. A full
 * list of these transformations can be found at https://en.cppreference.com/w/cpp/types/decay.
 * The most important rule is that references and plain const/volatile modifiers are removed.
 * Thus, an event defined with `registerEvent<std::string&&>` can be retrieved using
 * `getSubscribers<const std::string&>`. In both cases, the returned subscriber collection has the
 * type `SubscriberCollection<std::string>`. On the other hand, const/volatile qualifiers on
 * pointers are left untouched, so `std::string*` and `const std::string*` are not compatible,
 * even though there is an implicit conversion from the first to the second.
 *
 * The registry operations are guarded by the @link getRegistryMutex() registry mutex@endlink,
 * so the registry object is thread safe. The child subscriber collections are not guarded by
 * that mutex, so care must be taken when using them.
 */
class EventRegistry
{
private:
  // subscriber collections for named events
  // the subscriber collection objects are owned by the event system.
  std::map<std::string, SubscriberCollectionBase*> subscribersByEventName;
  // mutex for registry map
  mutable std::mutex registryMutex;
public:

  ~EventRegistry()
  {
    // delete all registered subscriber collections
    for (auto& entry: subscribersByEventName)
    {
      delete entry.second;
    }
    subscribersByEventName.clear();
  }

  /**
   * @brief Get the mutex which guards all interactions with the registry.
   *
   * This mutex ensures that the registry is thread-safe. However, the
   * individual subscriber collections are not guarded by this mutex.
   */
  std::mutex& getRegistryMutex() const
  {
    return registryMutex;
  }

  /**
   * @brief Get a map of all registered events.
   *
   * The result is a const view of the EventRegistry's internal storage. It can be used for introspection.
   *
   * This method does not aquire the @link getRegistryMutex() registry mutex@endlink.
   * If you deal with concurrent execution, you must aquire it manually.
   */
  const std::map<std::string, SubscriberCollectionBase*>& getRegisteredEvents() const
  {
    return subscribersByEventName;
  }

  template<typename ...Args>
  bool hasRegisteredEvent(std::string name)
  {
    // aquire mutex - released automatically on return
    std::unique_lock<std::mutex> lock(registryMutex);

    // check if the event was registered before
    auto existing = subscribersByEventName.find(name);

    // no such name - return failure
    if (existing == subscribersByEventName.end())
    {
      return false;
    }

    // duplicate, check that the template args match
    SubscriberCollectionBase* hcBase = existing->second;

    // use dynamic_cast to properly detect failure
    auto hc = dynamic_cast<SubscriberCollectionDecay<Args...>* >(hcBase);

    if (hc == NULL)
    {
      // the cast failed, so the template arguments do not match.
      return false;
    }

    // We found it - return success
    return true;
  }

  /**
     * @brief Register a named event and return the subscriber collection for it.
     *
     * The subscriber collection will be owned by the event system.
     *
     * @tparam Args event argument types
     * @param name event name
     *
     * @return subscriber collection for the registered event.
     *
     * @throws std::invalid_argument if the event is already registered.
     */
  template<typename ...Args>
  SubscriberCollectionDecay<Args...>* registerEvent(std::string name)
  {
    // aquire mutex - released automatically on return
    std::unique_lock<std::mutex> lock(registryMutex);
    // check if the event was registered before
    auto existing = subscribersByEventName.find(name);
    if (existing != subscribersByEventName.end())
    {
      // duplicate
      std::ostringstream os;
      os << "The event named '" << name << "' has already been registered!";
      throw std::invalid_argument(os.str());
    }

    // allocate subscriber collection
    auto hc = new SubscriberCollectionDecay<Args...>;

    // store it
    subscribersByEventName.emplace(name, hc);

    // return the new subscriber collection
    return hc;
  }

  /**
   * @brief Retrieve the subscriber collection for the given named event.
   *
   * The event must already have been registered, and the event argument types must match the ones
   * this event was registered with.
   *
   * @tparam Args event argument types
   * @param name event name
   *
   * @return subscriber collection for the registered event, or NULL if the event was not registered.
   *
   * @throws std::invalid_argument if the event argument types don't match.
   */
  template<typename ...Args>
  SubscriberCollectionDecay<Args...>* getSubscribers(std::string name)
  {
    // aquire mutex - released automatically on return
    std::unique_lock<std::mutex> lock(registryMutex);
    // locate in map
    auto entry = subscribersByEventName.find(name);
    if (entry == subscribersByEventName.end())
    {
      // does not exist
      return NULL;
    }
    SubscriberCollectionBase* hcBase = entry->second;
    // now, cast to the concrete templated SubscriberCollection.
    // use dynamic_cast to properly detect failure
    auto hc = dynamic_cast<SubscriberCollectionDecay<Args...>* >(hcBase);
    if (hc == NULL)
    {
      // the cast failed, so the template arguments do not match.
      std::ostringstream os;
      os << "The argument types for the event named '" << name << "' are:" << std::endl <<"\t";
      hcBase->appendEventArgsDescription(os);
      os << std::endl << "but getHandlers was invoked with:" << std::endl <<"\t";
      appendArgsDescription<Args...>(os);
      throw std::invalid_argument(os.str());
    }
    // return it
    return hc;
  }

  /**
   * @brief Retrieve the subscriber collection for the given named event, without typecasting.
   *
   * Use this to access the parts that aren't type dependent. C++ is smart and will
   * automatically choose the right overload.
   *
   * @param name event name
   *
   * @return subscriber collection for the registered event, or NULL if the event was not registered.
   */
  SubscriberCollectionBase* getSubscribers(std::string name)
  {
    // aquire mutex - released automatically on return
    std::unique_lock<std::mutex> lock(registryMutex);
    // locate in map
    auto entry = subscribersByEventName.find(name);
    if (entry == subscribersByEventName.end())
    {
      // does not exist
      return NULL;
    }
    return entry->second;
  }

  /**
   * @brief Retrieve the subscriber collection for the given named event, or create it if not found.
   *
   * If the event is already registered, the event argument types must match the ones
   * this event was registered with.
   *
   * This method should be used with caution. When multiple clients call this function for the
   * same event name, but with different argument types, the first client will register the event,
   * and all later ones will get an error.
   *
   * @tparam Args event argument types
   * @param name event name
   *
   * @return subscriber collection for the registered event.
   *
   * @throws std::invalid_argument if the event is known and the event argument types don't match.
   */
  template<typename ...Args>
  SubscriberCollectionDecay<Args...>* getOrRegister(std::string name)
  {
    // aquire mutex - released automatically on return
    std::unique_lock<std::mutex> lock(registryMutex);
    // check if the event was registered before
    auto existing = subscribersByEventName.find(name);
    if (existing != subscribersByEventName.end())
    {
      // duplicate, check that the template args match
      SubscriberCollectionBase* hcBase = existing->second;
      // use dynamic_cast to properly detect failure
      auto hc = dynamic_cast<SubscriberCollectionDecay<Args...>* >(hcBase);
      if (hc == NULL)
      {
        // the cast failed, so the template arguments do not match.
        std::ostringstream os;
        os << "The argument types for the event named '" << name << "' are:" << std::endl <<"\t";
        hcBase->appendEventArgsDescription(os);
        os << std::endl << "but getHandlers was invoked with:" << std::endl <<"\t";
        appendArgsDescription<Args...>(os);
        throw std::invalid_argument(os.str());
      }
      // return it
      return hc;
    }

    // allocate subscriber collection
    auto hc = new SubscriberCollectionDecay<Args...>;

    // store it
    subscribersByEventName.emplace(name, hc);

    // return the new subscriber collection
    return hc;
  }

  /**
   * @brief Print a description of all registered events to the given output stream.
   *
   * @param os Output stream to print to.
   */
  void print(std::ostream& os) const
  {
    // aquire mutex - released automatically on return
    std::unique_lock<std::mutex> lock(registryMutex);

    // loop through registered events
    for (auto& entry : subscribersByEventName)
    {
      std::string eventName = entry.first;
      SubscriberCollectionBase* event = entry.second;

      unsigned int pCount = event->getParametersParser()->getParameterCount();

      switch (pCount)
      {
        case 0:
          os << "Event " << eventName << " with " << pCount << " arguments" << std::endl;
          break;

        case 1:
        {
          os << "Event " << eventName << " with 1 argument of type ";

          switch (event->getParametersParser()->getParameterType(0))
          {
            case ParameterType::INT:
              os << "INT" << std::endl;
              break;

            case ParameterType::DOUBLE:
              os << "DOUBLE" << std::endl;
              break;

            case ParameterType::STRING:
              os << "STRING" << std::endl;
              break;

            case ParameterType::BOOL:
              os << "BOOL" << std::endl;
              break;

            default:
              os << "UNSUPPORTED: ";
              event->appendEventArgsDescription(os);
              os << std::endl;
              break;
          }
        }
        break;

        default:
          os << "Event " << eventName << " with " << pCount << " arguments: ";
          event->appendEventArgsDescription(os);
          os << std::endl;
          break;
      }

    }
  }
};

}  // namespace ES

#endif // EVENTREGISTRY_H
