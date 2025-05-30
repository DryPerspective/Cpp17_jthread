# Cpp17_jthread

A simple, minimal, (mostly) lock-free implementation of jthread in C++17. Includes `stop_source`, `stop_token`, `stop_callback`, and `jthread`.

The tools in this library match the interface of the C++20 standard tools. The implementation is lock-free in most aspects, and utilises the little-used `std::atomic_foo` overloads for `std::shared_ptr` which live in `<memory>`. The underlying operations on the `stop_source` family are free from data races. While not all operations are truly atomic, they should behave in the correct way with the correct synchronisation. For example, `stop_source::swap` cannot be completely atomic, but rely on a `compare_exchange` loop to ensure it has the proper behaviour. Operations on `jthread` are not internally synchronised (consistent with `std::jthread`). Concurrent access to `jthread` objects must be protected by the user. All tools in this repo exist in `namespace dp`.

As with the standard tool, `jthread` objects will request a stop on destruction if they are joinable.

```cpp
int main(){

	dp::jthread t1{[](dp::stop_token token){
		while(!token.stop_requested()){		
			std::cout << "Hello from thread\n";
			std::this_thread::sleep_for(std::chrono::seconds{5});	
		}

	}};

	//Code on main thread goes here.

} //request_stop() automatically called
```

Equally, multiple `jthread` objects may all share a single stop source and have their stops requested simultaneously:

```cpp
int main(){

	dp::stop_source src{};
	auto token = src.get_token();
	
	std::vector<dp::jthread> threads{};
	for(int i = 0; i < 5; ++i){
		threads.push_back(dp::jthread{[token]{while(!token.stop_requested()){/*...*/}}});_
	}

	//Do things on the main thread

	//Request all threads stop
	src.request_stop();

}
```

## Lock Free Specification

Most tools to manage state in this repo are lock free and wait free. Querying stop state via `stop_requested()` is always wait-free. Requesting a stop via `request_stop()` will only cause waiting in the case where a callback is being registered or deregistered simultaneously. As such, if the user either avoids stop callbacks or guarantees that a callback will not be being registered or deregistered while a stop is being requested, then requesting a stop is always wait-free. There may be some small waiting if multiple callbacks are being registred or deregistered simultaneously.

## Installation

This library is as simple as a collection of header and implementation files. Ensure that the `include` directory is added to your include path and all implementation files are being compiled and all should work out.

