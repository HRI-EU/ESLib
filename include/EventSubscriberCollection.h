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

#ifndef EVENTSUBSCRIBERCOLLECTION_H
#define EVENTSUBSCRIBERCOLLECTION_H

#include "EventSystemUtils.h"
#include "EventParametersParser.h"

#include <functional>
#include <vector>
#include <tuple>
#include <ostream>
#include <array>

namespace ES {

/**
 * @brief Type of ignore_result.
 */
struct ignore_result_t
{
};
/**
 * @brief Tag value to ignore the return value of subscribers.
 *
 * By default, the addSubscriber methods only allow void return types.
 * This tag explicitly allows a non-void result from a subscriber.
 * The result will be discarded on invocation.
 */
constexpr ignore_result_t ignore_result { };

/**
 * @brief Append a description of the template args to the given output stream.
 */
template<typename ...Args>
void appendArgsDescription(std::ostream& os)
{
  // array of argument type names
  std::array<std::string, sizeof...(Args)> argNames
          { detail::getTypeName<Args>()...};

  if (argNames.empty()) {
    // parameterless
    os << "[]";
    return;
  }

  // prepend '['
  os << "[";
  // concat separated by ', '
  auto it = argNames.begin();
  if (it != argNames.end())
  {
    while (true)
    {
      os << *it;
      it++;
      if (it != argNames.end())
      {
        os << ", ";
      }
      else
      {
        break;
      }
    }
  }
  // append ']'
  os << "]";
}

/**
 * @brief Template-free base class for handler collections.
 *
 * The EventFactory stores the HandlerCollections by name, independently of
 * their event argument template values. Every concrete SubscriberCollection inherits
 * from this class to provide common functionality.
 *
 */
class SubscriberCollectionBase
{
public:
  // virtual destructor to allow deleting base references.
  virtual ~SubscriberCollectionBase() = default;

  /**
   * @brief Returns the number of subscribers in this handler collection.
   */
  virtual std::size_t getHandlerCount() const = 0;

  /**
   * @brief Append a description of the event arguments to the given output stream.
   *
   * This method is intended for debugging purposes. It allows to query the event arguments
   * of any handler collection without using dynamic_cast.
   *
   * @param os output stream to append to
   */
  virtual void appendEventArgsDescription(std::ostream& os) const = 0;

  /**
   * @brief Get the parameter parser, which can call this event using string arguments.
   */
  virtual const EventParametersParserBase* getParametersParser() const = 0;

private:
  /*
   * The handler id system is private, clients should use the SubscriptionHandle wrapper.
   */
  friend class SubscriptionHandle;
  template<typename... Args>
  friend class SubscriberCollection;

  // type of the handler identifier.
  typedef unsigned int SubscriberIdType;

  // remove a subscriber by id. Used by SubscritionHandle.
  virtual void removeHandler(SubscriberIdType handlerId) = 0;
};

/**
 * @brief A handle for a subscriber registration.
 *
 * Since there is no general concept of equality for function handles,
 * we cannot define a removeHandler() method taking the functor object.
 * Instead, SubscriberCollection::addSubscriber returns a reference to the
 * added handler, which can be used for removal if needed.
 *
 * Use the unsubscribe() method to remove the handler.
 *
 * A SubscriptionHandle does not own the subscription, so when the handle
 * goes out of scope, the subscriber is not removed.
 * This allows to simply ignore the return value of addSubscriber if you never
 * want to remove the subscription.
 *
 * If automatic unsubsciption is desired, use ScopedSubscription instead.
 */
class SubscriptionHandle
{
private:
  // subscriber collection holding the referenced subscriber.
  // may be NULL for an empty reference
  SubscriberCollectionBase* subscriberCollection;
  // referenced subscriber id
  SubscriberCollectionBase::SubscriberIdType subscriberId;

  // allow access from SubscriberCollection
  template<typename ...Args>
  friend class SubscriberCollection;
  // create from values, private so it's usage is limited to SubscriberCollection
  SubscriptionHandle(SubscriberCollectionBase* handlerCollection,
      SubscriberCollectionBase::SubscriberIdType handlerId) :
      subscriberCollection(handlerCollection), subscriberId(handlerId)
  {
  }
public:
  /**
   * @brief Creates an empty subscription handle.
   *
   * An empty subscription handle does not reference any subscriber.
   * Calling it's unsubscribe() method does nothing.
   */
  SubscriptionHandle() :
      subscriberCollection(NULL), subscriberId(0)
  {
  }

  // copy constructor/assignment operator will be created implicitly

  /**
   * @brief Remove the referenced subscriber.
   *
   * Does nothing on an empty handle.
   * This handle is automatically turned into an empty handle when done.
   *
   * IMPORTANT: This must not be called while the event that the handler was
   * registered for is processed. Otherwise, you might get unexpected errors.
   *
   */
  void unsubscribe()
  {
    // guard against re-
    if (subscriberCollection == NULL)
    {
      return;
    }
    subscriberCollection->removeHandler(subscriberId);
    subscriberCollection = NULL;
  }
};

/**
 * @brief An owning subscription handle. The handler will be removed
 * automatically once this object goes out of scope.
 *
 * Since the ownership is unique, this object is not copyable, but movable.
 */
class ScopedSubscription: public SubscriptionHandle
{
public:
  /**
   * @brief Creates an empty subscription.
   */
  ScopedSubscription() = default;

  /**
   * @brief Calls unsubscribe to remove the handler.
   */
  ~ScopedSubscription()
  {
    unsubscribe();
  }

  /**
   * @brief Take ownership of a non-owning subscription handle.
   *
   * @param handle handle to take ownership of.
   */
  ScopedSubscription(const SubscriptionHandle& handle) :
      SubscriptionHandle(handle)
  {
  }
  /**
   * @brief Take ownership of a non-owning subscription handle.
   *
   * If this object already owns a subscription, it will be unsubscribed first.
   *
   * @param handle handle to take ownership of.
   */
  ScopedSubscription& operator=(const SubscriptionHandle& handle)
  {
    // if this still owns a reference, unsubscribe that reference
    unsubscribe();
    // copy members from handle
    SubscriptionHandle::operator=(handle);
    return *this;
  }

  // don't allow copying subscriptions due to unique ownership
  /// \cond
  ScopedSubscription(const ScopedSubscription&) = delete;
  ScopedSubscription& operator=(const ScopedSubscription&) = delete;
  /// \endcond

  /**
   * @brief Move ownership from the given subscription to this object.
   */
  ScopedSubscription(ScopedSubscription&& other) :
      // disown other and assign it to this
      SubscriptionHandle(other.release())
  {
  }
  /**
   * @brief Move ownership from the given subscription to this object.
   *
   * If this object already owns a subscription, it will be unsubscribed first.
   */
  ScopedSubscription& operator=(ScopedSubscription&& other)
  {
    // if this still owns a reference, unsubscribe that reference
    unsubscribe();
    // disown other and assign it to this
    *this = other.release();
    return *this;
  }

  /**
   * @brief Turn this subscription into an empty reference without unsubscribing.
   *
   * @return a SubscritionHandle to the formally referenced subscriber.
   */
  SubscriptionHandle release()
  {
    SubscriptionHandle res = *this;
    *this = SubscriptionHandle();
    return res;
  }
};

/**
 * @brief A collection of subscribers for one event type.
 *
 * The event type is defined by the argument types of it's subscriber functions.
 *
 *
 * @tparam Args argument types for the event function
 */
template<typename ...Args>
class SubscriberCollection: public SubscriberCollectionBase
{
private:
  struct Subscriber
  {
    // handler id, used for subscription handles
    SubscriberIdType id;
    // use std::function to mask the actual form of the defined function
    std::function<void(Args...)> function;

    Subscriber(SubscriberIdType id, std::function<void(Args...)> function) :
        id(id), function(function)
    {
    }
  };

  // list of subscriber implementations.
  std::vector<Subscriber> handlers;

  // handler id counter, produces incrementing ids for new handlers
  SubscriberIdType newIdCounter;

  // parameter from string parser
  EventParametersParser<Args...> paramParser;

public:
  /**
   * @brief Creates an new, empty handler collection.
   */
  SubscriberCollection() :
      newIdCounter(0), paramParser(this)
  {
  }
  virtual ~SubscriberCollection() = default;

  /*
   * @brief Returns the number of subscribers in this handler collection
   */
  virtual std::size_t getHandlerCount() const
  {
    return handlers.size();
  }
  /**
   * @brief Append a description of the event arguments to the given output stream.
   */
  virtual void appendEventArgsDescription(std::ostream& os) const
  {
    appendArgsDescription<Args...>(os);
  }

  /**
   * @brief Get the parameter parser, which can call this event using string arguments.
   */
  virtual const EventParametersParserBase* getParametersParser() const
  {
    return &paramParser;
  }

private:
  // add the given subscriber function. Common code for addSubscriber(func) and addSubscriber(func, ignore_result)
  SubscriptionHandle addSubscriberImpl(std::function<void(Args...)>&& function)
  {

    SubscriberIdType newId = newIdCounter++;
    handlers.emplace_back(newId, function);

    // return subscription handle.
    return
    { this, newId};
  }

  virtual void removeHandler(SubscriberIdType handlerId)
  {
    // look for the referenced handler in list
    for (auto it = handlers.begin(); it != handlers.end(); it++)
    {
      if (it->id == handlerId)
      {
        // found handler, remove it
        handlers.erase(it);
        return;
      }
    }
  }

public:

  /**
   * @name addSubscriber
   *
   * Add a subscriber to this handler collection.
   * The subscriber must be a callable object of the correct signature.
   *
   * By default, only functions returning void are allowed as subscribers.
   * A non-void return value can be explicitly ignored using the ignore_result overload.
   *
   * IMPORTANT: This must not be called while the event that the handler collection was
   * registered for is processed. Otherwise, you might get unexpected errors.
   */
  //@{
  /**
   * @brief Add a function object as event subscriber. The handler function must return void.
   *
   * @tparam Func function object type. Must be callable with Args.
   *
   * @param function subscriber function object
   *
   * @return a handle that can be used to remove the newly registered subscriber.
   */
  template<typename Func>
  SubscriptionHandle addSubscriber(Func function)
  {
    // on c++11, std::function<void> silently ignores void results. We don't want that.
    static_assert(std::is_void<decltype(function(std::declval<Args>()...)) >::value,
        "Only functions returning void are allowed as handlers. If the return value should be ignored, "
        "use the ignore_result_t overload. ");

    // construct a std::function from func.
    // this will handle the signature check, and it works for any callable object.
    std::function<void(Args...)> func_wrapper(function);

    return addSubscriberImpl(std::move(func_wrapper));
  }

  /**
   * @brief Add a function object as subscriber. The return value of the handler function is ignored.
   *
   * @tparam Func function object type. Must be callable with Args.
   *
   * @param function handler function object
   *
   * @return a handle that can be used to remove the newly registered subscriber.
   */
  template<typename Func>
  SubscriptionHandle addSubscriber(Func function, ignore_result_t)
  {

    // construct a std::function from func, ignoring the return value
    // this will handle the signature check, and it works for any callable object.
    std::function<void(Args...)> func_wrapper =
        detail::ignore_return_value_wrapper<Func, Args...>(function);

    return addSubscriberImpl(std::move(func_wrapper));
  }
  // convenience versions of addSubscriber for member functions

  /**
   * @brief Add a bound member function as subscriber. The handler function must return void.
   *
   * @tparam T owner type of the member function.
   * @tparam MFArgs Argument types for the member function. These are
   * different from the event Args to allow for implicit conversions.
   *
   * @param fp member function pointer
   * @param obj owner object to bind to
   *
   * @return a handle that can be used to remove the newly registered subscriber.
   */
  template<typename ... MFArgs, typename T>
  SubscriptionHandle addSubscriber(void (T::*fp)(MFArgs...),
      type_identity_t<T>* obj)
  {
    return addSubscriber(detail::bind_owner(fp, obj));
  }
  /**
   * @brief Add a bound const member function as subscriber. The handler function must return void.
   *
   * @tparam T owner type of the member function.
   * @tparam MFArgs Argument types for the member function. These are
   * different from the event Args to allow for implicit conversions.
   *
   * @param fp member function pointer
   * @param obj owner object to bind to
   *
   * @return a handle that can be used to remove the newly registered subscriber.
   */
  template<typename ... MFArgs, typename T>
  SubscriptionHandle addSubscriber(void (T::*fp)(MFArgs...) const,
      const type_identity_t<T>* obj)
  {
    return addSubscriber(detail::bind_owner(fp, obj));
  }

  /**
   * @brief Add a bound member function as subscriber. The return value of the handler function is ignored.
   *
   * @tparam T owner type of the member function.
   * @tparam MFArgs Argument types for the member function. These are
   * different from the event Args to allow for implicit conversions.
   *
   * @param fp member function pointer
   * @param obj owner object to bind to
   *
   * @return a handle that can be used to remove the newly registered subscriber.
   */
  template<typename ... MFArgs, typename T, typename Result>
  SubscriptionHandle addSubscriber(Result (T::*fp)(MFArgs...),
      type_identity_t<T>* obj, ignore_result_t)
  {
    return addSubscriber(detail::bind_owner(fp, obj), ignore_result);
  }

  /**
   * @brief Add a bound const member function as subscriber. The return value of the handler function is ignored.
   *
   * @tparam T owner type of the member function.
   * @tparam MFArgs Argument types for the member function. These are
   * different from the event Args to allow for implicit conversions.
   *
   * @param fp member function pointer
   * @param obj owner object to bind to
   *
   * @return a handle that can be used to remove the newly registered subscriber.
   */
  template<typename ... MFArgs, typename T, typename Result>
  SubscriptionHandle addSubscriber(Result (T::*fp)(MFArgs...) const,
      const type_identity_t<T>* obj, ignore_result_t)
  {
    return addSubscriber(detail::bind_owner(fp, obj), ignore_result);
  }
  //@}

  /**
   * @brief Call all registered subscribers using the specified arguments.
   *
   * Since this will loop through all subscribers, you must not add or remove handlers while a call is in progress.
   *
   * @param args event argument values
   */
  void call(Args ... args) const
  {
    // loop through subscribers
    for (auto& handler : handlers)
    {
      // invoke one handler
      handler.function(args...);
    }
  }
  /**
   * @brief Call all registered subscribers, extracting event arguments from the given tuple.
   *
   * Since this will loop through all subscribers, you must not add or remove handlers while a call is in progress.
   *
   * @tparam Tuple argument tuple type
   * @param args argument tuple
   */
  template<class Tuple>
  void call_tuple(Tuple&& args) const
  {
    // loop through subscribers
    for (auto& handler : handlers)
    {
      // invoke one handler
      detail::apply(handler.function, args);
    }
  }

};

// now, we can define the implementation of EventParametersParser::callEvent
template<typename ... Args>
inline void EventParametersParser<Args...>::callEvent(
    const std::vector<std::string>& parameterStrings) const
{
  subscriberCollection->call_tuple(parseArgs(parameterStrings));
}

}  // namespace ES

#endif /* EVENTSUBSCRIBERCOLLECTION_H */
