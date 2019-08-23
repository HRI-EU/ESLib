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

#include "EventRegistry.h"
#include "EventQueue.h"

#include "EventSystem.h"

#include "test_subscriptiononly.h"

#include <iostream>

using namespace std;

void event1_handler1(const std::string& strArg)
{
  cout << "event1_handler1 got " << strArg << endl;
}

void string_stealer(std::string&& strArg)
{
  std::string stolen(std::move(strArg));
  cout << "string_stealer got " << stolen << endl;
  cout << "=> Arg is now " << strArg << endl;
}

void event1_handler2(std::string strArg)
{
  cout << "event1_handler2 got " << strArg << endl;
}

void event1_handler_temp(std::string strArg)
{
  cout << "event1_handler_temp got " << strArg << endl;
}

void event2_handler1(int intArg)
{
  cout << "event2_handler1 got " << intArg << endl;
}

struct Foo
{
  void event2_handler2(int intArg)
  {
    cout << "event2_handler2 got " << intArg << endl;
  }

  void const_handler(int intArg) const {
    cout << "const_handler got " << intArg << endl;
  }

  void const_arg_handler(const std::string& strArg) {
    cout << "const_arg_handler got " << strArg << endl;
  }
};

struct Parent {
  virtual ~Parent() = default;

  virtual void inherited_handler(int intArg) {
    cout << classname() << " got " << intArg << endl;
  }

  virtual const char* classname() const {
    return "Parent";
  }

  void overloaded_handler(std::string strArg) {
    cout << "overloaded_handler(string) got " << strArg << endl;
  }
  void overloaded_handler(int intArg) {
    cout << "overloaded_handler(int) got " << intArg << endl;
  }
};

struct Child : public Parent {
  virtual const char* classname() const {
    return "Child";
  }
};


void event3_handler1(int intArg, double doubleArg)
{
  cout << "event3_handler1 got " << intArg << " and " << doubleArg << endl;
}

void const_pointer_handler(const char* str) {
  cout << "const_pointer_handler got " << str << endl;
}

void pointer_handler(char* str) {
  cout << "pointer_handler got " << str << endl;
}

bool returning_handler(int param) {
  cout << "returning_handler got " << param << endl;
  return true;
}

int main()
{
  size_t nErrors = 0;

  // create event system
  ES::EventRegistry es;

  // register events
  auto event1 = es.registerEvent<std::string>("event1");
  event1->addSubscriber(event1_handler1);
  event1->addSubscriber(string_stealer);
  event1->addSubscriber(event1_handler2);

  cout << event1->getHandlerCount() << endl;

  event1->call("A text");

  // try getting handler list
  auto event1ref2 = es.getSubscribers<std::string>("event1");
  event1ref2->call("A text 2");

  auto event2 = es.registerEvent<int>("event2");
  event2->addSubscriber(event2_handler1);

  Foo foo;
  event2->addSubscriber(&Foo::event2_handler2, &foo);
  event2->addSubscriber(&Foo::const_handler, const_cast<const Foo*>(&foo));
  event1->addSubscriber(&Foo::const_arg_handler, &foo);

  auto event3 = es.registerEvent<int, double>("event3");
  event3->addSubscriber(event3_handler1);
  event3->call(3, 3.5);

  // with return value
  event2->addSubscriber(returning_handler, ES::ignore_result);

  // event queue
  {
    ES::EventQueue queue;

    queue.enqueue<std::string>(event1, "Hello");
    queue.enqueue(event2, 42);
    queue.enqueue<std::string>(event1, "World");

    // fire them
//    queue.process();
    cout << "=== First queued event ===" << endl;
    queue.processOne();
    cout << "=== Second queued event ===" << endl;
    queue.processOne();
    cout << "=== Third queued event ===" << endl;
    queue.processOne();
    cout << "Has more events: " << boolalpha << queue.processOne() << endl;

    queue.enqueue<std::string>(event1, "Unhandled");
  }

  // test subscriber handles
  ES::SubscriptionHandle handle = event1->addSubscriber(event1_handler_temp);
  event1->call("With temp");
  cout << "Subscribed before release: " << boolalpha << handle.isSubscribed() << endl;
  release_subscription(handle);
  cout << "Subscribed after release: " << boolalpha << handle.isSubscribed() << endl;
  event1->call("Without temp");

  // the same with subscription
  {
    ES::ScopedSubscription subs = event1->addSubscriber(event1_handler_temp);
    event1->call("With temp");
  }
  event1->call("Without temp");

  {
    // Test stuff with event system
    cout << endl;

    ES::EventSystem es;

    es.registerEvent<int>("TestEvent");

    Parent parent;
    es.subscribe("TestEvent", &Parent::inherited_handler, &parent);

    es.subscribe<int>("TestEvent", &Parent::overloaded_handler, &parent);

    Child child;
    es.subscribe("TestEvent", &Parent::inherited_handler, &child);

    es.subscribe("TestEvent", event2_handler1);

    // test lambda with capture param
    es.subscribe("TestEvent", [child](int param){
      cout << "Lambda capturing "<< child.classname() <<" got " << param << endl;
    });


    // test constness
    es.registerEvent<std::string>("StrEvent");
    try {
      es.subscribe("StrEvent", event1_handler1);
    } catch (std::exception& ex) {
      nErrors++;
      cout << "Error adding StrEvent handler: " << ex.what() << endl;
    }
    es.subscribe<std::string>("StrEvent", &Parent::overloaded_handler, &parent);
    es.publish<std::string>("StrEvent", "Test");

    es.registerEvent<char*>("ConstEvent");
    es.subscribe<char*>("ConstEvent", const_pointer_handler);
    es.subscribe("ConstEvent", pointer_handler);

    char nonConstChar[6] = "Hello";
    es.publish("ConstEvent", nonConstChar);

    es.publish("TestEvent", 1);

    es.process();

    // test processing single event
    es.publish("TestEvent", 1);
    es.publish<std::string>("StrEvent", "Str1");
    es.publish("TestEvent", 2);
    es.publish<std::string>("StrEvent", "Str2");

    es.processNamed("StrEvent");

    es.process();
  }

  // Test stuff with arg parsing
  cout << endl;

  size_t paramCount = event3->getParametersParser()->getParameterCount();
  cout << "Param count: " << paramCount << endl;

  if (paramCount != 2)
  {
    nErrors++;
  }

  bool isInt = (event3->getParametersParser()->getParameterType(0) == ES::ParameterType::INT);
  cout << "Param #1 type is int: " << isInt << endl;

  if (!isInt)
  {
    nErrors++;
  }

  // test with parsed args
  event3->getParametersParser()->callEvent({"10", "2.5"});

  auto event4 = es.registerEvent<bool>("event4");
  event4->addSubscriber([](bool arg) {cout << "Boolean value: " << boolalpha << arg << endl;});
  event4->getParametersParser()->callEvent({"True"});
  event4->getParametersParser()->callEvent({"fAlSe"});

  es.print(cout);

  cout << endl << "Test revealed " << nErrors << " errors" << endl;

  return 0;
}
