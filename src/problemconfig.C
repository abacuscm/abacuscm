/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "problemconfig.h"
#include "logger.h"

#include <Qt/qlayout.h>
#include <Qt/q3frame.h>
#include <Qt/qlabel.h>
#include <Qt/qpushbutton.h>
#include <Qt/qlineedit.h>
#include <Qt/qspinbox.h>
#include <Qt/qcombobox.h>
#include <Qt/q3filedialog.h>
#include <Qt/q3listbox.h>

#include <sstream>

#define KEEP_STRING "Keep current file"

ProblemConfig::ProblemConfig(QWidget *parent)
	:Ui_ProblemConfigBase(), QDialog(parent)
{
}

ProblemConfig::~ProblemConfig() {
}

bool ProblemConfig::addAttribute(QGridLayout *g, std::string attr_name, std::string type) {
	QWidget *control = 0;
	switch(type[0]) {
	case 'I':
		{
			QSpinBox* sb = new QSpinBox(prop_data);
			sb->setMaxValue(999);
			control = _ints[attr_name] = sb;
		}
		break;
	case 'S':
		control = _strings[attr_name] = new QLineEdit(prop_data);
		break;
	case '(':
		{
			QGridLayout *subgrid = createCompoundGrid(type.substr(1, type.length() - 2), NULL, attr_name + ".");
			if(!subgrid)
				return false;
			QLabel * l = new QLabel(QString::fromStdString(attr_name), prop_data);
			l->setAlignment((l->alignment() & ~Qt::AlignVCenter) | Qt::AlignTop);
			int row = g->numRows();
			g->addWidget(l, row, 0);
			g->addMultiCellLayout(subgrid, row, row, 1, 2);
		}; return true;
	case 'F':
		{
			Q3Frame *frame = new Q3Frame(prop_data);
			QHBoxLayout *layout = new QHBoxLayout(frame);

			QLineEdit *lineedit = new QLineEdit(frame);
			lineedit->setEnabled(false);
			layout->addWidget(lineedit);

			QPushButton *pushbutton = new QPushButton("Browse", frame);
			layout->addWidget(pushbutton);

			Q3FileDialog *fdiag = new Q3FileDialog(frame);
			connect(fdiag, SIGNAL( fileSelected ( const QString & ) ), lineedit, SLOT( setText ( const QString & ) ));
			connect(pushbutton, SIGNAL( clicked() ), fdiag, SLOT( exec() ));

			control = frame;
			_files[attr_name] = lineedit;
		}; break;
	case '{':
		{
			QComboBox *combo = new QComboBox(prop_data);
			size_t i = 1;
			while(i < type.length()) {
				size_t epos = type.find(',', i);
				if(epos == std::string::npos)
					epos = type.length() - 1;
				combo->insertItem(QString::fromStdString(type.substr(i, epos - i)));
				i = epos + 1;
			}
			control = combo;
			_enums[attr_name] = combo;
		}; break;
	case '[':
		{
			Q3ListBox *list = new Q3ListBox(prop_data);
			size_t i = 1;
			while (i < type.length()) {
				size_t epos = type.find(',', i);
				if (epos == std::string::npos)
					epos = type.length() - 1;
				list->insertItem(QString::fromStdString(type.substr(i, epos - i)));
				i = epos + 1;
			}
			list->setSelectionMode(Q3ListBox::Multi);
			control = list;
			_lists[attr_name] = list;
		}; break;
	case '*':
		NOT_IMPLEMENTED();
		return true;
	default:
		log(LOG_ERR, "Uknown type %c encountered in problem description parsing code", type[0]);
		return false;
	}

	int row = g->numRows();

	g->addWidget(new QLabel(QString::fromStdString(attr_name), prop_data), row, 0);

	QPushButton *b = new QPushButton("?", prop_data);
	b->setMaximumWidth(30);
	g->addWidget(b, row, 1);

	g->addWidget(control, row, 2);

	return true;
}

QGridLayout *ProblemConfig::createCompoundGrid(std::string desc, QWidget *parent, std::string prefix) {
	QGridLayout *grid = new QGridLayout(parent);
	size_t spos = 0;
	while(spos < desc.length()) {
		int bcount = 0;
		size_t epos = spos;
		while(epos < desc.length() && (bcount > 0 || desc[epos] != ',')) {
			switch(desc[epos]) {
			case '(':
			case '{':
			case '[':
				bcount ++;
				break;
			case ')':
			case '}':
			case ']':
				bcount --;
				break;
			}
			epos++;
		}

		std::string this_attr = desc.substr(spos, epos - spos);
		size_t space = this_attr.find(' ');
		std::string attr_name = this_attr.substr(0, space);
		std::string attr_type = this_attr.substr(space + 1);
		log(LOG_DEBUG, "Adding attribute '%s' => '%s'", attr_name.c_str(), attr_type.c_str());

		// no need to clean up - Qt will do that for us.
		if(!addAttribute(grid, prefix + attr_name, attr_type))
			return NULL;

		spos = epos + 1;
		while(spos < desc.length() && isspace(desc[spos]))
			spos++;
	}
	return grid;
}

bool ProblemConfig::setProblemDescription(std::string descript, std::vector<std::string> existing_problems) {
	std::string desc = descript;
	if (existing_problems.size() > 0) {
		desc += ", dependencies [";
		for (size_t i = 0; i < existing_problems.size(); i++) {
			if (i > 0)
				desc += ",";
			desc += existing_problems[i];
		}
		desc += "]";
	}

	return createCompoundGrid(desc, prop_data) != NULL;
}

bool ProblemConfig::getProblemAttributes(AttributeMap &normal, AttributeMap &files) {
	StringAttrsMap::const_iterator s;
	ComboAttrsMap::const_iterator c;
	IntAttrsMap::const_iterator i;
	ListAttrsMap::const_iterator l;

	for(s = _files.begin(); s != _files.end(); ++s) {
		std::string val = s->second->text().ascii();
		if(val == "" || val == KEEP_STRING)
			files[s->first] = "-";
		else
			files[s->first] = val;
	}

	for(s = _strings.begin(); s != _strings.end(); ++s)
		normal[s->first] = s->second->text().ascii();

	for(c = _enums.begin(); c != _enums.end(); ++c)
		normal[c->first] = c->second->currentText().ascii();

	for(i = _ints.begin(); i != _ints.end(); ++i)
		normal[i->first] = i->second->cleanText().ascii();

	for (l = _lists.begin(); l != _lists.end(); ++l) {
		bool got_one = false;
		std::ostringstream value;
        Q3ListBox *list = l->second;
		for (size_t j = 0; j < list->count(); j++)
			if (list->isSelected(j)) {
				if (got_one)
					value << ",";
				value << list->text(j).toStdString();
				got_one = true;
			}
		normal[l->first] = value.str();
	}

	return true;
}

std::vector<std::string> ProblemConfig::getDependencies()
{
	std::vector<std::string> deps;
	ListAttrsMap::iterator it = _lists.find("dependencies");
	if (it != _lists.end()) {
        Q3ListBox *dependencies = it->second;
		for (size_t i = 0; i < dependencies->count(); i++)
			if (dependencies->isSelected(i))
				deps.push_back(dependencies->text(i).toStdString());
	}
	return deps;
}
