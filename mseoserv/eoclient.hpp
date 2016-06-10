
/* $Id: eoclient.hpp 490 2015-11-18 10:46:34Z sausage $
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#ifndef EOCLIENT_HPP_INCLUDED
#define EOCLIENT_HPP_INCLUDED

#include "fwd/eoclient.hpp"

#include "fwd/eodata.hpp"
#include "fwd/player.hpp"
#include "eoserver.hpp"
#include "packet.hpp"

#include "socket.hpp"

#include <cstdio>
#include <memory>
#include <queue>
#include <utility>

/**
 * An action the server will execute for the client
 */
struct ActionQueue_Action
{
	PacketReader reader;
	double time;
	bool auto_queue;

	ActionQueue_Action(PacketReader reader_, double time_, bool auto_queue_ = false)
		: reader(reader_)
		, time(time_)
		, auto_queue(auto_queue_)
	{ }
};

/**
 * A list of actions a client needs to eventually have executed for it
 */
class ActionQueue
{
	public:
		std::queue<std::unique_ptr<ActionQueue_Action>> queue;

		double next;
		void AddAction(const PacketReader& reader, double time, bool auto_queue = false);

		ActionQueue() : next(0) {};

		~ActionQueue();
};

/**
 * A connection between an EO Client and EOSERV
 */
class EOClient : public Client
{
	public:
		enum PacketState
		{
			ReadLen1,
			ReadLen2,
			ReadData
		};

		enum ClientState
		{
			Uninitialized,
			Initialized,
			LoggedIn,
			Playing
		};

	private:
		void Initialize();

		FileType upload_type;
		FILE *upload_fh;
		size_t upload_pos;
		size_t upload_size;

		std::string send_buffer2;
		size_t send_buffer2_gpos;
		size_t send_buffer2_ppos;
		size_t send_buffer2_used;

		int seq_start;
		int upcoming_seq_start;
		int seq;

	public:

		EOServer *server() const { return static_cast<EOServer *>(Client::server); };
		int version;
		Player *player;
		unsigned int id;
		bool needpong;
		int hdid;
		ClientState state;
		int login_attempts;

		ActionQueue queue;

		PacketState packet_state;
		unsigned char raw_length[2];
		unsigned int length;
		std::string data;

		PacketProcessor processor;

		EOClient() = delete;

		EOClient(EOServer *server_) : Client(server_)
		{
			this->Initialize();
		}

		EOClient(const Socket &sock, EOServer *server_) : Client(sock, server_)
		{
			this->Initialize();
		}

		bool NeedTick() override;

		void Tick();

		void InitNewSequence();
		void PingNewSequence();
		void PongNewSequence();
		std::pair<unsigned char, unsigned char> GetSeqInitBytes() const;
		std::pair<unsigned short, unsigned char> GetSeqUpdateBytes() const;
		int GenSequence();
		int GenUpcomingSequence();

		void Execute(const std::string &data);

		bool Upload(FileType type, int id, InitReply init_reply);
		bool Upload(FileType type, const std::string &filename, InitReply init_reply);
		void Send(const PacketBuilder &packet);

		~EOClient();
};

#endif // EOCLIENT_HPP_INCLUDED
