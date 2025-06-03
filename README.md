# C++17 `jthread`

A simple, minimal, (mostly) lock-free implementation of jthread in C++17. Includes `jthread`, `stop_source`, and `condition_variable_any`.

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
		threads.emplace_back([token]{while(!token.stop_requested()){/*...*/}});_
	}

	//Do things on the main thread

	//Request all threads stop
	src.request_stop();

}
```

And condition variables can wait on and be notified when a stop is requested

```cpp
void some_thread_function(dp::condition_variable_any& cond, dp::stop_token token){

	perform_setup();
	shared_progress_marker.store(true);

	//Will be notified either by a call to notify on the condvar, or a stop request on the source associated with the token
	cond.wait(shared_lock, token, [&progress_marker]{return shared_progress_marker.load() == false;})

	if(token.stop_requested()) return;

	perform_next_calculation();
}
```

## Lock Free Specification

The most potentially high-contention tools and functions to manage state in this repo are lock free and wait free. Querying stop state via `stop_requested()` is always wait-free. Requesting a stop via `request_stop()` will only cause some small waiting if there is contention between registering or deregistering a callback, and executing all callbacks. As such, if the user either avoids stop callbacks or guarantees that a callback will not be being registered or deregistered while a stop is being requested, then requesting a stop is always wait-free. There may be some small waiting if multiple callbacks are being registred or deregistered simultaneously.

Condition variables are built on locks, and this library's implementation is no exception. Locks are used internally.

## Installation

This library is as simple as a collection of header and implementation files. Ensure that the `include` directory is added to your include path and all implementation files are being compiled and all should work out.

## Documentation

A full writeup of the tools in this repo can be found on [its wiki](https://github.com/DryPerspective/Cpp17_jthread/wiki).