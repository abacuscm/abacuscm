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


void JudgeDecisionDialog::FileSelector_activated( const QString &key )
{
    if (key == "Select file to view...")
	FileData->setText("");
    else
	FileData->setText(data[key]);
}


void JudgeDecisionDialog::deferred()
{
    done(1);
}


void JudgeDecisionDialog::correct()
{
    done(2);
}


void JudgeDecisionDialog::wrong()
{
    done(3);
}
