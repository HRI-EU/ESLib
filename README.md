EventSystem - C++ publish/subscribe
===================================

The EventSystem library provides a decoupled publish/subscribe event model.
Event topics are registered by name and can have one or more typesafe paramerers.
Subscribers can be lambdas, free functions or member functions.
Events can be published asynchronously, and will be stored in a queue for synchronous processing.


```c++
ES::EventSystem es;

// subscribe with lambda
es.subscribe("Event", [](std::string arg) {
	std::cout << "Got argument " << arg << std::endl;
});

// subscribe with member function
Foo foo;
es.subscribe("AnotherEvent", &Foo::handleEvent, &foo);

// Multiple parameters are supported
es.subscribe("MultiparamEvent", [](std::string arg1, int arg2) {
	...
});

// enqueue an event
std::string eventContent = "Hello World!";
es.publish("Event", eventContent);

// process queued events
es.process();
```

Installation 
===================================
EventSystem is a header-only library with no external dependencies. To use it, simply add the `include` directory to your include path. Alternatively, you can install it using CMake:

	mkdir build && cd build
	cmake ..
	sudo make install

Then, you can use `find_package(EventSystem)` to locate the installation. The installed include directory path is stored in the CMake variable `EventSystem_INCLUDE_DIR`.

The library requires a language level of C++11. It has been tested with GCC 4.8 and Visual Studio 2015.

Documentation
===================================

The documentation is built using Doxygen. You can generate it through CMake:

	mkdir build && cd build
	cmake ..
	make doc

The generated documentation can be found in `doc/html`.

License
===================================

This project is licensed under the BSD 3-clause license - see the [LICENSE.md](LICENSE.md) file for details.
Some source code has been adapted from the pybind11 library (https://pybind11.readthedocs.io) which is released
under the BSD 3-clause license.

Disclaimer
===================================

The copyright holders are not liable for any damage(s) incurred due to improper use of this software.

