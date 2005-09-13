#ifndef __PROBLEMCONFIG_H__
#define __PROBLEMCONFIG_H__

#include "ui_problemconfigbase.h"

class QWidget;
class QGridLayout;

class ProblemConfig : public ProblemConfigBase {
private:
	bool addAttribute(QGridLayout *g, std::string attr_name, std::string type);
	QGridLayout *createCompoundGrid(std::string comp, QWidget *parent = NULL);
public:
	ProblemConfig(QWidget *parent);
	~ProblemConfig();

	bool setProblemDescription(std::string descript);
};

#endif
