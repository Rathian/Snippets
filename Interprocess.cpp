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
#include <mutex>
#include <memory>
#include <condition_variable>
#include <algorithm>

namespace bip = boost::interprocess;


class block_buffer : public std::enable_shared_from_this<block_buffer>
{
public:
	block_buffer(std::size_t size)
		: _memory(static_cast<char*>(malloc(size)))
		, _capacity(size)
		, _size(0)
		, _next(nullptr)
		, _lock()
	{ }

	~block_buffer() {
		free(_memory);
	}

	std::size_t size() {
		if (_size < _capacity)
			return _size;
		else if (_next)
			return _capacity + _next->size();
		else
			return _size;
	}

	std::shared_ptr<block_buffer> read(char* buffer, std::size_t size, std::size_t& offset) {
		std::unique_lock<std::mutex> lock(_lock);

		// read goes fully through in this item
		if (offset + size <= _capacity) {
			// wait for missing bytes to be written if missing
			while (offset + size > _size)
				_cond.wait(lock);
			std::memcpy(buffer, _memory + offset, size);
			offset += size;
			if (offset == _capacity) {
				offset = 0;
				if (!_next)
					_push();
				return _next;
			}
			else {
				return shared_from_this();
			}
		}

		// offset too big, musst read in next item
		else if (offset >= _capacity) {
			// wait for a next item to be added if mssing
			while (!_next)
				_cond.wait(lock);
			// read in next item
			offset -= _capacity;
			lock.unlock();
			return _next->read(buffer, size, offset);
		}

		// partial read in this item and the next
		else {
			std::size_t partial_size = _capacity - offset;
			// wait for missing bytes to be written if missing
			while (_capacity > _size)
				_cond.wait(lock);
			std::memcpy(buffer, _memory + offset, partial_size);
			size -= partial_size;
			offset = 0;
			buffer += partial_size;
			// wait for a next item to be added if mssing
			while (!_next)
				_cond.wait(lock);
			lock.unlock();
			return _next->read(buffer, size, offset);
		}
	}

	std::shared_ptr<block_buffer> write(const char* buffer, std::size_t size, std::size_t& offset) {
		std::unique_lock<std::mutex> lock(_lock);

		// write goes fully through in this item
		if (offset + size <= _capacity) {
			std::memcpy(_memory + offset, buffer, size);
			_size = std::max(offset + size, _size);
			offset += size;
			_cond.notify_all();
			return shared_from_this();
		}

		// offset too big, musst write in next item
		else if (offset >= _capacity) {
			// add a new item if the is no next one
			if (!_next)
				_push();
			offset -= _capacity;
			lock.unlock();
			std::shared_ptr<block_buffer> head = _next->write(buffer, size, offset);
			_cond.notify_all();
			return head;
		}

		// partial write in this item and the next
		else {
			std::size_t partial_size = _capacity - offset;
			std::memcpy(_memory + offset, buffer, partial_size);
			_size = _capacity;
			// add a new item if the is no next one
			if (!_next)
				_push();
			lock.unlock();
			size -= partial_size;
			offset = 0;
			buffer += partial_size;

			_cond.notify_all();
			return _next->write(buffer, size, offset);
		}
	}
private:
	char* _memory;
	std::size_t _capacity;
	std::size_t _size;
	std::shared_ptr<block_buffer> _next;
	std::mutex _lock;
	std::condition_variable _cond;

	void _push() {
		if (!_next) {
			_next = std::make_shared<block_buffer>(_capacity);
		}
		else {
			std::shared_ptr<block_buffer>* head = &_next;
			while (*head)
				head = &(*head)->_next;
			*head = std::make_shared<block_buffer>(_capacity);
		}
	}
};

class sync_streambuf : public std::basic_streambuf<char, std::char_traits<char>> {
public:
	sync_streambuf(const std::shared_ptr<block_buffer>& block)
		: _block(block)
		, _offset(0)
	{ }

	sync_streambuf(std::shared_ptr<block_buffer>&& block)
		: _block(std::move(block))
		, _offset(0)
	{ }

protected:
	int_type underflow() override final
	{
		char current = 0;
		std::size_t offset = _offset;
		_block->read(&current, 1, offset).reset();
		return traits_type::to_int_type(current);
	}

	int_type uflow() override final
	{
		char current = 0;
		_block = _block->read(&current, 1, _offset);
		return traits_type::to_int_type(current);
	}

	int_type pbackfail(int_type ch) override final
	{
		if (_offset == 0) {
			return traits_type::eof();
		}
		else if (ch != traits_type::eof()) {
			_offset--;
			return ch;
		}
		else {
			char current = 0;
			std::size_t offset = --_offset;
			_block = _block->read(&current, 1, offset);
			return traits_type::to_int_type(current);
		}
	}

	std::streamsize showmanyc() override final
	{
		return _block->size() - _offset;
	}

	sync_streambuf::int_type overflow(int_type ch) override final
	{
		char current = traits_type::to_char_type(ch);
		_block = _block->write(&current, 1, _offset);
		return traits_type::to_int_type(current);
	}

	std::streamsize xsgetn(char* s, std::streamsize n) override final
	{
		_block = _block->read(s, static_cast<std::size_t>(n), _offset);
		return n;
	}

	std::streamsize xsputn(const char* s, std::streamsize n) override final
	{
		_block = _block->write(s, static_cast<std::size_t>(n), _offset);
		return n;
	}
private:
	std::shared_ptr<block_buffer> _block;
	size_t _offset;
};

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
