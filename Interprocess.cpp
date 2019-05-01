// Interprocess.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/interprocess/streams/bufferstream.hpp>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

namespace bip = boost::interprocess;

int main()
{
	bip::shared_memory_object smo(bip::open_or_create, "MySharedMemory", bip::read_write);

	std::string str = "test data";
	smo.truncate(39); // 10 KiB
	bip::mapped_region r(smo, bip::read_write);

	auto mem_ptr = static_cast<char*>(r.get_address());
	auto mem_size = r.get_size();
	bip::interprocess_mutex* mutex = new (mem_ptr) bip::interprocess_mutex();
	mem_ptr += sizeof(bip::interprocess_mutex);
	mem_size -= sizeof(bip::interprocess_mutex);
	bip::interprocess_condition* cond = new (mem_ptr) bip::interprocess_condition();
	mem_ptr += sizeof(bip::interprocess_condition);
	mem_size -= sizeof(bip::interprocess_condition);

	bip::bufferstream stream(mem_ptr, mem_size);

	std::thread t([]() {
		bip::shared_memory_object smo(bip::open_only, "MySharedMemory", bip::read_write);
		bip::mapped_region r(smo, bip::read_write);
		auto mem_ptr = static_cast<char*>(r.get_address());
		auto mem_size = r.get_size();
		bip::interprocess_mutex* mutex = reinterpret_cast<bip::interprocess_mutex*>(mem_ptr);
		mem_ptr += sizeof(bip::interprocess_mutex);
		mem_size -= sizeof(bip::interprocess_mutex);
		bip::interprocess_condition* cond = reinterpret_cast<bip::interprocess_condition*>(mem_ptr);
		mem_ptr += sizeof(bip::interprocess_condition);
		mem_size -= sizeof(bip::interprocess_condition);

		bip::bufferstream stream(mem_ptr, mem_size);

		std::vector<char> out(100);

		for (int i = 0; i < 100; i++) {
			memset(out.data(), 0, 100);
			{
				bip::scoped_lock<bip::interprocess_mutex> lock(*mutex);
				cond->wait(lock);
				auto read = stream.readsome(out.data(), 11);
				std::cout << "Read: " << read << " -- ";
			}
			std::cout << out.data() << '\n';
		}
	});

	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	for (int i = 0; i < 100; i++) {
		{
			bip::scoped_lock<bip::interprocess_mutex> lock(*mutex);
			stream.write(str.c_str(), 10);
		}
		cond->notify_all();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	if (t.joinable())
		t.join();

	bip::shared_memory_object::remove("MySharedMemory");

    std::cout << "Hello World!\n"; 
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
