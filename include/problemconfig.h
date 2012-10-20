/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __PROBLEMCONFIG_H__
#define __PROBLEMCONFIG_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "ui_problemconfigbase.h"
#include "serverconnection.h"

#include <map>
#include <string>
#include <vector>
#include <Qt/qdialog.h>

class QWidget;
class QGridLayout;
class QLineEdit;
class QSpinBox;
class QComboBox;
class Q3ListBox;

class ProblemConfig : public Ui_ProblemConfigBase, public QDialog {
private:
	typedef std::map<std::string, QLineEdit*> StringAttrsMap;
	typedef std::map<std::string, QComboBox*> ComboAttrsMap;
	typedef std::map<std::string, QSpinBox*> IntAttrsMap;
	typedef std::map<std::string, Q3ListBox*> ListAttrsMap;

	StringAttrsMap _strings;
	ComboAttrsMap _enums;
	StringAttrsMap _files;
	IntAttrsMap _ints;
	ListAttrsMap _lists;

	bool addAttribute(QGridLayout *g, std::string attr_name, std::string type);
	QGridLayout *createCompoundGrid(std::string comp, QWidget *parent = NULL,
			std::string prefix = "");
public:
	ProblemConfig(QWidget *parent);
	~ProblemConfig();

	bool setProblemDescription(std::string descript, std::vector<std::string> existing_problems);
	bool getProblemAttributes(AttributeMap &normal, AttributeMap &files);
	std::vector<std::string> getDependencies();
};

#endif
