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

#ifndef EVENTSYSTEM_H_
#define EVENTSYSTEM_H_

#include "EventRegistry.h"
#include "EventQueue.h"

namespace ES {

/**
 * @brief Wrapper around EventRegistry and EventQueue to provide a simplified interface.
 *
 * You can add subscribers using the subscribe() methods, enqueue events using the publish() method,
 * and process queued events using the process() method.
 *
 * The registerEvent() method is not strictly required. All methods us TODO
 *
 */
class EventSystem : public EventRegistry
{
public:

  /**
   * @brief Register a named event and return the handler collection for it.
   *
   * The handler collection will be owned by the event system.
   *
   * @tparam Args event argument types
   * @param name event name
   *
   * @return handler collection for the registered event.
   */
  template<typename ...Args>
  SubscriberCollectionDecay<Args...>* registerEvent(std::string name)
  {
    return getOrRegister<Args...>(name);
  }

private:
  // helper to get a HandlerCollection for a specific signature
  // the function pointer parameter allows to deduce Args from a function type
  template<typename Return, typename ...Args>
  SubscriberCollectionDecay<Args...>* getForSignature(std::string name, Return (*)(Args...)) {
    return getOrRegister<Args...>(name);
  }
public:

  /**
   * @name subscribe
   *
   * Register a subscriber for the given named event.
   *
   * The event argument types are usually derived from the passed function, but that is not possible
   * in every case. For those cases, you can specify the event arguments explicitly. If even that does
   * not work, it's recommended to use a lambda:
   *
   * @code
   * es.subscribe([](AT1 a1, AT2 a2) { my_fn(a1, a2); });
   * @endcode
   *
   * By default, only functions returning void are allowed as subscribers.
   * A non-void return value can be explicitly ignored using the ignore_result overload.
   *
   * IMPORTANT: This must not be called while the event that the handler collection was
   * registered for is processed. Otherwise, you might get unexpected errors.
   */
  //@{

  /**
   * @brief Subscribe with a free function or lambda. The event argument types are derived from the
   * function signature.
   *
   * This method will not work if the event argument types cannot be derived from the function.
   * In particular, this is the case for the result of std::bind, or generally of any
   * function object with more than one call operator definition.
   *
   * @param name      event name
   * @param function  handler function
   * @return          a handle that can be used to remove the newly registered subscriber.
   */
  template<typename Func>
    SubscriptionHandle subscribe(std::string name, Func function)
  {
    auto e = getForSignature(name, (detail::function_signature_t<Func>*) nullptr);
    return e->addSubscriber(function);
  }

  /**
   * @brief Subscribe with a free function or lambda. The event argument types are derived from the
   * function signature. The return value of the handler function is ignored.
   *
   * This method will not work if the event argument types cannot be derived from the function.
   * In particular, this is the case for the result of std::bind, or generally of any
   * function object with more than one call operator definition.
   *
   * @param name      event name
   * @param function  handler function
   * @return          a handle that can be used to remove the newly registered subscriber.
   */
  template<typename Func>
    SubscriptionHandle subscribe(std::string name, Func function, ignore_result_t)
  {
    auto e = getForSignature(name, (detail::function_signature_t<Func>*) nullptr);
    return e->addSubscriber(function, ignore_result);
  }


  /**
   * @brief Subscribe with a free function or lambda. The event argument types must be specified explicitly.
   *
   * This overload exists to support functions whose signature differs from the event argument types, or
   * whose signature cannot be derived.
   *
   * @tparam Args event argument types
   *
   * @param name      event name
   * @param function  handler function
   * @return          a handle that can be used to remove the newly registered subscriber.
   */
  template<typename... Args, typename Func, typename = typename std::enable_if<sizeof...(Args) != 0>::type>
    SubscriptionHandle subscribe(std::string name, Func function)
  {
    auto e = getOrRegister<Args...>(name);
    return e->addSubscriber(function);
  }

  /**
   * @brief Subscribe with a free function or lambda. The event argument types must be specified explicitly.
   * The return value of the handler function is ignored.
   *
   * This overload exists to support functions whose signature differs from the event argument types, or
   * whose signature cannot be derived.
   *
   * @tparam Args event argument types
   *
   * @param name      event name
   * @param function  handler function
   * @return          a handle that can be used to remove the newly registered subscriber.
   */
  template<typename... Args, typename Func, typename = typename std::enable_if<sizeof...(Args) != 0>::type>
    SubscriptionHandle subscribe(std::string name, Func function, ignore_result_t)
  {
    auto e = getOrRegister<Args...>(name);
    return e->addSubscriber(function, ignore_result);
  }

  /**
   * @brief Subscribe with a member function.
   *
   * The event argument types will typically be derived from the passed function pointer.
   * However, you can also specify them explicitly, thus forcing a specific overload to
   * be used without an explicit static_cast. Note that C++ has no way to distinguish
   * between an empty argument list and a non-specified argument list, so it's not possible
   * to select a parameterless overload this way.
   *
   * @tparam Args event argument types
   *
   * @param name  event name
   * @param fp    member function pointer
   * @param obj   owner object to bind to
   * @return      a handle that can be used to remove the newly registered subscriber.
   */
  template<typename ...Args, typename T>
    SubscriptionHandle subscribe(std::string name, void(T::*fp)(Args...), type_identity_t<T>* obj)
  {
    auto e = getOrRegister<Args...>(name);
    return e->addSubscriber(fp, obj);
  }

  /**
   * @brief Subscribe with a const member function.
   *
   * The event argument types will typically be derived from the passed function pointer.
   * However, you can also specify them explicitly, thus forcing a specific overload to
   * be used without an explicit static_cast. Note that C++ has no way to distinguish
   * between an empty argument list and a non-specified argument list, so it's not possible
   * to select a parameterless overload this way.
   *
   * @tparam Args event argument types
   *
   * @param name  event name
   * @param fp    member function pointer
   * @param obj   owner object to bind to
   * @return      a handle that can be used to remove the newly registered subscriber.
   */
  template<typename ...Args, typename T>
    SubscriptionHandle subscribe(std::string name, void(T::*fp)(Args...) const, const type_identity_t<T>* obj)
  {
    auto e = getOrRegister<Args...>(name);
    return e->addSubscriber(fp, obj);
  }

  /**
   * @brief Subscribe with a member function. The return value of the handler function is ignored.
   *
   * The event argument types will typically be derived from the passed function pointer.
   * However, you can also specify them explicitly, thus forcing a specific overload to
   * be used without an explicit static_cast. Note that C++ has no way to distinguish
   * between an empty argument list and a non-specified argument list, so it's not possible
   * to select a parameterless overload this way.
   *
   * @tparam Args event argument types
   *
   * @param name  event name
   * @param fp    member function pointer
   * @param obj   owner object to bind to
   * @return      a handle that can be used to remove the newly registered subscriber.
   */
  template<typename ...Args, typename Return, typename T>
  SubscriptionHandle subscribe(std::string name, Return(T::*fp)(Args...), type_identity_t<T>* obj, ignore_result_t)
  {
    auto e = getOrRegister<Args...>(name);
    return e->addSubscriber(fp, obj, ignore_result);
  }

  /**
   * @brief Subscribe with a const member function. The return value of the handler function is ignored.
   *
   * The event argument types will typically be derived from the passed function pointer.
   * However, you can also specify them explicitly, thus forcing a specific overload to
   * be used without an explicit static_cast. Note that C++ has no way to distinguish
   * between an empty argument list and a non-specified argument list, so it's not possible
   * to select a parameterless overload this way.
   *
   * @tparam Args event argument types
   *
   * @param name  event name
   * @param fp    member function pointer
   * @param obj   owner object to bind to
   * @return      a handle that can be used to remove the newly registered subscriber.
   */
  template<typename ...Args, typename Return, typename T>
  SubscriptionHandle subscribe(std::string name, Return(T::*fp)(Args...) const, const type_identity_t<T>* obj,
      ignore_result_t)
  {
    auto e = getOrRegister<Args...>(name);
    return e->addSubscriber(fp, obj, ignore_result);
  }
  //@}

  /**
   * @brief Enqueue an event.
   *
   * The event is added to the event queue.
   *
   * This method is thread safe, it can be invoked from a background thread.
   *
   * @tparam Args event argument types
   * @param name event name
   * @param args event argument values
   *
   * @return `false` if the event was not registered because there are no subscribers.
   */
  template<typename ...Args>
  bool publish(std::string name, Args... args)
  {
    auto e = getSubscribers<Args...>(name);
    if (e == NULL)
    {
      // not registered
      return false;
    }

    // enqueue event
    dynamicQueue.enqueue(e, args...);

    return true;
  }

  /**
   * @brief Call an event.
   *
   * The event is called directlz.
   *
   * This method is not thread safe.
   *
   * @tparam Args event argument types
   * @param name event name
   * @param args event argument values
   *
   * @return `false` if the event was not registered because there are no subscribers.
   */
  template<typename ...Args>
  bool call(std::string name, Args... args)
  {
    auto e = getSubscribers<Args...>(name);
    if (e == NULL)
    {
      // not registered
      return false;
    }

    // enqueue event
    e->call(args...);

    return true;
  }

  /**
   * @brief Process all queued events from the inner event queue.
   */
  void process()
  {
    dynamicQueue.process();
  }

  /**
   * @brief Process all events in the queue. If more events are queued during processing, they are picked up as well.
   */
  int  processUntilEmpty(int maxProcessCalls=-1)
  {
    return dynamicQueue.processUntilEmpty(maxProcessCalls);
  }

  /**
   * @brief Process all queued events for a single named event.
   * @param name event name
   */
  void processNamed(std::string name)
  {
    auto subscribers = getSubscribers(name);
    dynamicQueue.processForSubscribers(subscribers);
  }
protected:
  /// @brief queue for published events
  EventQueue dynamicQueue;
};

}  // namespace ES

#endif // EVENTSYSTEM_H_
