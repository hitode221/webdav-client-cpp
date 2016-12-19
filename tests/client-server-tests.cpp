#include <webdav/client-server.hpp>
#include "stdafx.h"
#include <map>
#include <unistd.h>  
#include "catch.hpp"

SCENARIO("Hash", "[getHash]"){
	getHash("testfile.txt");
	getHash("same_testfile.txt");
	REQUIRE(hash_to_string("testfile.txt.hash") == hash_to_string("same_testfile.txt.hash"));
	REQUIRE(!has_file_changed("testfile.txt"));

}

SCENARIO("crypt", "[crypt]"){
	encrypt("testfile.txt", "en_testfile.txt");
	decrypt("en_testfile.txt", "de_testfile.txt");
  	std::ifstream expected("testfile.txt");
	std::ifstream output("de_testfile.txt");
	std::string first, second;
	
	bool flag = true;

	while (expected || output) {
		std::getline(expected, first);
		std::getline(output, second);
		if (first != second) {
			flag = false;
			break;
		}
	}
  REQUIRE(flag);
}

SCENARIO("upload", "[upload]"){
	std::unique_ptr<WebDAV::Client> client(WebDAV::Client::Init(init_client("config.txt")));
	upload_to_disk_root("upload", client);
	REQUIRE(client->check("1.txt"));
	REQUIRE(client->check("2.txt"));
	REQUIRE(client->check("dir/"));
	REQUIRE(client->check("dir/3.txt"));
	REQUIRE(client->check("dir/dir_in/"));
	REQUIRE(client->check("dir/dir_in/4.txt"));
}

SCENARIO("download", "[download]"){
	std::unique_ptr<WebDAV::Client> client(WebDAV::Client::Init(init_client("config.txt")));
	download_from_disk_root("download", client);
	REQUIRE(!client->check("1.txt"));
	REQUIRE(!client->check("2.txt"));
	REQUIRE(!client->check("dir/"));
	REQUIRE(!client->check("dir/3.txt"));
	REQUIRE(!client->check("dir/dir_in/"));
	REQUIRE(!client->check("dir/dir_in/4.txt"));
}
