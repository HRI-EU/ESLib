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

/**
 * @file EventSubscription.h
 * @brief Everything required for SubscriptionHandle and ScopedSubscription. This header
 * is designed to be minimal, so it can be included in headers of classes containing
 * ScopedSubscriptions without including the full header stack.
 */

#ifndef EVENTSUBSCRIPTION_H_
#define EVENTSUBSCRIPTION_H_

#include <iosfwd>

namespace ES {

// forward-define types that are only referred to
class EventParametersParserBase;

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
    // guard against empty reference
    if (subscriberCollection == NULL)
    {
      return;
    }
    subscriberCollection->removeHandler(subscriberId);
    subscriberCollection = NULL;
  }

  /**
   * @brief Clear the handle without removing the subscription.
   */
  void clear()
  {
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
    clear();
    return res;
  }
};

}  // namespace ES

#endif /* EVENTSUBSCRIPTION_H_ */
