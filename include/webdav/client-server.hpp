#pragma once

#include <webdav/client.hpp>
#include <memory>
#include <sstream>
#include <map>
#include <string>
#include <boost/asio/io_service.hpp>
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <openssl/evp.h>
#include <openssl/sha.h>
#pragma comment(lib, "wldap32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "ssleay32.lib")
#pragma comment(lib, "libeay32.lib")


#define BUFSIZE 1024

std::string getHash(std::string name)
{
	int inlen;
	FILE * input;
	fopen_s(&input, name.c_str(), "rb");
	SHA256_CTX handler;
	unsigned char inbuf[BUFSIZE];
	unsigned char digest[SHA256_DIGEST_LENGTH];
	SHA256_Init(&handler);
	for (;;) {
		inlen = fread(inbuf, 1, BUFSIZE, input);
		if (inlen <= 0) break;
		SHA256_Update(&handler, inbuf, inlen);
	}
	SHA256_Final(digest, &handler);
	auto file_hash = name + ".hash";
	FILE * out;
	fopen_s(&out, file_hash.c_str(), "wb");
	fwrite(digest, 1, SHA256_DIGEST_LENGTH, out);
	fclose(out);
	fclose(input);
	return file_hash;
}

std::string hash_to_string(std::string hash_file)
{
	FILE * input;
	fopen_s(&input, hash_file.c_str(), "rb");
	char buffer[32];
	fread(buffer, sizeof(char), 32, input);
	std::stringstream ss;
	ss << buffer;
	fclose(input);
	return ss.str().substr(0, 32);
}

void encrypt(std::string name, std::string new_name) {
	int outlen, inlen;
	FILE * input, *output;
	fopen_s(&input, name.c_str(), "rb");
	fopen_s(&output, new_name.c_str(), "wb");
	unsigned char inbuf[BUFSIZE], outbuf[BUFSIZE];
	unsigned char key[32] = "1234567890098765432112345678900";
	unsigned char iv[8] = "1234567";
	EVP_CIPHER_CTX ctx;
	EVP_CIPHER_CTX_init(&ctx);
	EVP_EncryptInit(&ctx, EVP_aes_256_ofb(), key, iv);
	for (;;) {
		inlen = fread(inbuf, 1, BUFSIZE, input);
		if (inlen <= 0)
			break;
		EVP_EncryptUpdate(&ctx, outbuf, &outlen, inbuf, inlen);
		fwrite(outbuf, 1, outlen, output);
	}
	EVP_EncryptFinal(&ctx, outbuf, &outlen);
	fwrite(outbuf, 1, outlen, output);
	EVP_CIPHER_CTX_cleanup(&ctx);
	fclose(input);
	fclose(output);
}

void decrypt(std::string name, std::string new_name) {
	int outlen, inlen;
	FILE * input, *output;
	fopen_s(&input, name.c_str(), "rb");
	fopen_s(&output, new_name.c_str(), "wb");
	unsigned char inbuf[BUFSIZE], outbuf[BUFSIZE];
	unsigned char key[32] = "1234567890098765432112345678900";
	unsigned char iv[8] = "1234567";
	EVP_CIPHER_CTX ctx;
	EVP_CIPHER_CTX_init(&ctx);
	EVP_DecryptInit(&ctx, EVP_aes_256_ofb(), key, iv);
	for (;;) {
		inlen = fread(inbuf, 1, BUFSIZE, input);
		if (inlen <= 0) break;
		EVP_DecryptUpdate(&ctx, outbuf, &outlen, inbuf, inlen);
		fwrite(outbuf, 1, outlen, output);
	}
	EVP_DecryptFinal(&ctx, outbuf, &outlen);
	fwrite(outbuf, 1, outlen, output);
	EVP_CIPHER_CTX_cleanup(&ctx);
	fclose(input);
	fclose(output);
}

void decryptfile(std::string path)
{
	decrypt(path + ".crpt", path);
	remove((path + ".crpt").c_str());
}

void hash_files(std::string dir_name)
{
	for (boost::filesystem::recursive_directory_iterator it(dir_name), end; it != end; ++it) {
		auto current_file = it->path();
		if (!is_directory(current_file))
		{
			if (current_file.extension() != ".hash") {
				getHash(current_file.string());
			}
		}
	}
}

void upload_to_disk(std::string dir_name, std::string disk_dir_name, std::unique_ptr<WebDAV::Client>& client)
{
	hash_files(dir_name);
	for (boost::filesystem::directory_iterator it(dir_name), end; it != end; ++it) {
		auto current_file = it->path();
		if (boost::filesystem::is_directory(current_file)) {
			client->create_directory(disk_dir_name + "/" + current_file.leaf().string());
			upload_to_disk(current_file.string(), disk_dir_name + "/" + current_file.leaf().string(), client);
		}
		else {
			bool flag = false;
			if (current_file.extension() != ".hash") {
				if (client->check(disk_dir_name + "/" + current_file.leaf().string() + ".hash"))
				{
					client->download(disk_dir_name + "/" + current_file.leaf().string() + ".hash", current_file.string() + ".hashd");
					flag = (hash_to_string(current_file.string() + ".hash") == hash_to_string(current_file.string() + ".hashd"));
					remove((current_file.string() + ".hashd").c_str());
				};
				if (!flag) {
					auto crypt_file = current_file.string() + ".crpt";
					encrypt(current_file.string(), crypt_file);
					client->upload(disk_dir_name + "/" + current_file.leaf().string(), crypt_file);
					remove(crypt_file.c_str());
				}
			}
			else
			{
				client->upload(disk_dir_name + "/" + current_file.leaf().string(), current_file.string());
				remove(current_file.string().c_str());
			}
		}
	}
}

void decrypt_threads(std::string dir_name)
{
	boost::asio::io_service ioService;
	boost::thread_group threadpool;
	boost::asio::io_service::work work(ioService);
	for (size_t i = 0; i < 5; i++) {
		threadpool.create_thread(boost::bind(&boost::asio::io_service::run, &ioService));
	}
	for (boost::filesystem::recursive_directory_iterator it(dir_name), end; it != end; ++it) {
		auto current_file = it->path();
		if (!is_directory(current_file))
		{
			if (current_file.extension() == ".crpt") {
				auto new_name(current_file.string().substr(0, current_file.string().find(".crpt")));
				ioService.post(boost::bind(decryptfile, new_name));
			}
		}
	}
	ioService.stop();
	threadpool.join_all();
}

void download_from_disk(std::string dir, std::string disk_dir, std::unique_ptr<WebDAV::Client> & client)
{
	boost::filesystem::create_directory(dir + "/" + disk_dir);
	auto files = client->list(disk_dir);
	for (auto i : files)
	{
		if (client->is_dir(disk_dir + i))
		{
			download_from_disk(dir, disk_dir + i, client);
		}
		else
		{
			if (i.find(".hash") == -1) {
				auto path = dir + "/" + disk_dir + "/" + i;
				client->download(disk_dir + i, path + ".crpt");
			}
		}
		client->clean(disk_dir + i);
	}
	decrypt_threads(dir);
}
