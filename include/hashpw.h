/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */

#ifndef __HASHPW_H__
#define __HASHPW_H__

#include <string>

std::string hashpw(const std::string &username, const std::string &password);

#endif /* !__HASHPW_H__ */
