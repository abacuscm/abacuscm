#ifndef __PROBLEMCONFIG_H__
#define __PROBLEMCONFIG_H__

#include "ui_problemconfigbase.h"
#include "serverconnection.h"

#include <map>
#include <string>

class QWidget;
class QGridLayout;
class QLineEdit;
class QSpinBox;
class QComboBox;

class ProblemConfig : public ProblemConfigBase {
private:
	typedef std::map<std::string, QLineEdit*> StringAttrsMap;
	typedef std::map<std::string, QComboBox*> ComboAttrsMap;
	typedef std::map<std::string, QSpinBox*> IntAttrsMap;

	StringAttrsMap _strings;
	ComboAttrsMap _enums;
	StringAttrsMap _files;
	IntAttrsMap _ints;

	bool addAttribute(QGridLayout *g, std::string attr_name, std::string type);
	QGridLayout *createCompoundGrid(std::string comp, QWidget *parent = NULL,
			std::string prefix = "");
public:
	ProblemConfig(QWidget *parent);
	~ProblemConfig();

	bool setProblemDescription(std::string descript);
	bool getProblemAttributes(AttributeMap &normal, AttributeMap &files);
};

#endif
