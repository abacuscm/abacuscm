/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you want to add, delete, or rename functions or slots, use
** Qt Designer to update this file, preserving your code.
**
** You should not define a constructor or destructor in this file.
** Instead, write your code in functions called init() and destroy().
** These will automatically be called by the form's constructor and
** destructor.
*****************************************************************************/

#include "logger.h"
#include "misc.h"

void JudgeDecisionDialog::FileSelector_activated( const QString &key )
{
	if (key == "Select file to view...")
		FileData->setText("");
	else {
		const std::string &text = data[key];
		std::string display = "";
		for (std::string::size_type i = 0; i < text.length(); i++) {
			if (::isprint(text[i]) || ::isspace(text[i]))
				display += text[i];
			else {
				char buffer[10];
				sprintf(buffer, "\\%03o", (unsigned char) text[i]);
				log(LOG_DEBUG, "buffer: %s", buffer);
				display += buffer;
			}
		}
		FileData->setText(display);
        }
}

void JudgeDecisionDialog::deferred()
{
    done(JUDGE);
}

void JudgeDecisionDialog::correct()
{
    done(CORRECT);
}

void JudgeDecisionDialog::wrong()
{
    done(WRONG);
}


void JudgeDecisionDialog::formatError()
{
    done(FORMAT_ERROR);
}
