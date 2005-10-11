#include "problemconfig.h"
#include "logger.h"

#include <qlayout.h>
#include <qframe.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qlineedit.h>
#include <qspinbox.h>
#include <qcombobox.h>
#include <qfiledialog.h>

#include <sstream>

#define KEEP_STRING "Keep current file"

ProblemConfig::ProblemConfig(QWidget *parent)
	:ProblemConfigBase(parent)
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
			QLabel * l = new QLabel(attr_name, prop_data);
			l->setAlignment(l->alignment() & ~Qt::AlignVCenter | Qt::AlignTop);
			int row = g->numRows();
			g->addWidget(l, row, 0);
			g->addMultiCellLayout(subgrid, row, row, 1, 2);
		}; return true;
	case 'F':
		{
			QFrame *frame = new QFrame(prop_data);
			QHBoxLayout *layout = new QHBoxLayout(frame);

			QLineEdit *lineedit = new QLineEdit(frame);
			lineedit->setEnabled(false);
			layout->addWidget(lineedit);

			QPushButton *pushbutton = new QPushButton("Browse", frame);
			layout->addWidget(pushbutton);

			QFileDialog *fdiag = new QFileDialog(frame);
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
				combo->insertItem(type.substr(i, epos - i));
				i = epos + 1;
			}
			control = combo;
			_enums[attr_name] = combo;
		}; break;
	case '*':
		NOT_IMPLEMENTED();
		return true;
	default:
		log(LOG_ERR, "Uknown type %c encountered in problem description parsing code", type[0]);
		return false;
	}

	int row = g->numRows();

	g->addWidget(new QLabel(attr_name, prop_data), row, 0);

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
				bcount ++;
				break;
			case ')':
			case '}':
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

bool ProblemConfig::setProblemDescription(std::string descript) {
	return createCompoundGrid(descript, prop_data) != NULL;
}

bool ProblemConfig::getProblemAttributes(AttributeMap &normal, AttributeMap &files) {
	StringAttrsMap::const_iterator s;
	ComboAttrsMap::const_iterator c;
	IntAttrsMap::const_iterator i;
	
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

	return true;
}
