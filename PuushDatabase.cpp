#include "stdafx.h"
#include "PuushDatabase.h"
#include "md5.h"
#include <iomanip>

PuushDatabase::PuushDatabase()
	: m_database(NULL), m_randomDistribution(0, (10 + 26 + 26) - 1), m_randomGenerator(m_randomDevice())
{
}

PuushDatabase::~PuushDatabase()
{
	close();
}

bool PuushDatabase::load(const char *filename)
{
	if (sqlite3_open(filename, &m_database) != 0)
	{
		return false;
	}

	printf("init users '%s'\n", execute("CREATE TABLE IF NOT EXISTS users ('id' INTEGER PRIMARY KEY AUTOINCREMENT, 'username' VARCHAR(64), 'password' VARCHAR(64), 'apikey' VARCHAR(20))", NULL).c_str());
	printf("init files '%s'\n", execute("CREATE TABLE IF NOT EXISTS files ('id' INTEGER PRIMARY KEY AUTOINCREMENT, 'filename' VARCHAR(256), 'path' VARCHAR(256), 'short' VARCHAR(16), 'md5hash' VARCHAR(32), 'time' DATETIME, 'user_id' INTEGER)", NULL).c_str());

	return true;
}

void PuushDatabase::close()
{
	if (m_database != NULL)
	{
		sqlite3_close(m_database);
	}
}

std::string PuushDatabase::addUser(const char *username, const char *password)
{
	std::string apiKey = generateApiKey();
	
	char *query = sqlite3_mprintf("INSERT INTO `users` (`username`, `password`, `apikey`) VALUES ('%q', '%q', '%q')", username, password, apiKey.c_str());
	std::string err = execute(query, NULL);
	sqlite3_free(query);
	if (!err.empty())
	{
		std::cerr << "Database Error addUser: " << err << std::endl;
		return "";
	}

	return apiKey;
}

std::string PuushDatabase::authenticateUser(const char *username, const char *passwordOrApiKey)
{
	char *query = sqlite3_mprintf("SELECT `apikey` FROM `users` WHERE `username` = '%q' AND (`password` = '%q' OR `apikey` = '%q') LIMIT 1", username, passwordOrApiKey, passwordOrApiKey);
	QueryResult result;
	std::string err = execute(query, &result);
	sqlite3_free(query);
	if (!err.empty())
	{
		std::cerr << "Database Error authenticateUser: " << err << std::endl;
		return "";
	}

	if (result.rows.size() == 0)
	{
		return "";
	}

	QueryField &field = result.rows.front().at(0);
	std::cerr << "[debug] authenticated user " << username << " with api key \"" << field.value << "\"" << std::endl;

	return field.value;
}

std::string PuushDatabase::addFile(const char *apiKey, const char *filename, const char *path, const char *md5hash)
{
	char *query = sqlite3_mprintf("SELECT `id` FROM `users` WHERE `apikey` = '%q'", apiKey);
	QueryResult result;
	std::string err = execute(query, &result);
	sqlite3_free(query);
	if (!err.empty())
	{
		std::cerr << "Database Error addFile: " << err << std::endl;
		return "";
	}

	if (result.rows.size() == 0)
	{
		// user with given api key not found
		std::cerr << "Unauthorized upload attempt, api key = \"" << apiKey << "\"" << std::endl;
		return "";
	}

	QueryField &field = result.rows.front().at(0);
	if (field.isNull)
		return "";

	int userId = atoi(field.value.c_str());

	FILE *f = fopen(path, "rb");
	if (f == NULL)
	{
		std::cerr << "Unable to open " << path << " for reading" << std::endl;
		return "";
	}

	char buf[2048];
	MD5_CTX ctx;
	MD5_Init(&ctx);
	while (!feof(f))
	{
		size_t num = fread(buf, 1, sizeof(buf), f);
		MD5_Update(&ctx, buf, num);
	}
	unsigned char res[16];
	MD5_Final(res, &ctx);

	std::stringstream ss;
	for (int i = 0; i < sizeof(res); ++i)
	{
		ss << std::setfill('0') << std::setw(2) << std::hex << (int) res[i];
	}

	if (stricmp(md5hash, ss.str().c_str()) != 0)
	{
		// md5 doesnt match
		std::cerr << "MD5 didn't match for file upload" << std::endl;
		return "";
	}

	// search for a short name that is not already used
	bool exists;
	std::string shortName;
	do
	{
		shortName = generateShortName();
		printf("generated %s\n", shortName.c_str());

		query = sqlite3_mprintf("SELECT `id` FROM `files` WHERE `short` = '%q' LIMIT 1", shortName.c_str());
		err = execute(query, &result);
		sqlite3_free(query);
		if (!err.empty())
		{
			std::cerr << "Database Error addFile: " << err << std::endl;
			return "";
		}

		exists = result.rows.size() != 0;
	}
	while (exists);

	query = sqlite3_mprintf("INSERT INTO `files` (`filename`, `path`, `short`, `md5hash`, `time`, `user_id`) VALUES ('%q', '%q', '%q', '%q', DATETIME(), %d)", filename, path, shortName.c_str(), ss.str().c_str(), userId);
	err = execute(query, NULL);
	sqlite3_free(query);
	if (!err.empty())
	{
		std::cerr << "Database Error addFile: " << err << std::endl;
		return "";
	}

	return shortName;
}

std::string PuushDatabase::lookupFile(const char *shortName, int &httpStatus)
{
	char *query = sqlite3_mprintf("SELECT `path` FROM `files` WHERE `short` = '%q' LIMIT 1", shortName);
	QueryResult result;
	std::string err = execute(query, &result);
	sqlite3_free(query);
	if (!err.empty())
	{
		std::cerr << "Database Error lookupFile: " << err << std::endl;
		httpStatus = 500;
		return "";
	}

	if (result.rows.size() == 0)
	{
		httpStatus = 404;
		return "";
	}

	QueryField &field = result.rows.front().at(0);
	httpStatus = 200;
	return field.value;
}

std::string PuushDatabase::execute(const char *query, PuushDatabase::QueryResult *destResult)
{
	if (m_database == NULL)
		return "database not initialized";

	if (destResult != NULL)
	{
		destResult->rows.clear();
		destResult->columns.clear();
	}

	char *errmsg = NULL;
	std::string result;
	if (sqlite3_exec(m_database, query, &PuushDatabase::queryCallback, destResult, &errmsg) != SQLITE_OK)
	{
		result = errmsg;
		sqlite3_free(errmsg);
	}

	return result;
}

int PuushDatabase::queryCallback(void *userdata, int argc, char **argv, char **columns)
{
	if (userdata != NULL)
	{
		QueryResult *destResult = (QueryResult *) userdata;

		bool doColumns = destResult->columns.size() == 0;

		std::vector<QueryField> fields;
		for (int i = 0; i < argc; ++i)
		{
			char *columnName = columns[i];
			if (doColumns)
				destResult->columns.push_back(columnName);

			QueryField field;
			field.isNull = argv[i] == NULL;
			if (argv[i] != NULL)
				field.value = argv[i];
			fields.push_back(field);
		}

		destResult->rows.push_back(fields);
	}
	return 0;
}

std::string PuushDatabase::generateApiKey()
{
	static const char alphanum[] =
		"0123456789"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz";

	std::string str;
	str.resize(20);
	for (int i = 0; i < 20; ++i)
	{
		str[i] = alphanum[m_randomDistribution(m_randomGenerator)];
	}

	return str;
}

std::string PuushDatabase::generateShortName()
{
	static const char alphanum[] =
		"0123456789"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz";

	std::string str;
	str.resize(4);
	for (int i = 0; i < 4; ++i)
	{
		str[i] = alphanum[m_randomDistribution(m_randomGenerator)];
	}

	return str;
}
