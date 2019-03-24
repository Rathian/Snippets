#pragma once
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <tuple>
#include <thread>
#include <iostream>

struct doom_header
{
	uint32_t packet_size;
	uint32_t packet_id;
	uint32_t protocol;
	uint32_t type;
};

class tcp_connection
{
public:
	/// Constructor.
	tcp_connection(boost::asio::io_service& io_service,
		const boost::asio::ip::address& address,
		unsigned short port)
		: socket_(io_service)
	{
		//boost::asio::ip::tcp::endpoint endpoint(address, port);
		//// Start an asynchronous connect operation.
		//boost::asio::async_connect(socket_, endpoint,
		//	boost::bind(&tcp_connection::handle_connect, this,
		//		boost::asio::placeholders::error));
	}

	/// Get the underlying socket. Used for making a connection or for accepting
	/// an incoming connection.
	boost::asio::ip::tcp::socket& socket()
	{
		return socket_;
	}

	///// Handle completion of a accept operation.
	//void handle_accept(const boost::system::error_code& e, connection_ptr conn)
	//{
	//	if (!e)
	//	{
	//		// Successfully accepted a new connection. Send the list of stocks to the
	//		// client. The connection::async_write() function will automatically
	//		// serialize the data structure for us.
	//		conn->async_write(stocks_,
	//			boost::bind(&server::handle_write, this,
	//				boost::asio::placeholders::error, conn));
	//	}

	//	// Start an accept operation for a new connection.
	//	connection_ptr new_conn(new connection(acceptor_.get_io_service()));
	//	acceptor_.async_accept(new_conn->socket(),
	//		boost::bind(&tcp_connection::handle_accept, this,
	//			boost::asio::placeholders::error, new_conn));
	//}


	//void handle_connect(const boost::system::error_code& e)
	//{
	//	if (!e)
	//	{
	//	}
	//	else
	//	{
	//	}
	//}

	/// Asynchronously write a data structure to the socket.
	template <typename T, typename Handler>
	void async_write(const T& t, Handler handler)
	{
		// Serialize the data first so we know how large it is.
		std::ostringstream archive_stream;
		boost::archive::binary_oarchive archive(archive_stream);
		archive << t;
		const std::string& str = archive_stream.str();
		buffer_out_.clear();
		std::copy(str.begin(), str.end(), std::back_inserter(buffer_out_));

		// Format the header.
		doom_header& header = header_out_;
		header.packet_size = outbound_data_.size();
		header.packet_id = 0;
		header.protocol = 0;
		header.type = 0;

		// Write the serialized data to the socket. We use "gather-write" to send
		// both the header and the data in a single write operation.
		std::vector<boost::asio::const_buffer> buffers;
		buffers.push_back(boost::asio::buffer(&header_out_, sizeof(doom_header)));
		buffers.push_back(boost::asio::buffer(outbound_data_));
		boost::asio::async_write(socket_, buffers, handler);
	}

	/// Asynchronously read a data structure from the socket.
	template <typename T, typename Handler>
	void async_read(T& t, Handler handler)
	{
		// Issue a read operation to read exactly the number of bytes in a header.
		void (connection::*f)(
			const boost::system::error_code&,
			T&, std::tuple<Handler>)
			= &tcp_connection::handle_read_header<T, Handler>;
		boost::asio::async_read(socket_, boost::asio::buffer(&header_in_, sizeof(doom_header)),
			boost::bind(f,
				this, boost::asio::placeholders::error, boost::ref(t),
				boost::make_tuple(handler)));
	}

	/// Handle a completed read of a message header. The handler is passed using
	/// a tuple since boost::bind seems to have trouble binding a function object
	/// created using boost::bind as a parameter.
	template <typename T, typename Handler>
	void handle_read_header(const boost::system::error_code& e,
		T& t, std::tuple<Handler> handler)
	{
		if (e)
		{
			std::get<0>(handler)(e);
		}
		else
		{
			const doom_header& header = header_in_;
			// Start an asynchronous call to receive the data.
			buffer_in_.resize(header.packet_size);
			void (connection::*f)(
				const boost::system::error_code&,
				T&, std::tuple<Handler>)
				= &tcp_connection::handle_read_data<T, Handler>;
			boost::asio::async_read(socket_, boost::asio::buffer(buffer_in_),
				boost::bind(f, this,
					boost::asio::placeholders::error, boost::ref(t), handler));
		}
	}

	/// Handle a completed read of message data.
	template <typename T, typename Handler>
	void handle_read_data(const boost::system::error_code& e,
		T& t, std::tuple<Handler> handler)
	{
		if (e)
		{
			std::get<0>(handler)(e);
		}
		else
		{
			// Extract the data structure from the data just received.
			try
			{
				std::string archive_data(&buffer_in_[0], buffer_in_.size());
				std::istringstream archive_stream(archive_data);
				boost::archive::binary_iarchive archive(archive_stream);
				archive >> t;
			}
			catch (std::exception& e)
			{
				// Unable to decode data.
				boost::system::error_code error(boost::asio::error::invalid_argument);
				boost::get<0>(handler)(error);
				return;
			}

			// Inform caller that data has been received ok.
			std::get<0>(handler)(e);
		}
	}

private:
	/// The underlying socket.
	boost::asio::ip::tcp::socket socket_;

	doom_header header_in_;
	doom_header header_out_;
	std::vector<char> buffer_in_;
	std::vector<char> buffer_out_;
};

class udp_connection
{
public:
	/// Constructor.
	udp_connection(boost::asio::io_service& io_service, 
		const boost::asio::ip::address& address, 
		unsigned short port,
		unsigned int buffer_size)
		: endpoint_in_()
		, endpoint_out_(address, port)
		, socket_in_(io_service)
		, socket_out_(io_service, endpoint_out_.protocol())
	{
		boost::asio::ip::udp::endpoint listen_endpoint(
			boost::asio::ip::address::from_string("0.0.0.0"), port);
		socket_in_.open(listen_endpoint.protocol());
		socket_in_.set_option(boost::asio::ip::udp::socket::reuse_address(true));
		socket_in_.bind(listen_endpoint);

		// Join the multicast group.
		socket_in_.set_option(boost::asio::ip::multicast::join_group(address));

		buffer_in_.resize(buffer_size);
		buffer_out_.resize(buffer_size);
	}

	/// Get the underlying socket. Used for making a connection or for accepting
	/// an incoming connection.
	//boost::asio::ip::udp::socket& socket()
	//{
	//	return socket_;
	//}

	/// Asynchronously write a data structure to the socket.
	template <typename T, typename Handler>
	void async_write(const T& t, Handler handler)
	{
		// Serialize the data first so we know how large it is.
		std::ostringstream archive_stream;
		boost::archive::binary_oarchive archive(archive_stream);
		archive << t;
		std::string outbound_data_ = archive_stream.str();

		// Format the header.
		doom_header& header = *(doom_header*)&buffer_out_[0];
		header.packet_size = outbound_data_.size();
		header.packet_id = 0;
		header.protocol = 0;
		header.type = 0;
		size_t datagram_size = sizeof(doom_header) + outbound_data_.size();
		if (datagram_size > buffer_out_.size())
		{
			boost::system::error_code error(boost::asio::error::message_size);
			handler(error);
		}
		std::memcpy(&buffer_out_[sizeof(doom_header)], outbound_data_.c_str(), outbound_data_.size());

		// Write the serialized data to the socket. We use "gather-write" to send
		// both the header and the data in a single write operation.
		void (udp_connection::*f)(
			const boost::system::error_code&,
			std::tuple<Handler>)
			= &udp_connection::handle_write<Handler>;
		socket_out_.async_send_to(boost::asio::buffer(buffer_out_.data(), datagram_size), endpoint_out_, handler);
	}

	template <typename Handler>
	void handle_write(const boost::system::error_code& e,
		std::tuple<Handler> handler)
	{
		// Issue a read operation to read exactly the number of bytes in a header.
		std::get<0>(handler)(e);
	}

	/// Asynchronously read a data structure from the socket.
	template <typename T, typename Handler>
	void async_read(T& t, Handler handler)
	{
		// Issue a read operation to read exactly the number of bytes in a header.
		void (udp_connection::*f)(
			const boost::system::error_code&, size_t,
			T&, std::tuple<Handler>)
			= &udp_connection::handle_read<T, Handler>;
		socket_in_.async_receive_from(boost::asio::buffer(buffer_in_), endpoint_in_,
			boost::bind(f,
				this, boost::asio::placeholders::error, 
				boost::asio::placeholders::bytes_transferred, 
				std::ref(t),
				std::make_tuple(handler)));
	}

	template <typename T, typename Handler>
	void handle_read(const boost::system::error_code& e, size_t bytes_recvd,
		T& t, std::tuple<Handler> handler)
	{
		if (e)
		{
			std::get<0>(handler)(e);
		}
		else
		{
			const doom_header& header = *(doom_header*)&buffer_in_[0];

			try
			{
				std::string archive_data(&buffer_in_[sizeof(doom_header)], header.packet_size);
				std::istringstream archive_stream(archive_data);
				boost::archive::binary_iarchive archive(archive_stream);
				archive >> t;
			}
			catch (std::exception& /*e*/)
			{
				// Unable to decode data.
				boost::system::error_code error(boost::asio::error::invalid_argument);
				std::get<0>(handler)(error);
				return;
			}

			// Inform caller that data has been received ok.
			std::get<0>(handler)(e);
		}
	}
private:
	boost::asio::ip::udp::endpoint endpoint_in_;
	boost::asio::ip::udp::endpoint endpoint_out_;
	/// The underlying socket.
	boost::asio::ip::udp::socket socket_in_;
	boost::asio::ip::udp::socket socket_out_;

	std::vector<char> buffer_in_;
	std::vector<char> buffer_out_;
};

class doom_connection_manager
{
public:
	doom_connection_manager(boost::asio::io_service& io_service)
		: connection_(io_service, boost::asio::ip::address::from_string("235.255.0.1"), 666, 1024)
	{
		connection_.async_read(text,
			boost::bind(&doom_connection_manager::handle_receive_from, this,
				boost::asio::placeholders::error));
	}

	void handle_receive_from(const boost::system::error_code& error)//, size_t bytes_recvd)
	{
		if (!error)
		{
			std::cout << text;

			connection_.async_read(text,
				boost::bind(&doom_connection_manager::handle_receive_from, this,
					boost::asio::placeholders::error));
		}
		else
		{
			std::cerr << "Doom receive error: " << error.message() << '\n';
		}
	}

	void handle_send_to(const boost::system::error_code& error)
	{
		//if (!error && message_count_ < max_message_count)
		//{
		//	timer_.expires_from_now(boost::posix_time::seconds(1));
		//	timer_.async_wait(
		//		boost::bind(&sender::handle_timeout, this,
		//			boost::asio::placeholders::error));
		//}
		if (error)
		{
			std::cerr << "Doom send error: " << error.message() << '\n';
		}
	}

	void send(const std::string& message)
	{
		connection_.async_write(message,
			boost::bind(&doom_connection_manager::handle_send_to, this,
				boost::asio::placeholders::error));
	}

private:
	udp_connection connection_;
	std::string text;
};

class connection_manager
{
public:
	connection_manager();
	~connection_manager();

	void start();
	void stop();
	boost::asio::io_service& context();

	void send(const std::string& message)
	{
		if (doom_manager)
		{
			doom_manager->send(message);
		}
	}
private:
	boost::asio::io_service io_service;
	std::thread worker;
	std::unique_ptr<doom_connection_manager> doom_manager;
};

