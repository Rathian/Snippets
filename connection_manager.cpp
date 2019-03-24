#include "pch.h"
#include "connection_manager.h"


connection_manager::connection_manager()
	: doom_manager(nullptr)
{
	start();
}


connection_manager::~connection_manager()
{
	stop();
}

boost::asio::io_service& connection_manager::context()
{
	return io_service;
}

void connection_manager::start()
{
	worker = std::thread([this]() {
		boost::asio::io_service::work work(io_service);
		io_service.run();
	});

	doom_manager = std::make_unique<doom_connection_manager>(io_service);
}

void connection_manager::stop()
{
	io_service.stop();

	doom_manager.reset();
}