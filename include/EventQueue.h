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

#ifndef EVENTQUEUE_H
#define EVENTQUEUE_H

#include "EventSubscriberCollection.h"

#include <queue>
#include <tuple>

#include <mutex>

namespace ES {

/**
 * @brief Allows to asynchronously queue events to be fired at a later point in time.
 *
 * Events can be enqueued at any point in time. All enqueued events are passed to
 * their handlers when the process() method is called. This happens in the same order
 * in which the events were queued.
 *
 * This class is thread-safe. Events can be enqueued asynchronously without problems.
 * It is also possible to enqueue events while the queued events are being processed.
 * However, the newly queued events will only be handled with the next call to process().
 */
class EventQueue
{
private:

  // Private copy constructor and assignment operator to avoid memory leaks
  // after copying pointers. Ideally, this should be implemented, rather than
  // being made private.
  EventQueue(const EventQueue&);
  EventQueue& operator=(const EventQueue&);

  /**
   * Template-free base type for queued events.
   */
  class QueuedEventBase
  {
  public:
    // next event in queue, not owned!
    QueuedEventBase* next;

    QueuedEventBase() :
        next(NULL)
    {
    }

    virtual ~QueuedEventBase() = default;

    // call to pass the event to it's handlers
    virtual void fire() = 0;
  };

  /**
   * Event with arguments on the event queue.
   */
  template<typename ...Args>
  class QueuedEvent: public QueuedEventBase
  {
  private:
    // reference to handler collection
    const SubscriberCollection<Args...>* handlerRef;
    // event arguments
    std::tuple<Args...> args;

  public:
    QueuedEvent(const SubscriberCollection<Args...>* handlerRef,
        std::tuple<Args...> args) :
        handlerRef(handlerRef), args(args)
    {
    }
    virtual ~QueuedEvent() = default;

    virtual void fire()
    {
      // call event handlers
      handlerRef->call_tuple(args);
    }
  };

  /*
   * Uses a single-linked list to store the events.
   *
   * This allows for fast insertions at the queue tail as well for
   * an efficient get-and-clear operation during the process method.
   *
   * For every event added to the queue, we need to allocate memory
   * for the queue node. In case of the event queue, we need to do
   * so anyways since the events require concrete template values.
   * Thus, this approach is more efficient than a std container,
   * which would only be able to store pointers to queued event objects.
   *
   */
  // head
  QueuedEventBase* eventQueueHead;
  // tail
  QueuedEventBase* eventQueueTail;

  // mutex guarding the event queue
  // TODO maybe use a timed mutex here
  mutable std::recursive_mutex queueMutex;

public:
  /// @brief Create an empty event queue.
  EventQueue() :
      eventQueueHead(NULL), eventQueueTail(NULL)
  {
  }

  /**
   * @brief Destroy the event queue.
   *
   * Any unprocessed events are discarded.
   */
  ~EventQueue()
  {
    // destroy unhandled events if any
    if (eventQueueHead != NULL)
    {
      printf("Warning: destroying EventQueue while some events are still "
          "queued. Unhandled events will be discarded.\n");

      QueuedEventBase* cur = eventQueueHead;
      while (cur != NULL)
      {
        QueuedEventBase* next = cur->next;
        delete cur;
        cur = next;
      }
    }
  }

  /**
   * @brief Enqueue an event using the specified arguments.
   *
   * The event is added to the tail of the queue.
   *
   * @tparam Args event argument types
   *
   * @param handlerRef event handler collection
   * @param args event argument values
   */
  template<typename ...Args>
  void enqueue(const SubscriberCollection<typename std::decay<Args>::type...>* handlerRef, Args ... args)
  {
    enqueue_tuple(handlerRef, std::make_tuple(args...));
  }

  /**
   * @brief Enqueue an event, extracting event arguments from the given tuple.
   *
   * The event is added to the tail of the queue.
   *
   * @tparam Args event argument types
   *
   * @param handlerRef event handler collection
   * @param args event argument values
   */
  template<typename ...Args>
  void enqueue_tuple(const SubscriberCollection<Args...>* handlerRef,
      std::tuple<Args...> args)
  {
    // create queued event object
    auto* event = new QueuedEvent<Args...>(handlerRef, args);

    // aquire mutex, freed automatically on return
    std::lock_guard<std::recursive_mutex> guard(queueMutex);

    // push on queue
    if (eventQueueHead == NULL)
    {
      eventQueueHead = event;
    }
    else
    {
      eventQueueTail->next = event;
    }
    eventQueueTail = event;
  }

  /**
   * @brief Process all events in the queue.
   *
   * Will ignore events that get queued during processing.
   *
   * @return false if the queue was empty.
   */
  bool process()
  {
    // unique_lock makes sure exceptions are handled
    std::unique_lock<std::recursive_mutex> guard(queueMutex, std::defer_lock);
    // aquire mutex
    // TODO maybe a timed try-lock, to ensure the main loop isn't blocked forever.
    guard.lock();
    // grab queue
    QueuedEventBase* cur = eventQueueHead;
    if (cur == NULL)
    {
      // queue is empty
      return false;
    }

    // and clear head/tail references, thus the queue is empty now
    eventQueueHead = NULL;
    eventQueueTail = NULL;

    // now the queue is detached from this, so we can unlock.
    guard.unlock();

    // loop through event queue
    while (cur != NULL)
    {
      // fire event to handlers
      cur->fire();
      // delete event object
      QueuedEventBase* next = cur->next;

      delete cur;
      cur = next;
    }
    return true;
  }

  /**
   * @brief Process all events in the queue. If more events are queued during processing, they are picked up as well.
   */
  void processUntilEmpty()
  {
    // simply call process until the queue is empty.
    while (process());
  }

  /**
   * @brief Process the first queued event, if any.
   *
   * @return true if an event was processed.
   */
  bool processOne() {
    // unique_lock makes sure exceptions are handled
    std::unique_lock<std::recursive_mutex> guard(queueMutex, std::defer_lock);
    // aquire mutex
    // TODO maybe a timed try-lock, to ensure the main loop isn't blocked forever.
    guard.lock();

    // obtain head queued event
    QueuedEventBase* toProcess = eventQueueHead;
    if (toProcess == NULL) {
      // queue is empty
      return false;
    }

    // remove from head
    eventQueueHead = toProcess->next;
    // reset tail if empty
    if (toProcess == eventQueueTail) {
      eventQueueTail = NULL;
    }

    // can unlock now
    guard.unlock();

    // finally, process the event
    toProcess->fire();

    // delete queued event object
    delete toProcess;

    return true;
  }

  /**
   * @brief Compute the number of events waiting to be processed.
   */
  size_t size() const
  {
    // unique_lock makes sure exceptions are handled
    std::unique_lock<std::recursive_mutex> guard(queueMutex, std::defer_lock);

    size_t eCount = 0;
    // aquire mutex
    // it's ok to block the mutex during the entire loop, since the enqueuing will happen asynchronously.
    // TODO maybe a timed try-lock, to ensure the main loop isn't blocked forever.
    guard.lock();
    // grab queue
    QueuedEventBase* cur = eventQueueHead;

    // loop through event queue
    while (cur != NULL)
    {
      eCount++;
      QueuedEventBase* next = cur->next;
      cur = next;
    }

    // can unlock now
    guard.unlock();

    return eCount;
  }

  /**
   * @brief Check if there are any waiting events.
   *
   * This method should be preferred over size() if the exact number of elements doesn't matter,
   * since it is more efficient.
   */
  bool isEmpty() const
  {
    // unique_lock makes sure exceptions are handled
    // the constructor also aquires the lock directly
    std::unique_lock<std::recursive_mutex> guard(queueMutex);

    // the queue is empty if the head element is NULL.
    return eventQueueHead == NULL;
  }

  /**
   * @brief Remove all events from the queue without invoking their event handlers.
   */
  void clear()
  {
    // unique_lock makes sure exceptions are handled
    std::unique_lock<std::recursive_mutex> guard(queueMutex, std::defer_lock);
    // aquire mutex
    // it's ok to block the mutex during the entire loop, since the enqueuing will happen asynchronously.
    // TODO maybe a timed try-lock, to ensure the main loop isn't blocked forever.
    guard.lock();
    // grab queue
    QueuedEventBase* cur = eventQueueHead;
    // and clear head/tail references, thus the queue is empty now
    eventQueueHead = NULL;
    eventQueueTail = NULL;

    // can unlock now
    guard.unlock();

    // loop through event queue
    while (cur != NULL)
    {
      // delete event object
      QueuedEventBase* next = cur->next;
      delete cur;
      cur = next;
    }

  }

};

// now, we can define the implementation of EventParametersParser::enqueueEvent
template<typename ... Args>
inline void EventParametersParser<Args...>::enqueueEvent(EventQueue* queue,
    const std::vector<std::string>& parameterStrings) const
{
  queue->enqueue_tuple(subscriberCollection, parseArgs(parameterStrings));
}

}  // namespace ES

#endif /* EVENTQUEUE_H */
