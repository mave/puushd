class PuushServer;

#ifndef _PUUSHSERVER_H
#define _PUUSHSERVER_H

#include <random>
#include "Configuration.h"
#include "PuushDatabase.h"

#ifndef WIN32
#define uniform_int_distribution uniform_int
namespace std { typedef minstd_rand0 default_random_engine; }
#endif

class PuushServer
{
public:
	PuushServer(Configuration *config, PuushDatabase *database);
	~PuushServer();

	void start(int port);
	void stop();

private:
	static void *forwardMongooseEvent(mg_event ev, mg_connection *conn);
	void *handleMongooseEvent(mg_event ev, mg_connection *conn);

	bool tryServeFile(mg_connection *conn, const mg_request_info *info, const std::string &request_uri);
	void handleRequest(mg_connection *conn, const mg_request_info *info);
	void handleAuthRequest(mg_connection *conn, const mg_request_info *info);
	void handleFileUpload(mg_connection *conn, const mg_request_info *info);

	static std::string generateRandomFilename(void *userdata);

	mg_context *m_mongooseContext;
	
	std::random_device m_randomDevice;
	std::default_random_engine m_randomGenerator;
#if __GNUC__ >= 4 && __GNUC_MINOR__ < 5
	std::uniform_int<int> m_randomDistribution;
#elif __GNUC__ >= 4 && __GNUC_MINOR__ >= 5
	std::uniform_int_distribution<int> m_randomDistribution;
#endif

	Configuration *m_config;
	PuushDatabase *m_database;
	
	std::string m_puushUrl;
};

#endif
