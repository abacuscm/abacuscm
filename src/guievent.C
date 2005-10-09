#include "guievent.h"

#include <qapplication.h>
#include <qevent.h>

QWidget* GUIEvent::_receiver;

GUIEvent::GUIEvent() : QCustomEvent(QEvent::User) {
}

GUIEvent::~GUIEvent() {
}

void GUIEvent::post() {
	QApplication::postEvent(_receiver, this);
}

void GUIEvent::setReceiver(QWidget* receiver) {
	_receiver = receiver;
};
