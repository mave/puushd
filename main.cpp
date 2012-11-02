#include "stdafx.h"
#include "PuushServer.h"
#include "PuushDatabase.h"
#include "Configuration.h"
#include <fstream>

bool writeDefaultConfig()
{
	std::ofstream os;
	os.open("puushd.conf", std::ios_base::out);
	if (os.fail())
	{
		return false;
	}

	os << "# Default configuration generated by puushd" << std::endl;
	os << std::endl;
	os << "dbfile = puushd.db" << std::endl;
	os << "port = 1200" << std::endl;
	os << "puushurl = http://localhost:1200/" << std::endl;

	os.flush();
	os.close();

	return true;
}

int main(int argc, char* argv[])
{	
	Configuration config;
	if (!config.load("puushd.conf"))
	{
		std::cerr << "Unable to load puushd.conf" << std::endl;
		if (!writeDefaultConfig())
		{
			std::cerr << "Unable to write default configuration to puushd.conf" << std::endl;
			return 1;
		}
		else
		{
			std::cerr << "Default configuration written to puushd.conf" << std::endl;

			if (!config.load("puushd.conf"))
			{
				std::cerr << "Unable to load newly written configuration" << std::endl;
				return 1;
			}
		}
	}

	std::cout << std::endl;
	
	std::string databaseFile = config.getString("dbfile", "puushd.db");
	
	PuushDatabase db;
	if (!db.load(databaseFile.c_str()))
	{
		std::cerr << "Unable to load database (" << databaseFile << ")" << std::endl;
		return 1;
	}
	
#ifdef WIN32
	if (argc >= 4 && _stricmp(argv[1], "adduser") == 0)
#else
	if (argc >= 4 && strcasecmp(argv[1], "adduser") == 0)
#endif
	{
		std::cout << "Adding user '" << argv[2] << "'/'" << argv[3] << "'..." << std::endl;
		std::string apiKey = db.addUser(argv[2], argv[3]);
		std::cout << "Done. API key: " << apiKey << std::endl;
		return 1;
	}
	
	std::cout << "Loaded database from " << databaseFile << std::endl;
	
	int port = config.getInteger("port", 1200);
	
	std::cout << std::endl << "Starting server on port " << port << "..." << std::endl;
	
	PuushServer server(&config, &db);
	server.start(port);

	getchar();

	std::cout << std::endl << "Stopping server..." << std::endl;
	
	server.stop();

	return 0;
}
