/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "mainwindow.h"

#include "serverconnection.h"
#include "acmconfig.h"
#include "sigsegv.h"
#include "logger.h"
#include "guievent.h"

#include <qapplication.h>
#include <qmessagebox.h>
#include <stdlib.h>
#include <string>
#include <openssl/ssl.h>
#include <stdarg.h>

using namespace std;

int main(int argc, char** argv) {
	setup_sigsegv();

	Config &config = Config::getConfig();

	config.load(DEFAULT_CLIENT_CONFIG);
	char *home = getenv("HOME");
	if(home)
		config.load(string(home) + "/.abacus");
	config.load("abacus.conf");

	log(LOG_INFO, "abacus config loaded, preparing to start abacus.");

	ServerConnection servercon;

	QApplication application(argc, argv);
	MainWindow mainwindow;
	application.setMainWidget(&mainwindow);
	mainwindow.show();

	return application.exec();
}
