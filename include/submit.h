#ifndef __SUBMIT_H__
#define __SUBMIT_H__

#include "ui_submit.h"

#include <Qt/qdialog.h>

class Submit : public Ui_Submit, public QDialog {
public:
	Submit(QWidget* parent = 0) : QDialog(parent) {}
};

#endif
