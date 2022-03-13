extern "C" {
//#include <tox/tox.h>
#include <tox.h>
#include <sodium.h>
#include <zed_net.h>
}

#include <stdexcept>
#include <exception>
#include <thread>
#include <cassert>
#include <vector>
#include <optional>
#include <iostream>

inline std::vector<uint8_t> hex2bin(const std::string& str) {
	std::vector<uint8_t> bin{};
	bin.resize(str.size()/2, 0);

	sodium_hex2bin(bin.data(), bin.size(), str.c_str(), str.length(), nullptr, nullptr, nullptr);

	return bin;
}

inline std::string bin2hex(const std::vector<uint8_t>& bin) {
	std::string str{};
	str.resize(bin.size()*2, '?');

	// HECK, std is 1 larger than size returns ('\0')
	sodium_bin2hex(str.data(), str.size()+1, bin.data(), bin.size());

	return str;
}

inline std::string tox_get_own_address(const Tox *tox) {
	std::vector<uint8_t> self_addr{};
	self_addr.resize(TOX_ADDRESS_SIZE);

	tox_self_get_address(tox, self_addr.data());

	return bin2hex(self_addr);
}

// callbacks
inline void log_cb(Tox*, TOX_LOG_LEVEL level, const char *file, uint32_t line, const char *func, const char *message, void *);
inline void self_connection_status_cb(Tox*, TOX_CONNECTION connection_status, void *);
inline void friend_connection_status_cb(Tox *tox, uint32_t friend_number, TOX_CONNECTION connection_status, void *);
inline void friend_request_cb(Tox *tox, const uint8_t *public_key, const uint8_t *message, size_t length, void *);
inline void friend_lossy_packet_cb(Tox *tox, uint32_t friend_number, const uint8_t *data, size_t length, void*);

class TunnelService {
	const bool _in;

	Tox* _tox = nullptr;
	std::optional<uint32_t> _friend_number;

	zed_net_socket_t _socket;
	zed_net_address_t _send_to;

	TunnelService(void) = delete;

	public:
	TunnelService(bool in, uint16_t udp_port) : _in(in) {
		{ // zednet
			zed_net_init();
			if (_in) { // input side, so we open the socket at port
				if (zed_net_udp_socket_open(&_socket, udp_port, 1)) {
					throw std::runtime_error{zed_net_get_error()};
				}
				zed_net_get_address(&_send_to, "127.0.0.1", 0);
				std::cout << "setup for listening on " << udp_port << "\n";
			} else { // output side, so we forward to port
				if (zed_net_udp_socket_open(&_socket, 0, 1)) {
					throw std::runtime_error{zed_net_get_error()};
				}
				zed_net_get_address(&_send_to, "127.0.0.1", udp_port);
				std::cout << "setup for sending to " << udp_port << "\n";
			}
		}

		{ // tox
			TOX_ERR_OPTIONS_NEW err_opt_new;
			Tox_Options* options = tox_options_new(&err_opt_new);
			assert(err_opt_new == TOX_ERR_OPTIONS_NEW::TOX_ERR_OPTIONS_NEW_OK);
			tox_options_set_log_callback(options, log_cb);
#ifndef USE_TEST_NETWORK
			tox_options_set_local_discovery_enabled(options, true);
#endif
			tox_options_set_udp_enabled(options, true);
			tox_options_set_hole_punching_enabled(options, true);
			tox_options_set_tcp_port(options, 0);

			TOX_ERR_NEW err_new;
			_tox = tox_new(options, &err_new);
			tox_options_free(options);
			if (err_new != TOX_ERR_NEW_OK) {
				throw std::runtime_error{"tox_new failed with error code " + std::to_string(err_new)};
			}

			std::cout << "created tox instance with addr:" << tox_get_own_address(_tox) << "\n";

#define CALLBACK_REG(x) tox_callback_##x(_tox, x##_cb)
			CALLBACK_REG(self_connection_status);

			CALLBACK_REG(friend_connection_status);
			CALLBACK_REG(friend_request);

			CALLBACK_REG(friend_lossy_packet);
#undef CALLBACK_REG

#if 0 // enable and fill for bootstrapping and tcp relays
			// dht bootstrap
			{
				struct DHT_node {
					const char *ip;
					uint16_t port;
					const char key_hex[TOX_PUBLIC_KEY_SIZE*2 + 1]; // 1 for null terminator
					unsigned char key_bin[TOX_PUBLIC_KEY_SIZE];
				};

				DHT_node nodes[] =
				{
					// own bootsrap node, to reduce load
					{"tox.plastiras.org",					33445,	"8E8B63299B3D520FB377FE5100E65E3322F7AE5B20A0ACED2981769FC5B43725", {}}, // 14
				};

				for (size_t i = 0; i < sizeof(nodes)/sizeof(DHT_node); i ++) {
					sodium_hex2bin(
						nodes[i].key_bin, sizeof(nodes[i].key_bin),
						nodes[i].key_hex, sizeof(nodes[i].key_hex)-1,
						NULL, NULL, NULL
					);
					tox_bootstrap(_tox, nodes[i].ip, nodes[i].port, nodes[i].key_bin, NULL);
					// TODO: use extra tcp option to avoid error msgs
					// ... this is hardcore
					tox_add_tcp_relay(_tox, nodes[i].ip, nodes[i].port, nodes[i].key_bin, NULL);
				}
			}
#endif

		}
	}

	~TunnelService(void) {
		// tox
		tox_kill(_tox);

		// zednet
		zed_net_socket_close(&_socket);
		zed_net_shutdown();
	}

	bool add_friend(const std::string& addr) {
		auto addr_bin = hex2bin(addr);
		if (addr_bin.size() != TOX_ADDRESS_SIZE) {
			return false;
		}

		Tox_Err_Friend_Add e_fa {TOX_ERR_FRIEND_ADD_NULL};
		tox_friend_add(_tox, addr_bin.data(), reinterpret_cast<const uint8_t*>("nope"), 4, &e_fa);

		return e_fa == TOX_ERR_FRIEND_ADD_OK;
	}

	void friend_online(uint32_t friend_number) {
		_friend_number = friend_number;
	}

	// blocks
	void run(void) {
		while (true) {
			using namespace std::literals;
			std::this_thread::sleep_for(1ms);

			tox_iterate(_tox, this);

			if (!_friend_number) {
				continue;
			}

			// 192-254 for lossy
			const size_t buffer_size = 1024;
			uint8_t buffer[buffer_size + 1] = {200}; // fist byte is tox pkg id
			zed_net_address_t sender;
			int bytes_read = zed_net_udp_socket_receive(&_socket, &sender, buffer + 1, buffer_size);
			if (bytes_read) {
#ifndef NDEBUG
				//printf("Received %d bytes from %s:%d: %s\n", bytes_read, zed_net_host_to_str(sender.host), sender.port, buffer + 1);
				printf("Received %d bytes from %s:%d\n", bytes_read, zed_net_host_to_str(sender.host), sender.port);
#endif
				if (_in) {
					_send_to = sender;
				}

				Tox_Err_Friend_Custom_Packet e_fcp = TOX_ERR_FRIEND_CUSTOM_PACKET_OK;
				tox_friend_send_lossy_packet(_tox, *_friend_number, buffer, bytes_read+1, &e_fcp);
				if (e_fcp != TOX_ERR_FRIEND_CUSTOM_PACKET_OK) {
					std::cerr << "error sending lossy pkg " << e_fcp << "\n";
				}
			}
		}
	}

	void handle_packet(const uint8_t *data, size_t length) {
		if (length < 2) {
			return;
		}

		if (data[0] != 200) {
			return; // invalid channel
			std::cerr << "invalid channel " << (int) data[0] << "\n";
		}

		if (_send_to.port != 0) {
			if (zed_net_udp_socket_send(&_socket, _send_to, data+1, length-1)) {
				std::cerr << "error sending " << zed_net_get_error() << "\n";
			} else {
#ifndef NDEBUG
				std::cout << "sent " << length-1 << "\n";
#endif
			}
		} else {
			std::cerr << "no idea where to\n";
		}
	}
};

inline void log_cb(Tox*, TOX_LOG_LEVEL level, const char *file, uint32_t line, const char *func, const char *message, void *) {
	std::cerr << "TOX " << level << " " << file << ":" << line << "(" << func << ") " << message << "\n";
}

inline void self_connection_status_cb(Tox*, TOX_CONNECTION connection_status, void *) {
	std::cout << "self_connection_status_cb " << connection_status << "\n";
}

// friend
inline void friend_connection_status_cb(Tox *tox, uint32_t friend_number, TOX_CONNECTION connection_status, void* user_data) {
	std::cout << "friend_connection_status_cb " << connection_status << "\n";
	if (connection_status != TOX_CONNECTION_NONE) {
		static_cast<TunnelService*>(user_data)->friend_online(friend_number);
	}
}

inline void friend_request_cb(Tox *tox, const uint8_t *public_key, const uint8_t *message, size_t length, void *) {
	std::cout << "friend_request_cb\n";

	Tox_Err_Friend_Add e_fa = TOX_ERR_FRIEND_ADD::TOX_ERR_FRIEND_ADD_OK;
	tox_friend_add_norequest(tox, public_key, &e_fa);
}

// custom packets
inline void friend_lossy_packet_cb(Tox *tox, uint32_t friend_number, const uint8_t *data, size_t length, void* user_data) {
#ifndef NDEBUG
	std::cout << "friend_lossy_packet_cb " << length << "\n";
#endif
	static_cast<TunnelService*>(user_data)->handle_packet(data, length);
}

// command line :
// in|out
// udp_port
// [friend_tox_addr]
int main(int argc, char** argv) {
	if (argc < 3) {
		std::cerr << "not enough params, usage:\n$ " << argv[0] << " <in|out> <udp_port> [friend_tox_addr]\n";
		return -1;
	}

	// type
	bool in = true;
	std::string_view type_sv{argv[1]};
	if (type_sv == "in") {
		in = true;
	} else if (type_sv == "out") {
		in = false;
	} else {
		std::cerr << "error: invalid type " << type_sv << ", must be either in or out.\n";
		return -1;
	}
	std::cout << "set type to " << type_sv << "\n";

	// port
	uint16_t port = atoi(argv[2]);
	if (port < 1024) {
		std::cerr << "error: port must be at least 1024\n";
		return -1;
	}

	std::string_view friend_sv{};
	if (argc == 4) { // friend?
		friend_sv = argv[3];
	}

	TunnelService ts{in, port};

	if (!friend_sv.empty()) {
		if (!ts.add_friend(std::string(friend_sv))) {
			std::cerr << "error adding friend!\n";
			return -1;
		}
	}

	ts.run();

	return 0;
}

