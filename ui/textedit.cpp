#include "textedit.h"
#include <QFile>
#include <QMessageBox>
#include <QTextStream>
#include <QApplication>
#include <QFileInfo>
#include <Qsci/qscilexerlua.h>
#include <Qsci/qscilexerxml.h>
#include <Qsci/qscilexer.h>
#include <QCloseEvent>
#include <Qsci/qsciapis.h>
#include <Qsci/qscicommand.h>
#include <Qsci/qscicommandset.h>

static void keysForMac(QsciScintilla* qscintilla)
{
	QsciCommandSet* commandSet = qscintilla->standardCommands();
	QList<QsciCommand*> commands = commandSet->commands();

	for (int i = 0; i < commands.size(); ++i)
	{
		switch (commands[i]->key())
		{
		// case insensitive left/right (option+left/right)
		case Qt::Key_Left | Qt::CTRL:
			commands[i]->setKey(Qt::Key_Left | Qt::ALT);
			break;
		case Qt::Key_Right | Qt::CTRL:
			commands[i]->setKey(Qt::Key_Right | Qt::ALT);
			break;
		case Qt::Key_Left | Qt::CTRL | Qt::SHIFT:
			commands[i]->setKey(Qt::Key_Left | Qt::ALT | Qt::SHIFT);
			break;
		case Qt::Key_Right | Qt::CTRL | Qt::SHIFT:
			commands[i]->setKey(Qt::Key_Right | Qt::ALT | Qt::SHIFT);
			break;

		// start of line / end of line (command + left/right)
		case Qt::Key_Home:
			commands[i]->setAlternateKey(Qt::Key_Left | Qt::CTRL);
			break;
		case Qt::Key_End:
			commands[i]->setAlternateKey(Qt::Key_Right | Qt::CTRL);
			break;
		case Qt::Key_Home | Qt::SHIFT:
			commands[i]->setAlternateKey(Qt::Key_Left | Qt::CTRL | Qt::SHIFT);
			break;
		case Qt::Key_End | Qt::SHIFT:
			commands[i]->setAlternateKey(Qt::Key_Right | Qt::CTRL | Qt::SHIFT);
			break;

		// begin of document / end of document (command + up/down)
		case Qt::Key_Home | Qt::CTRL:
			commands[i]->setAlternateKey(Qt::Key_Up | Qt::CTRL);
			break;
		case Qt::Key_End | Qt::CTRL:
			commands[i]->setAlternateKey(Qt::Key_Down | Qt::CTRL);
			break;
		case Qt::Key_Home | Qt::CTRL | Qt::SHIFT:
			commands[i]->setAlternateKey(Qt::Key_Up | Qt::CTRL | Qt::SHIFT);
			break;
		case Qt::Key_End | Qt::CTRL | Qt::SHIFT:
			commands[i]->setAlternateKey(Qt::Key_Down | Qt::CTRL | Qt::SHIFT);
			break;

		// delete word to left (option + backspace)
		case Qt::Key_Backspace | Qt::CTRL:
			commands[i]->setKey(Qt::Key_Backspace | Qt::ALT);
			break;

		// delete line to the left (command + backspace)
		case Qt::Key_Backspace | Qt::CTRL | Qt::SHIFT:
			commands[i]->setKey(Qt::Key_Backspace | Qt::CTRL);
			break;

		// page up / page down (control + up/down)
		case Qt::Key_PageUp:
			commands[i]->setAlternateKey(Qt::Key_Up | Qt::META);
			break;
		case Qt::Key_PageDown:
			commands[i]->setAlternateKey(Qt::Key_Down | Qt::META);
			break;
		case Qt::Key_PageUp | Qt::SHIFT:
			commands[i]->setAlternateKey(Qt::Key_Up | Qt::META | Qt::SHIFT);
			break;
		case Qt::Key_PageDown | Qt::SHIFT:
			commands[i]->setAlternateKey(Qt::Key_Down | Qt::META | Qt::SHIFT);
			break;
		}
	}

	// case sensitive left right (control + left/right)
	const int SCMOD_SHIFT = QsciScintillaBase::SCMOD_SHIFT;
	const int SCMOD_META = 16;
	qscintilla->SendScintilla(	QsciScintillaBase::SCI_ASSIGNCMDKEY,
								(SCMOD_META << 16) | QsciScintillaBase::SCK_LEFT,
								QsciScintillaBase::SCI_WORDPARTLEFT);
	qscintilla->SendScintilla(	QsciScintillaBase::SCI_ASSIGNCMDKEY,
								(SCMOD_META << 16) | QsciScintillaBase::SCK_RIGHT,
								QsciScintillaBase::SCI_WORDPARTRIGHT);
	qscintilla->SendScintilla(	QsciScintillaBase::SCI_ASSIGNCMDKEY,
								((SCMOD_META | SCMOD_SHIFT) << 16) | QsciScintillaBase::SCK_LEFT,
								QsciScintillaBase::SCI_WORDPARTLEFTEXTEND);
	qscintilla->SendScintilla(	QsciScintillaBase::SCI_ASSIGNCMDKEY,
								((SCMOD_META | SCMOD_SHIFT) << 16) | QsciScintillaBase::SCK_RIGHT,
								QsciScintillaBase::SCI_WORDPARTRIGHTEXTEND);

	qscintilla->SendScintilla(	QsciScintillaBase::SCI_ASSIGNCMDKEY,
								(SCMOD_META << 16) | QsciScintillaBase::SCK_BACK,
								QsciScintillaBase::SCI_DELWORDLEFT);
}


static QsciLexer* createLexerByExtension(QString ext)
{
	ext = ext.toLower();

	QsciLexer* lexer = 0;

	if (ext == "lua")
	{
		QsciLexerLua* lexerlua = new QsciLexerLua;
		lexer = lexerlua;

		QsciAPIs* api = new QsciAPIs(lexer);
//		api->add(QString("addEventListener(type, listener, [data]) Registers a listener function and an optional data value"));
		api->load("Resources/gideros_annot.api");
		api->prepare();
		lexer->setAPIs(api);

		lexerlua->setFoldCompact(false); // this function does not exists in QsciLexer

		lexerlua->setColor(Qt::blue, QsciLexerLua::Keyword);
		lexerlua->setColor(QColor(0xff, 0x80, 0x00), QsciLexerLua::Number);
		// to be filled
	}
	else if (ext == "xml")
	{
		lexer = new QsciLexerXML;
	}

	if (lexer)
	{
#ifdef Q_OS_MAC
		lexer->setFont(QFont("Monaco", 12));
#else
		lexer->setFont(QFont("Courier New", 10));
#endif
		lexer->setPaper(QColor(255, 255, 255));
	}

	return lexer;
}

TextEdit::TextEdit(QWidget* parent)
	: MdiSubWindow(parent)
{
	setAttribute(Qt::WA_DeleteOnClose);

	sciScintilla_ = new QsciScintilla(this);
	setWidget(sciScintilla_);

#ifdef Q_OS_MAC
	keysForMac(sciScintilla_);
#endif

	isUntitled_ = true;

// settings
#ifdef Q_OS_MAC
	sciScintilla_->setFont(QFont("Monaco", 12));
#else
	sciScintilla_->setFont(QFont("Courier New", 10));
#endif

	sciScintilla_->setFolding(QsciScintilla::BoxedTreeFoldStyle, 3);
	sciScintilla_->setAutoIndent(true);
	sciScintilla_->setTabWidth(4);
	sciScintilla_->setIndentationsUseTabs(true);
	sciScintilla_->setIndentationGuides(true);

	sciScintilla_->setMarginLineNumbers(1, true);
	sciScintilla_->setMarginWidth(1, QString("10000"));

	sciScintilla_->setBraceMatching(QsciScintilla::SloppyBraceMatch);

	sciScintilla_->setUtf8(true);

	sciScintilla_->setCaretLineVisible(true);
	sciScintilla_->setCaretLineBackgroundColor(QColor(232, 232, 255));

	sciScintilla_->setMatchedBraceBackgroundColor(QColor(232, 232, 255));
	sciScintilla_->setUnmatchedBraceBackgroundColor(QColor(232, 232, 255));

	connect(sciScintilla_, SIGNAL(modificationChanged(bool)), this, SLOT(onModificationChanged(bool)));
	connect(sciScintilla_, SIGNAL(copyAvailable(bool)), this, SIGNAL(copyAvailable(bool)));
	connect(sciScintilla_, SIGNAL(textChanged()), this, SIGNAL(textChanged()));

	sciScintilla_->setMarginMarkerMask(1, 0);		// we dont want any markers at line number margin

	sciScintilla_->setMarginWidth(2, 14);			// margin 2 is bookmark margin
	sciScintilla_->markerDefine(QsciScintilla::RightTriangle, 1);
	sciScintilla_->setMarginMarkerMask(2, 1 << 1);
	sciScintilla_->setMarkerForegroundColor(QColor(255, 150, 30), 1);
	sciScintilla_->setMarkerBackgroundColor(QColor(255, 200, 80), 1);

	sciScintilla_->setEolMode(QsciScintilla::EolUnix);

	sciScintilla_->setAutoCompletionThreshold(2);
	sciScintilla_->setAutoCompletionSource(QsciScintilla::AcsAll);
}

TextEdit::~TextEdit()
{

}

void TextEdit::newFile()
{
	static int sequenceNumber = 1;
	isUntitled_ = true;
	fileName_ = tr("new %1").arg(sequenceNumber++);
	setWindowTitle(fileName_ + "[*]");
}

bool TextEdit::loadFile(const QString& fileName, bool suppressErrors/* = false*/)
{
	QFile file(fileName);
	if (file.open(QFile::ReadOnly | QFile::Text) == false)
	{
		if (suppressErrors == false)
		{
			QMessageBox::warning(0, tr("Warning"),
				tr("Cannot read file %1:\n%2.")
				.arg(fileName)
				.arg(file.errorString()));
		}
		return false;
	}

	isUntitled_ = true;
	fileName_ = fileName;

	QFileInfo fileInfo(fileName_);

	QsciLexer* lexer = createLexerByExtension(fileInfo.suffix());
	sciScintilla_->setLexer(lexer);

	setWindowTitle(fileInfo.fileName() + "[*]");

	QTextStream in(&file);
	in.setCodec("UTF-8");
	QApplication::setOverrideCursor(Qt::WaitCursor);
	sciScintilla_->setUtf8(true);
	sciScintilla_->setText(in.readAll());
	QApplication::restoreOverrideCursor();

	sciScintilla_->setModified(false);
	setWindowModified(false);

	return true;
}

const QString& TextEdit::fileName() const
{
	return fileName_;
}

void TextEdit::onModificationChanged(bool m)
{
	setWindowModified(m);
}

bool TextEdit::hasSelectedText() const
{
	return sciScintilla_->hasSelectedText();
}

bool TextEdit::isRedoAvailable() const
{
	return sciScintilla_->isRedoAvailable();
}

bool TextEdit::isUndoAvailable() const
{
	return sciScintilla_->isUndoAvailable();
}

bool TextEdit::save()
{
	if (sciScintilla_->isModified() == false)
		return false;

	QFile file(fileName_);
	if (!file.open(QFile::WriteOnly | QFile::Text))
	{
		QMessageBox::warning(0, tr("Warning"),
			tr("Cannot write file %1:\n%2.")
			.arg(fileName_)
			.arg(file.errorString()));
		return false;
	}

	QTextStream out(&file);
	out.setCodec("UTF-8");
	QApplication::setOverrideCursor(Qt::WaitCursor);
	out << sciScintilla_->text();
	QApplication::restoreOverrideCursor();

	sciScintilla_->setModified(false);
	setWindowModified(false);

	return true;
}

void TextEdit::setCursorPosition(int line, int index)
{
	sciScintilla_->setCursorPosition(line, index);
}

bool TextEdit::isModified() const
{
	return sciScintilla_->isModified();
}

bool TextEdit::findFirst(	const QString &expr, bool re, bool cs, bool wo,
							bool wrap, bool forward/* = true*/)
{

	int lineFrom, indexFrom, lineTo, indexTo;
	sciScintilla_->getSelection (&lineFrom, &indexFrom, &lineTo, &indexTo);

	if (lineFrom == -1 || indexFrom == -1 || lineTo == -1 || indexTo == -1)
	{
		sciScintilla_->getCursorPosition(&lineFrom, &indexFrom);
		lineTo = lineFrom;
		indexTo = indexFrom;
	}

	int line, index;
	if (forward == true)
	{
		line = lineTo;
		index = indexTo;
	}
	else
	{
		line = lineFrom;
		index = indexFrom;
	}

	return sciScintilla_->findFirst(expr, re, cs, wo, wrap, forward, line, index);
}

bool TextEdit::replace(	const QString &expr, const QString &replaceStr, bool re, bool cs, bool wo,
						 bool wrap)
{
	bool forward = true;

	int lineFrom, indexFrom, lineTo, indexTo;
	sciScintilla_->getSelection (&lineFrom, &indexFrom, &lineTo, &indexTo);

	if (lineFrom == -1 || indexFrom == -1 || lineTo == -1 || indexTo == -1)
	{
		// if there is no selected text, do find and return
		int line, index;
		sciScintilla_->getCursorPosition(&line, &index);
		return sciScintilla_->findFirst(expr, re, cs, wo, wrap, forward, line, index);
	}

	// if there is selected text, first do find from the *beginning* of selection
	bool found;
	
	found = sciScintilla_->findFirst(expr, re, cs, wo, wrap, forward, lineFrom, indexFrom);

	if (found == false)
		return false;

	int newlineFrom, newindexFrom, newlineTo, newindexTo;
	sciScintilla_->getSelection(&newlineFrom, &newindexFrom, &newlineTo, &newindexTo);

	// check if new selection is between old selection
	if (lineFrom <= newlineFrom && indexFrom <= newindexFrom && lineTo >= newlineTo && indexTo >= newindexTo)
	{
		// replace text and find again
		sciScintilla_->replace(replaceStr);
		sciScintilla_->getSelection(&newlineFrom, &newindexFrom, &newlineTo, &newindexTo);
		sciScintilla_->findFirst(expr, re, cs, wo, wrap, forward, newlineTo, newindexTo);
	}
	else
	{
		// if not, restore the selection and find from the *end* of selection
		sciScintilla_->setSelection(lineFrom, indexFrom, lineTo, indexTo);
		found = sciScintilla_->findFirst(expr, re, cs, wo, wrap, forward, lineTo, indexTo);
	}

	return found;
}

int TextEdit::replaceAll(const QString &expr, const QString &replaceStr, bool re, bool cs, bool wo,
				bool wrap)
{
	sciScintilla_->beginUndoAction();

	bool forward = true;

	int line, index;
	sciScintilla_->getCursorPosition(&line, &index);

	int lineFrom, indexFrom, lineTo, indexTo;
	sciScintilla_->getSelection (&lineFrom, &indexFrom, &lineTo, &indexTo);

	int replaceline, replaceindex;

	if (lineFrom == -1 || indexFrom == -1 || lineTo == -1 || indexTo == -1)
	{
		replaceline = line;
		replaceindex = index;
	}
	else
	{
		replaceline = lineFrom;		// replaceAll starts from *beginning* of selection
		replaceindex = indexFrom;
	}

	int replaced = 0;
	while (true)
	{
		bool found = sciScintilla_->findFirst(expr, re, cs, wo, wrap, forward, replaceline, replaceindex);
		if (found == false)
			break;
		
		sciScintilla_->replace(replaceStr);
		replaced++;

		int newlineFrom, newindexFrom, newlineTo, newindexTo;
		sciScintilla_->getSelection(&newlineFrom, &newindexFrom, &newlineTo, &newindexTo);

		replaceline = newlineTo;
		replaceindex = newindexTo;
	}

	// restore old selection
	sciScintilla_->setSelection(lineFrom, indexFrom, lineTo, indexTo);

	// restore old cursor position
	sciScintilla_->setCursorPosition(line, index);

	sciScintilla_->endUndoAction();

	return replaced;
}

void TextEdit::toogleBookmark()
{
	int line, index;
	sciScintilla_->getCursorPosition(&line, &index);

	if (sciScintilla_->markersAtLine(line) & (1 << 1))
		sciScintilla_->markerDelete(line, 1);
	else
		sciScintilla_->markerAdd(line, 1);
}

void TextEdit::nextBookmark()
{
	int line, index;
	sciScintilla_->getCursorPosition(&line, &index);

	int next = -1;
	
	next = sciScintilla_->markerFindNext(line + 1, (1 << 1));

	if (next < 0)
		next = sciScintilla_->markerFindNext(0, (1 << 1));

	if (next >= 0)
		sciScintilla_->setCursorPosition(next, 0);
}

void TextEdit::previousBookmark()
{
	int line, index;
	sciScintilla_->getCursorPosition(&line, &index);

	int prev = -1;
	
	if (line > 0)
		prev = sciScintilla_->markerFindPrevious(line - 1, (1 << 1));

	if (prev < 0)
		prev = sciScintilla_->markerFindPrevious(sciScintilla_->lines(), (1 << 1));

	if (prev >= 0)
		sciScintilla_->setCursorPosition(prev, 0);
}

void TextEdit::clearBookmarks()
{
	sciScintilla_->markerDeleteAll(1);
}

void TextEdit::undo()
{
	sciScintilla_->undo();
}

void TextEdit::redo()
{
	sciScintilla_->redo();
}

bool TextEdit::maybeSave()
{
	if (isModified())
	{
		QMessageBox msgBox(this);
		msgBox.setWindowTitle("Save");
		msgBox.setText(QString("Save file\"%1\" ?").arg(fileName_));
		msgBox.setIcon(QMessageBox::Question);
		msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
		msgBox.setDefaultButton(QMessageBox::Yes);
		int ret = msgBox.exec();		
		switch (ret)
		{
			case QMessageBox::Yes:
				save();
				return true;
			case QMessageBox::No:
				return true;
			case QMessageBox::Cancel:
				return false;
		}		
	}

	return true;
}

void TextEdit::closeEvent(QCloseEvent* event)
{
	if (maybeSave())
	{
		event->accept();
	} 
	else
	{
		event->ignore();
	}
}

