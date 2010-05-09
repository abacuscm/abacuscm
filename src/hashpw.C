/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */

#include <string>
#include <sstream>
#include <iomanip>
#include <openssl/evp.h>
#include "hashpw.h"

using namespace std;

string hashpw(const string &username, const string &password)
{
	EVP_MD_CTX *ctx;
	unsigned char raw[EVP_MAX_MD_SIZE];
	unsigned int raw_size = 0;
	ostringstream ans;

	ctx = EVP_MD_CTX_create();
	if (ctx == NULL)
		goto cleanup;
	if (!EVP_DigestInit_ex(ctx, EVP_md5(), NULL))
		goto cleanup;
	if (!EVP_DigestUpdate(ctx, username.data(), username.length()))
		goto cleanup;
	if (!EVP_DigestUpdate(ctx, password.data(), password.length()))
		goto cleanup;
	if (!EVP_DigestFinal_ex(ctx, raw, &raw_size))
		goto cleanup;

	ans << setbase(16);
	for (unsigned int i = 0; i < raw_size; i++)
		ans << static_cast<unsigned int>(raw[i]);
	EVP_MD_CTX_destroy(ctx);
	return ans.str();

cleanup:
	if (ctx != NULL)
		EVP_MD_CTX_destroy(ctx);
	return "";
}
