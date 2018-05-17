#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QPainter>
#include <QScrollBar>

#include "qhexedit.h"
#include <algorithm>
#include <QDebug>


// ********************************************************************** Constructor, destructor

QHexEdit::QHexEdit(QWidget *parent) : QAbstractScrollArea(parent)
{
    _addressArea = true;
    _addressWidth = 4;
    _asciiArea = true;
    _overwriteMode = true;
    _highlighting = true;
    _readOnly = false;
    _cursorPosition = 0;
    _lastEventSize = 0;
    _hexCharsInLine = 47;
    _bytesPerLine = 16;
    _editAreaIsAscii = false;
    _hexCaps = false;
    _dynamicBytesPerLine = false;

    _chunks = new Chunks(this);
    _undoStack = new UndoStack(_chunks, this);

    m_isGroupMode = false;
    m_groupBytes = 4;
    m_groupCharLen = m_groupBytes*2;
    m_separated = "-";
#ifdef Q_OS_WIN32
    setFont(QFont("Courier", 10));
#else
    setFont(QFont("Monospace", 10));
#endif
    setAddressAreaColor(this->palette().alternateBase().color());
    setHighlightingColor(QColor(0xff, 0xff, 0x99, 0xff));
    setSelectionColor(this->palette().highlight().color());
    setDisabledColor(QColor(Qt::gray));
    connect(&_cursorTimer, SIGNAL(timeout()), this, SLOT(updateCursor()));
    connect(verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(adjust()));
    connect(horizontalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(adjust()));
    connect(_undoStack, SIGNAL(indexChanged(int)), this, SLOT(dataChangedPrivate(int)));

    _cursorTimer.setInterval(500);
    _cursorTimer.start();

    setAddressWidth(4);
    setAddressArea(true);
    setAsciiArea(true);
    setOverwriteMode(true);
    setHighlighting(true);
    setReadOnly(false);

    init();

}

QHexEdit::~QHexEdit()
{
}

// ********************************************************************** Properties

void QHexEdit::setAddressArea(bool addressArea)
{
    _addressArea = addressArea;
    adjust();
    setCursorPosition(_cursorPosition);
    viewport()->update();
}

bool QHexEdit::addressArea()
{
    return _addressArea;
}

void QHexEdit::setAddressAreaColor(const QColor &color)
{
    _addressAreaColor = color;
    viewport()->update();
}

QColor QHexEdit::disabledColor()
{
    return _disabledColor;
}

void QHexEdit::setDisabledColor(const QColor &color)
{
    _disabledColor = color;
    viewport()->update();
}

QColor QHexEdit::addressAreaColor()
{
    return _addressAreaColor;
}

void QHexEdit::setAddressOffset(qint64 addressOffset)
{
    _addressOffset = addressOffset;
    adjust();
    setCursorPosition(_cursorPosition);
    viewport()->update();
}

qint64 QHexEdit::addressOffset()
{
    return _addressOffset;
}

void QHexEdit::setAddressWidth(int addressWidth)
{
    _addressWidth = addressWidth;
    adjust();
    setCursorPosition(_cursorPosition);
    viewport()->update();
}

int QHexEdit::addressWidth()
{
    qint64 size = _chunks->size();
    int n = 1;
    if (size > Q_INT64_C(0x100000000)){ n += 8; size /= Q_INT64_C(0x100000000);}
    if (size > 0x10000){ n += 4; size /= 0x10000;}
    if (size > 0x100){ n += 2; size /= 0x100;}
    if (size > 0x10){ n += 1; size /= 0x10;}

    if (n > _addressWidth)
        return n;
    else
        return _addressWidth;
}

void QHexEdit::setAsciiArea(bool asciiArea)
{
    if (!asciiArea)
        _editAreaIsAscii = false;
    _asciiArea = asciiArea;
    adjust();
    setCursorPosition(_cursorPosition);
    viewport()->update();
}

bool QHexEdit::asciiArea()
{
    return _asciiArea;
}

void QHexEdit::setBytesPerLine(int count)
{
    _bytesPerLine = count;
    if(m_isGroupMode)
    {
        //多少个字节*每个字节要俩字符+（空格数量=组数-1）
        _hexCharsInLine = count*2+count*2/m_groupCharLen-1;
    }
    else
    {
        _hexCharsInLine = count * 3 - 1;
    }
    adjust();
    setCursorPosition(_cursorPosition);
    viewport()->update();
}

int QHexEdit::bytesPerLine()
{
    return _bytesPerLine;
}

void QHexEdit::setCursorPosition(qint64 position)
{
    // 1. delete old cursor
    _blink = false;
    viewport()->update(_cursorRect);

    // 2. Check, if cursor in range?
    if (position > (_chunks->size() * 2 - 1))
        position = _chunks->size() * 2  - (_overwriteMode ? 1 : 0);

    if (position < 0)
        position = 0;

    // 3. Calc new position of cursor
    if(m_isGroupMode) //add by reddream
    {
        //XXXXXXXX  eight X plus one space
//        qint64 tmpPos = calculatePos(position);
        _bPosCurrent = position/2;
        _pxCursorY = (int)(((position / 2 - _bPosFirst) / _bytesPerLine + 1) * _pxCharHeight);//Modify by lbg 20171206:修改静态问题
        int x = (position % (2 * _bytesPerLine));
        if (_editAreaIsAscii)
        {
            _pxCursorX =(((x / m_groupCharLen ) * (m_groupCharLen+1)) + (x % m_groupCharLen)) * _pxCharWidth + _pxPosAsciiX; //x / 2 * _pxCharWidth + _pxPosAsciiX;
            _cursorPosition = position & 0xFFFFFFFFFFFFFFFE;
        } else {
            _pxCursorX = (((x / m_groupCharLen ) * (m_groupCharLen+1)) + (x % m_groupCharLen)) * _pxCharWidth + _pxPosHexX;
            _cursorPosition = position;
        }
    }
    else
    {
        _bPosCurrent = position / 2;
        _pxCursorY = (int)(((position / 2 - _bPosFirst) / _bytesPerLine + 1) * _pxCharHeight);//Modify by lbg 20171206:修改静态问题
        int x = (position % (2 * _bytesPerLine));
        if (_editAreaIsAscii)
        {
            _pxCursorX = x / 2 * _pxCharWidth + _pxPosAsciiX;
            _cursorPosition = position & 0xFFFFFFFFFFFFFFFE;
        } else {
            _pxCursorX = (((x / 2) * 3) + (x % 2)) * _pxCharWidth + _pxPosHexX;
            _cursorPosition = position;
        }
    }


    if (_overwriteMode)
        _cursorRect = QRect(_pxCursorX - horizontalScrollBar()->value(), _pxCursorY + _pxCursorWidth, _pxCharWidth, _pxCursorWidth);
    else
        _cursorRect = QRect(_pxCursorX - horizontalScrollBar()->value(), _pxCursorY - _pxCharHeight + 4, _pxCursorWidth, _pxCharHeight);

    // 4. Immediately draw new cursor
    _blink = true;
    viewport()->update(_cursorRect);
    emit currentPosChanged(_bPosCurrent);
    emit currentAddressChanged(_bPosCurrent);
}

qint64 QHexEdit::cursorPosition(QPoint pos)
{
    // Calc cursor position depending on a graphical position
    qint64 result = -1;
    int posX = pos.x() + horizontalScrollBar()->value();
    int posY = pos.y() - 3;
    if ((posX >= _pxPosHexX) && (posX < (_pxPosHexX + (1 + _hexCharsInLine) * _pxCharWidth)))
    {
        _editAreaIsAscii = false;
        int x = (posX - _pxPosHexX) / _pxCharWidth;
        if(m_isGroupMode)
        {
            qDebug()<<"GroupMode,calculate with m_groupCharLen:"<<x;
            x = (x / (m_groupCharLen+1)) * m_groupCharLen + x % (m_groupCharLen+1);
            qDebug()<<"GroupMode,calculate with m_groupCharLen:"<<x;
        }
        else
        {
            x = (x / 3) * 2 + x % 3;
        }
        int y = (posY / _pxCharHeight) * 2 * _bytesPerLine;
        result = _bPosFirst * 2 + x + y;
    }
    else
        if (_asciiArea && (posX >= _pxPosAsciiX) && (posX < (_pxPosAsciiX + (1 + _bytesPerLine) * _pxCharWidth)))
        {
            _editAreaIsAscii = true;
            int x = 2 * (posX - _pxPosAsciiX) / _pxCharWidth;
            int y = (posY / _pxCharHeight) * 2 * _bytesPerLine;
            result = _bPosFirst * 2 + x + y;
        }
    return result;
}

qint64 QHexEdit::cursorPosition()
{
    return _cursorPosition;
}

void QHexEdit::setData(const QByteArray &ba)
{
    _data.clear();
    _data.append(ba);
//    _data = ba;
    _bData.setData(_data);
    setData(_bData);
}

QByteArray QHexEdit::data()
{
    return _chunks->data(0, -1);
}

void QHexEdit::setHighlighting(bool highlighting)
{
    _highlighting = highlighting;
    viewport()->update();
}

bool QHexEdit::highlighting()
{
    return _highlighting;
}

void QHexEdit::setHighlightingColor(const QColor &color)
{
    _brushHighlighted = QBrush(color);
    _penHighlighted = QPen(viewport()->palette().color(QPalette::WindowText));
    viewport()->update();
}

QColor QHexEdit::highlightingColor()
{
    return _brushHighlighted.color();
}

void QHexEdit::setOverwriteMode(bool overwriteMode)
{
    _overwriteMode = overwriteMode;
    emit overwriteModeChanged(overwriteMode);
}

bool QHexEdit::overwriteMode()
{
    return _overwriteMode;
}

void QHexEdit::setSelectionColor(const QColor &color)
{
    _brushSelection = QBrush(color);
    _penSelection = QPen(Qt::white);
    viewport()->update();
}

void QHexEdit::setGroupSize(unsigned int groupSize)
{
    m_groupBytes = groupSize;
    m_groupCharLen = groupSize*2;
}

unsigned int QHexEdit::groupSize() const
{
    return m_groupBytes;
}

void QHexEdit::setSeparate(char split)
{
    m_separated = QString(split);
}

char QHexEdit::getSeparate() const
{
    if(m_separated.isEmpty())
        return ' ';
    else
        return m_separated.at(0).toAscii();
}

QColor QHexEdit::selectionColor()
{
    return _brushSelection.color();
}

bool QHexEdit::isReadOnly()
{
    return _readOnly;
}

void QHexEdit::setReadOnly(bool readOnly)
{
    _readOnly = readOnly;
    // refresh();//add by reddream
     viewport()->update();
}

void QHexEdit::setHexCaps(const bool isCaps)
{
    if (_hexCaps != isCaps)
    {
        _hexCaps = isCaps;
        viewport()->update();
    }
}

bool QHexEdit::hexCaps()
{
    return _hexCaps;
}

void QHexEdit::setDynamicBytesPerLine(const bool isDynamic)
{
    _dynamicBytesPerLine = isDynamic;
    resizeEvent(NULL);
}

bool QHexEdit::dynamicBytesPerLine()
{
    return _dynamicBytesPerLine;
}

// ********************************************************************** Access to data of qhexedit
bool QHexEdit::setData(QIODevice &iODevice)
{
    bool ok = _chunks->setIODevice(iODevice);
    init();
    dataChangedPrivate();
    return ok;
}

QByteArray QHexEdit::dataAt(qint64 pos, qint64 count)
{
    return _chunks->data(pos, count);
}

bool QHexEdit::write(QIODevice &iODevice, qint64 pos, qint64 count)
{
    return _chunks->write(iODevice, pos, count);
}

// ********************************************************************** Char handling
void QHexEdit::insert(qint64 index, char ch)
{
    _undoStack->insert(index, ch);
    refresh();
}

void QHexEdit::remove(qint64 index, qint64 len)
{
    _undoStack->removeAt(index, len);
    refresh();
}

void QHexEdit::replace(qint64 index, char ch)
{
    _undoStack->overwrite(index, ch);
    refresh();
}

// ********************************************************************** ByteArray handling
void QHexEdit::insert(qint64 pos, const QByteArray &ba)
{
    _undoStack->insert(pos, ba);
    refresh();
}

void QHexEdit::replace(qint64 pos, qint64 len, const QByteArray &ba)
{
    _undoStack->overwrite(pos, (int)len, ba);//Modify by lbg 20171205:修改静态问题
    refresh();
}

// ********************************************************************** Utility functions
void QHexEdit::ensureVisible()
{
    if (_cursorPosition < (_bPosFirst * 2))
        verticalScrollBar()->setValue((int)(_cursorPosition / 2 / _bytesPerLine));
    if (_cursorPosition > ((_bPosFirst + (_rowsShown - 1)*_bytesPerLine) * 2))
        verticalScrollBar()->setValue((int)(_cursorPosition / 2 / _bytesPerLine) - _rowsShown + 1);
    if (_pxCursorX < horizontalScrollBar()->value())
        horizontalScrollBar()->setValue(_pxCursorX);
    if ((_pxCursorX + _pxCharWidth) > (horizontalScrollBar()->value() + viewport()->width()))
        horizontalScrollBar()->setValue(_pxCursorX + _pxCharWidth - viewport()->width());
    viewport()->update();
}

qint64 QHexEdit::indexOf(const QByteArray &ba, qint64 from)
{
    qint64 pos = _chunks->indexOf(ba, from);
    if (pos > -1)
    {
        qint64 curPos = pos*2;
        setCursorPosition(curPos + ba.length()*2);
        resetSelection(curPos);
        setSelection(curPos + ba.length()*2);
        ensureVisible();
    }
    return pos;
}

bool QHexEdit::isModified()
{
    return _modified;
}

qint64 QHexEdit::lastIndexOf(const QByteArray &ba, qint64 from)
{
    qint64 pos = _chunks->lastIndexOf(ba, from);
    if (pos > -1)
    {
        qint64 curPos = pos*2;
        setCursorPosition(curPos - 1);
        resetSelection(curPos);
        setSelection(curPos + ba.length()*2);
        ensureVisible();
    }
    return pos;
}

void QHexEdit::redo()
{
    _undoStack->redo();
    setCursorPosition(_chunks->pos()*(_editAreaIsAscii ? 1 : 2));
    refresh();
}

QString QHexEdit::selectionToReadableString()
{
    QByteArray ba = _chunks->data(getSelectionBegin(), getSelectionEnd() - getSelectionBegin());
    return toReadable(ba);
}

void QHexEdit::setFont(const QFont &font)
{
    QWidget::setFont(font);
    _pxCharWidth = fontMetrics().width(QLatin1Char('2'));
    _pxCharHeight = fontMetrics().height();
    _pxGapAdr = _pxCharWidth / 2;
    _pxGapAdrHex = _pxCharWidth;
    _pxGapHexAscii = 2 * _pxCharWidth;
    _pxCursorWidth = _pxCharHeight / 7;
    _pxSelectionSub = _pxCharHeight / 5;
    viewport()->update();
}

QString QHexEdit::toReadableString()
{
    QByteArray ba = _chunks->data();
    return toReadable(ba);
}

QString QHexEdit::toJAMCodeString(int groupSize, QString strSplit)
{
    QByteArray ba = _chunks->data();
    // 返回大写(jiangqiang modify 2017-12-15 )
    return toJAMCode(ba,(unsigned short)groupSize,strSplit).toUpper();//Modify by lbg 20171205:修改静态问题
}

void QHexEdit::findPos(unsigned int pos, bool isFindInByte)
{
   qint64 curPos = pos*2;
   if(!isFindInByte)
       curPos = pos*4;
   setCursorPosition(curPos);
   resetSelection(curPos);
   setSelection(curPos + 2);
   ensureVisible();
}

void QHexEdit::setGroupMode(bool flagGroupMode)
{
    m_isGroupMode = flagGroupMode;
}

const bool QHexEdit::groupMode()
{
    return m_isGroupMode;
}

void QHexEdit::undo()
{
    _undoStack->undo();
    setCursorPosition(_chunks->pos()*(_editAreaIsAscii ? 1 : 2));
    refresh();
}

// ********************************************************************** Handle events
void QHexEdit::keyPressEvent(QKeyEvent *event)
{
    // Cursor movements
    if (event->matches(QKeySequence::MoveToNextChar))
    {
        qint64 pos = _cursorPosition + 1;
        if (_editAreaIsAscii)
            pos += 1;
        setCursorPosition(pos);
        resetSelection(pos);
    }
    if (event->matches(QKeySequence::MoveToPreviousChar))
    {
        qint64 pos = _cursorPosition - 1;
        if (_editAreaIsAscii)
            pos -= 1;
        setCursorPosition(pos);
        resetSelection(pos);
    }
    if (event->matches(QKeySequence::MoveToEndOfLine))
    {
        qint64 pos = _cursorPosition - (_cursorPosition % (2 * _bytesPerLine)) + (2 * _bytesPerLine) - 1;
        setCursorPosition(pos);
        resetSelection(_cursorPosition);
    }
    if (event->matches(QKeySequence::MoveToStartOfLine))
    {
        qint64 pos = _cursorPosition - (_cursorPosition % (2 * _bytesPerLine));
        setCursorPosition(pos);
        resetSelection(_cursorPosition);
    }
    if (event->matches(QKeySequence::MoveToPreviousLine))
    {
        setCursorPosition(_cursorPosition - (2 * _bytesPerLine));
        resetSelection(_cursorPosition);
    }
    if (event->matches(QKeySequence::MoveToNextLine))
    {
        setCursorPosition(_cursorPosition + (2 * _bytesPerLine));
        resetSelection(_cursorPosition);
    }
    if (event->matches(QKeySequence::MoveToNextPage))
    {
        setCursorPosition(_cursorPosition + (((_rowsShown - 1) * 2 * _bytesPerLine)));
        resetSelection(_cursorPosition);
    }
    if (event->matches(QKeySequence::MoveToPreviousPage))
    {
        setCursorPosition(_cursorPosition - (((_rowsShown - 1) * 2 * _bytesPerLine)));
        resetSelection(_cursorPosition);
    }
    if (event->matches(QKeySequence::MoveToEndOfDocument))
    {
        setCursorPosition(_chunks->size() * 2 );
        resetSelection(_cursorPosition);
    }
    if (event->matches(QKeySequence::MoveToStartOfDocument))
    {
        setCursorPosition(0);
        resetSelection(_cursorPosition);
    }

    // Select commands
    if (event->matches(QKeySequence::SelectAll))
    {
        resetSelection(0);
        setSelection(2 * _chunks->size() + 1);
    }
    if (event->matches(QKeySequence::SelectNextChar))
    {
        qint64 pos = _cursorPosition + 1;
        if (_editAreaIsAscii)
            pos += 1;
        setCursorPosition(pos);
        setSelection(pos);
    }
    if (event->matches(QKeySequence::SelectPreviousChar))
    {
        qint64 pos = _cursorPosition - 1;
        if (_editAreaIsAscii)
            pos -= 1;
        setSelection(pos);
        setCursorPosition(pos);
    }
    if (event->matches(QKeySequence::SelectEndOfLine))
    {
        qint64 pos = _cursorPosition - (_cursorPosition % (2 * _bytesPerLine)) + (2 * _bytesPerLine) - 1;
        setCursorPosition(pos);
        setSelection(pos);
    }
    if (event->matches(QKeySequence::SelectStartOfLine))
    {
        qint64 pos = _cursorPosition - (_cursorPosition % (2 * _bytesPerLine));
        setCursorPosition(pos);
        setSelection(pos);
    }
    if (event->matches(QKeySequence::SelectPreviousLine))
    {
        qint64 pos = _cursorPosition - (2 * _bytesPerLine);
        setCursorPosition(pos);
        setSelection(pos);
    }
    if (event->matches(QKeySequence::SelectNextLine))
    {
        qint64 pos = _cursorPosition + (2 * _bytesPerLine);
        setCursorPosition(pos);
        setSelection(pos);
    }
    if (event->matches(QKeySequence::SelectNextPage))
    {
        qint64 pos = _cursorPosition + (((viewport()->height() / _pxCharHeight) - 1) * 2 * _bytesPerLine);
        setCursorPosition(pos);
        setSelection(pos);
    }
    if (event->matches(QKeySequence::SelectPreviousPage))
    {
        qint64 pos = _cursorPosition - (((viewport()->height() / _pxCharHeight) - 1) * 2 * _bytesPerLine);
        setCursorPosition(pos);
        setSelection(pos);
    }
    if (event->matches(QKeySequence::SelectEndOfDocument))
    {
        qint64 pos = _chunks->size() * 2;
        setCursorPosition(pos);
        setSelection(pos);
    }
    if (event->matches(QKeySequence::SelectStartOfDocument))
    {
        qint64 pos = 0;
        setCursorPosition(pos);
        setSelection(pos);
    }

    // Edit Commands
    if (!_readOnly)
    {
        /* Cut */
        if (event->matches(QKeySequence::Cut))
        {
            QByteArray ba = _chunks->data(getSelectionBegin(), getSelectionEnd() - getSelectionBegin()).toHex();
            for (qint64 idx = 32; idx < ba.size(); idx +=33)
                ba.insert((int)idx, "\n");//Modify by lbg 20171205:修改静态问题
            QClipboard *clipboard = QApplication::clipboard();
            clipboard->setText(ba);
            if (_overwriteMode)
            {
                qint64 len = getSelectionEnd() - getSelectionBegin();
                replace(getSelectionBegin(), (int)len, QByteArray((int)len, char(0)));
            }
            else
            {
                remove(getSelectionBegin(), getSelectionEnd() - getSelectionBegin());
            }
            setCursorPosition(2 * getSelectionBegin());
            resetSelection(2 * getSelectionBegin());
        } else

        /* Paste */
        if (event->matches(QKeySequence::Paste))
        {
            QClipboard *clipboard = QApplication::clipboard();
            //Add by lbg:20171202, 修改静态测试问题
            if(clipboard == NULL)
            {
                return;
            }
            QByteArray ba = QByteArray().fromHex(clipboard->text().toLatin1());
            if (_overwriteMode)
            {
                ba = ba.left(std::min<qint64>(ba.size(), (_chunks->size() - _bPosCurrent)));
                replace(_bPosCurrent, ba.size(), ba);
            }
            else
                insert(_bPosCurrent, ba);
            setCursorPosition(_cursorPosition + 2 * ba.size());
            resetSelection(getSelectionBegin());
        } else

        /* Delete char */
        if (event->matches(QKeySequence::Delete))
        {
            if (getSelectionBegin() != getSelectionEnd())
            {
                _bPosCurrent = getSelectionBegin();
                if (_overwriteMode)
                {
                    QByteArray ba = QByteArray(getSelectionEnd() - getSelectionBegin(), char(0));
                    replace(_bPosCurrent, ba.size(), ba);
                }
                else
                {
                    remove(_bPosCurrent, getSelectionEnd() - getSelectionBegin());
                }
            }
            else
            {
                if (_overwriteMode)
                    replace(_bPosCurrent, char(0));
                else
                    remove(_bPosCurrent, 1);
            }
            setCursorPosition(2 * _bPosCurrent);
            resetSelection(2 * _bPosCurrent);
        } else

        /* Backspace */
        if ((event->key() == Qt::Key_Backspace) && (event->modifiers() == Qt::NoModifier))
        {
            if (getSelectionBegin() != getSelectionEnd())
            {
                _bPosCurrent = getSelectionBegin();
                setCursorPosition(2 * _bPosCurrent);
                if (_overwriteMode)
                {
                    QByteArray ba = QByteArray(getSelectionEnd() - getSelectionBegin(), char(0));
                    replace(_bPosCurrent, ba.size(), ba);
                }
                else
                {
                    remove(_bPosCurrent, getSelectionEnd() - getSelectionBegin());
                }
                resetSelection(2 * _bPosCurrent);
            }
            else
            {
                bool behindLastByte = false;
                if ((_cursorPosition / 2) == _chunks->size())
                    behindLastByte = true;

                _bPosCurrent -= 1;
                if (_overwriteMode)
                    replace(_bPosCurrent, char(0));
                else
                    remove(_bPosCurrent, 1);

                if (!behindLastByte)
                    _bPosCurrent -= 1;

                setCursorPosition(2 * _bPosCurrent);
                resetSelection(2 * _bPosCurrent);
            }
        } else

        /* undo */
        if (event->matches(QKeySequence::Undo))
        {
            undo();
        } else

        /* redo */
        if (event->matches(QKeySequence::Redo))
        {
            redo();
        } else

        if ((QApplication::keyboardModifiers() == Qt::NoModifier) ||
            (QApplication::keyboardModifiers() == Qt::KeypadModifier) ||
            (QApplication::keyboardModifiers() == Qt::ShiftModifier) ||
            (QApplication::keyboardModifiers() == (Qt::AltModifier | Qt::ControlModifier)) ||
            (QApplication::keyboardModifiers() == Qt::GroupSwitchModifier))
        {
            /* Hex and ascii input */
            int key;
            if (_editAreaIsAscii)
                key = (uchar)event->text()[0].toLatin1();
            else
                key = int(event->text()[0].toLower().toLatin1());

            if ((((key >= '0' && key <= '9') || (key >= 'a' && key <= 'f')) && _editAreaIsAscii == false)
                || (key >= ' ' && _editAreaIsAscii))
            {
                if (getSelectionBegin() != getSelectionEnd())
                {
                    if (_overwriteMode)
                    {
                        qint64 len = getSelectionEnd() - getSelectionBegin();
                        replace(getSelectionBegin(), (int)len, QByteArray((int)len, char(0)));
                    } else
                    {
                        remove(getSelectionBegin(), getSelectionEnd() - getSelectionBegin());
                        _bPosCurrent = getSelectionBegin();
                    }
                    setCursorPosition(2 * _bPosCurrent);
                    resetSelection(2 * _bPosCurrent);
                }

                // If insert mode, then insert a byte
                if (_overwriteMode == false)
                    if ((_cursorPosition % 2) == 0)
                        insert(_bPosCurrent, char(0));

                // Change content
                if (_chunks->size() > 0)
                {
                    char ch = key;
                    if (!_editAreaIsAscii){
                        QByteArray hexValue = _chunks->data(_bPosCurrent, 1).toHex();
                        if ((_cursorPosition % 2) == 0)
                            hexValue[0] = key;
                        else
                            hexValue[1] = key;
                        ch = QByteArray().fromHex(hexValue)[0];
                    }
                    replace(_bPosCurrent, ch);
                    if (_editAreaIsAscii)
                        setCursorPosition(_cursorPosition + 2);
                    else
                        setCursorPosition(_cursorPosition + 1);
                    resetSelection(_cursorPosition);
                }
            }
        }


    }

    /* Copy */
    if (event->matches(QKeySequence::Copy))
    {
        QByteArray ba = _chunks->data(getSelectionBegin(), getSelectionEnd() - getSelectionBegin()).toHex();
        for (qint64 idx = 32; idx < ba.size(); idx +=33)
            ba.insert((int)idx, "\n");//add by lbg 20171206:修改静态测试问题
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setText(ba);
    }

    // Switch between insert/overwrite mode
    if ((event->key() == Qt::Key_Insert) && (event->modifiers() == Qt::NoModifier))
    {
        setOverwriteMode(!overwriteMode());
        setCursorPosition(_cursorPosition);
    }

    // switch from hex to ascii edit
    if (event->key() == Qt::Key_Tab && !_editAreaIsAscii){
        _editAreaIsAscii = true;
        setCursorPosition(_cursorPosition);
    }

    // switch from ascii to hex edit
    if (event->key() == Qt::Key_Backtab  && _editAreaIsAscii){
        _editAreaIsAscii = false;
        setCursorPosition(_cursorPosition);
    }

    refresh();
}

void QHexEdit::mouseMoveEvent(QMouseEvent * event)
{
    _blink = false;
    viewport()->update();
    qint64 actPos = cursorPosition(event->pos());
    if (actPos >= 0)
    {
        setCursorPosition(actPos);
        setSelection(actPos);
    }
}

void QHexEdit::mousePressEvent(QMouseEvent * event)
{
    _blink = false;
    viewport()->update();
    qint64 cPos = cursorPosition(event->pos());
    if (cPos >= 0)
    {
        resetSelection(cPos);
        setCursorPosition(cPos);
    }
}

void QHexEdit::paintHex(QPaintEvent *event)
{
    QPainter painter(viewport());
    int pxOfsX = horizontalScrollBar()->value();

    if (event->rect() != _cursorRect)
    {
        int pxPosStartY = _pxCharHeight;

        // draw some patterns if needed
        painter.fillRect(event->rect(), viewport()->palette().color(QPalette::Base));
        if (_addressArea)
            painter.fillRect(QRect(-pxOfsX, event->rect().top(), _pxPosHexX - _pxGapAdrHex/2, height()), _addressAreaColor);
        if (_asciiArea)
        {
            int linePos = _pxPosAsciiX - (_pxGapHexAscii / 2);
            painter.setPen(Qt::gray);
            painter.drawLine(linePos - pxOfsX, event->rect().top(), linePos - pxOfsX, height());
        }

        painter.setPen(viewport()->palette().color(QPalette::WindowText));

        // paint address area
        if (_addressArea)
        {
            QString address;
            for (int row=0, pxPosY = _pxCharHeight; row <= (_dataShown.size()/_bytesPerLine); row++, pxPosY +=_pxCharHeight)
            {
                address = QString("%1").arg(_bPosFirst + row*_bytesPerLine + _addressOffset, _addrDigits, 16, QChar('0'));
                painter.drawText(_pxPosAdrX - pxOfsX, pxPosY, address);
            }
        }

        // paint hex and ascii area
        QPen colStandard = QPen(viewport()->palette().color(QPalette::WindowText));
        if(!_readOnly)
          painter.setBackgroundMode(Qt::TransparentMode);

        for (int row = 0, pxPosY = pxPosStartY; row <= _rowsShown; row++, pxPosY +=_pxCharHeight)
        {
            QByteArray hex;
            int pxPosX = _pxPosHexX  - pxOfsX;
            int pxPosAsciiX2 = _pxPosAsciiX  - pxOfsX;
            qint64 bPosLine = row * _bytesPerLine;
            for (int colIdx = 0; ((bPosLine + colIdx) < _dataShown.size() && (colIdx < _bytesPerLine)); colIdx++)
            {
                QColor c = viewport()->palette().color(QPalette::Base);
                if(_readOnly) //add by reddream
                {
                    c =  _disabledColor;   //QColor(208,208,208); //viewport()->palette().color(QPalette::Shadow);
                }
                painter.setPen(colStandard);

                qint64 posBa = _bPosFirst + bPosLine + colIdx;
                if ((getSelectionBegin() <= posBa) && (getSelectionEnd() > posBa))
                {
                    c = _brushSelection.color();
                    painter.setPen(_penSelection);
                }
                else
                {
                    if (_highlighting)
                        if (_markedShown.at((int)(posBa - _bPosFirst)))
                        {
                            c = _brushHighlighted.color();
                            painter.setPen(_penHighlighted);
                        }
                }

                // render hex value
                QRect r;
                    if (colIdx == 0)
                        r.setRect(pxPosX, pxPosY - _pxCharHeight + _pxSelectionSub, 2*_pxCharWidth, _pxCharHeight);
                    else
                        r.setRect(pxPosX - _pxCharWidth, pxPosY - _pxCharHeight + _pxSelectionSub, 3*_pxCharWidth, _pxCharHeight);
                    painter.fillRect(r, c);
                    hex = _hexDataShown.mid((int)(bPosLine + colIdx) * 2, 2);//Modify by lbg 20171205:修改静态问题
                    painter.drawText(pxPosX, pxPosY, hexCaps()?hex.toUpper():hex);
                    pxPosX += 3*_pxCharWidth;


                // render ascii value
                if (_asciiArea)
                {
                    int ch = (uchar)_dataShown.at((int)bPosLine + colIdx);//Modify by lbg 20171205:修改静态问题
                    if ( ch < 0x20 )
                        ch = '.';
                    r.setRect(pxPosAsciiX2, pxPosY - _pxCharHeight + _pxSelectionSub, _pxCharWidth, _pxCharHeight);
                    painter.fillRect(r, c);
                    painter.drawText(pxPosAsciiX2, pxPosY, QChar(ch));
                    pxPosAsciiX2 += _pxCharWidth;
                }
            }
        }
        if(!_readOnly)
            painter.setBackgroundMode(Qt::TransparentMode);
        painter.setPen(viewport()->palette().color(QPalette::WindowText));
    }

    // paint cursor
    if (_blink && !_readOnly && hasFocus())
        painter.fillRect(_cursorRect, this->palette().color(QPalette::WindowText));
    else
    {
        //modified by reddream
        QColor fillColor =  viewport()->palette().color(_readOnly?QPalette::Shadow:QPalette::Base);
        painter.fillRect(QRect(_pxCursorX - pxOfsX, _pxCursorY - _pxCharHeight, _pxCharWidth, _pxCharHeight),fillColor);
        if(_readOnly)
            painter.setPen(viewport()->palette().color(QPalette::BrightText));
        if (_editAreaIsAscii) {
            QByteArray ba = _dataShown.mid((int)(_cursorPosition - _bPosFirst) / 2, 1);//Modify by lbg 20171205:修改静态问题
            if (ba != "")
            {
                if (ba.at(0) <= ' ')
                    ba[0] = '.';
                painter.drawText(_pxCursorX - pxOfsX, _pxCursorY, ba);
            }
        } else {
            painter.drawText(_pxCursorX - pxOfsX, _pxCursorY, _hexDataShown.mid((int)(_cursorPosition - _bPosFirst), 1));//Modify by lbg 20171205:修改静态问题
        }
    }

    // emit event, if size has changed
    if (_lastEventSize != _chunks->size())
    {
        _lastEventSize = _chunks->size();
        emit currentSizeChanged(_lastEventSize);
    }
}

void QHexEdit::paintHexInGroupMode(QPaintEvent *event)
{
    QPainter painter(viewport());
    int pxOfsX = horizontalScrollBar()->value();
    if (event->rect() != _cursorRect)
    {
        int pxPosStartY = _pxCharHeight;
        int countOfChars = m_groupCharLen+1;
        // draw some patterns if needed
        painter.fillRect(event->rect(), viewport()->palette().color(QPalette::Base));
        if (_addressArea)
            painter.fillRect(QRect(-pxOfsX, event->rect().top(), _pxPosHexX - _pxGapAdrHex/2, height()), _addressAreaColor);
        if (_asciiArea)
        {
            int linePos = _pxPosAsciiX - (_pxGapHexAscii / 2);
            painter.setPen(Qt::gray);
            painter.drawLine(linePos - pxOfsX, event->rect().top(), linePos - pxOfsX, height());
        }

        painter.setPen(viewport()->palette().color(QPalette::WindowText));

        // paint address area
        if (_addressArea)
        {
            QString address;
            for (int row=0, pxPosY = _pxCharHeight; row <= (_dataShown.size()/_bytesPerLine); row++, pxPosY +=_pxCharHeight)
            {
                address = QString("%1").arg(_bPosFirst + row*_bytesPerLine + _addressOffset, _addrDigits, 16, QChar('0'));
                painter.drawText(_pxPosAdrX - pxOfsX, pxPosY, address);
            }
        }

        // paint hex and ascii area
        QPen colStandard = QPen(viewport()->palette().color(QPalette::WindowText));

        if(!_readOnly)
          painter.setBackgroundMode(Qt::TransparentMode);

        for (int row = 0, pxPosY = pxPosStartY; row <= _rowsShown; row++, pxPosY +=_pxCharHeight)
        {
            QByteArray hex;
            int pxPosX = _pxPosHexX  - pxOfsX;
//            int pxPosAsciiX2 = _pxPosAsciiX  - pxOfsX;
            qint64 bPosLine = row * _bytesPerLine;

            for (int colIdx = 0; ((bPosLine + colIdx) < _dataShown.size() && (colIdx < _bytesPerLine)); colIdx += m_groupBytes)
            {

                QColor c = viewport()->palette().color(QPalette::Base);
                if(_readOnly) //add by reddream
                {
                   c = _disabledColor; //QColor(208,208,208); //viewport()->palette().color(QPalette::Shadow);//背景
                }
                painter.setPen(colStandard);

                qint64 posBa = _bPosFirst + bPosLine + colIdx;
                if ((getSelectionBegin() <= posBa) && (getSelectionEnd() > posBa))
                {
                    c = _brushSelection.color();
                    painter.setPen(_penSelection);
                }
                else
                {
                    /*
                    if (_highlighting)
                    {
                        bool isShowHightlighted = false;
                        for(int nindex=0;nindex<m_groupBytes;nindex++)
                        {
                           int dataindex =  posBa - _bPosFirst+nindex;
                           isShowHightlighted = _markedShown.at(dataindex);
                           if(isShowHightlighted)
                               break;
                        }
                        if(isShowHightlighted)
                        {
                            c = _brushHighlighted.color();
                            painter.setPen(_penHighlighted);
                        }
                    }
                    */
                }

               QColor highliteColor =  _brushHighlighted.color();
               for(int nindex=0;nindex<m_groupBytes;nindex++)
               {
                   if((bPosLine + colIdx+nindex) * 2+2>_hexDataShown.size())
                       break;
                   int dataindex =  posBa - _bPosFirst+nindex;
                   bool isShowHightlighted = _markedShown.at(dataindex);
                   QRect r;
                   bool isEndOfOneGroupAndNotTheLastLine =
                           nindex==m_groupBytes-1
                           && colIdx+m_groupBytes<_bytesPerLine
                           && (bPosLine + colIdx+m_groupBytes) < _dataShown.size();
                   int rectWidth = 2*_pxCharWidth;//isEndOfOneGroupAndNotTheLastLine?3*_pxCharWidth:2*_pxCharWidth;
                   r.setRect(pxPosX, pxPosY - _pxCharHeight + _pxSelectionSub, rectWidth, _pxCharHeight);
                   if(isShowHightlighted) //高亮
                   {
                       painter.fillRect(r, highliteColor);
                   }
                   else
                   {
                       painter.fillRect(r, c);
                   }

                   hex = _hexDataShown.mid(((int)bPosLine + colIdx+nindex) * 2, 2);//Modify by lbg 20171205:修改静态问题
                   painter.drawText(pxPosX, pxPosY, hexCaps()?hex.toUpper():hex);
                   pxPosX+=_pxCharWidth*2;
                   if(isEndOfOneGroupAndNotTheLastLine)
                   {
                     QRect rSeparated;
                     rSeparated.setRect(pxPosX, pxPosY - _pxCharHeight + _pxSelectionSub, _pxCharWidth, _pxCharHeight);
                     painter.fillRect(rSeparated, c);
                     painter.drawText(pxPosX, pxPosY,m_separated);
                     pxPosX+=_pxCharWidth;
                   }
               }
//               pxPosX += countOfChars*_pxCharWidth;

/*
                // render hex value
                QRect r;
                if (colIdx == 0)
                {
                    r.setRect(pxPosX, pxPosY - _pxCharHeight + _pxSelectionSub, m_groupCharLen*_pxCharWidth, _pxCharHeight);
                }
                else
                {
                    r.setRect(pxPosX - _pxCharWidth, pxPosY - _pxCharHeight + _pxSelectionSub, (m_groupCharLen+1)*_pxCharWidth, _pxCharHeight);
                }
                painter.fillRect(r, c);
                hex = _hexDataShown.mid((bPosLine + colIdx) * 2, m_groupCharLen);
                painter.drawText(pxPosX, pxPosY, hexCaps()?hex.toUpper():hex);
                pxPosX += countOfChars*_pxCharWidth;

                */


                // render ascii value
//                if (_asciiArea)
//                {
//                    int ch = (uchar)_dataShown.at(bPosLine + colIdx);
//                    if ( ch < 0x20 )
//                        ch = '.';
//                    r.setRect(pxPosAsciiX2, pxPosY - _pxCharHeight + _pxSelectionSub, _pxCharWidth, _pxCharHeight);
//                    painter.fillRect(r, c);
//                    painter.drawText(pxPosAsciiX2, pxPosY, QChar(ch));
//                    pxPosAsciiX2 += _pxCharWidth;
//                }
            }
        }
        if(!_readOnly)
          painter.setBackgroundMode(Qt::TransparentMode);
        painter.setPen(viewport()->palette().color(QPalette::WindowText));
    }

    // paint cursor
    if (_blink && !_readOnly && hasFocus())
        painter.fillRect(_cursorRect, this->palette().color(QPalette::WindowText));
    else
    {
        //modified by reddream
        QColor fillColor =  viewport()->palette().color(_readOnly?QPalette::Shadow:QPalette::Base);
        painter.fillRect(QRect(_pxCursorX - pxOfsX, _pxCursorY - _pxCharHeight, _pxCharWidth, _pxCharHeight),fillColor);
        if(_readOnly)
            painter.setPen(viewport()->palette().color(QPalette::BrightText));
        if (_editAreaIsAscii) {
            QByteArray ba = _dataShown.mid(((int)(_cursorPosition - _bPosFirst) / 2), 1);//Modify by lbg 20171205:修改静态问题
            if (ba != "")
            {
                if (ba.at(0) <= ' ')
                    ba[0] = '.';
                painter.drawText(_pxCursorX - pxOfsX, _pxCursorY, hexCaps()?ba.toUpper():ba);
            }
        } else {
            QByteArray ba =  _hexDataShown.mid((int)(_cursorPosition - _bPosFirst), 1);
            painter.drawText(_pxCursorX - pxOfsX, _pxCursorY, hexCaps()?ba.toUpper():ba);//Modify by lbg 20171205:修改静态问题
        }
    }

    // emit event, if size has changed
    if (_lastEventSize != _chunks->size())
    {
        _lastEventSize = _chunks->size();
        emit currentSizeChanged(_lastEventSize);
    }
}

void QHexEdit::paintEvent(QPaintEvent *event)
{
    if(m_isGroupMode)
        paintHexInGroupMode(event);
    else
        paintHex(event);
}

void QHexEdit::resizeEvent(QResizeEvent *)
{
    if (_dynamicBytesPerLine)
    {
        if(m_isGroupMode)
        {
            // +1 because the last hex value do not have space. so it is effective one char more
            int charWidth = (viewport()->width()) / _pxCharWidth + 1;

            //一组 m_groupCharLen+1 每组 m_groupCharLen/2 个字节
            setBytesPerLine(std::max(charWidth / ((int)m_groupCharLen+1)*((int)m_groupCharLen/2),1));
        }else
        {
            int pxFixGaps = 0;
            if (_addressArea)
                pxFixGaps = addressWidth() * _pxCharWidth + _pxGapAdr;
            pxFixGaps += _pxGapAdrHex;
            if (_asciiArea)
                pxFixGaps += _pxGapHexAscii;

            // +1 because the last hex value do not have space. so it is effective one char more
            int charWidth = (viewport()->width() - pxFixGaps ) / _pxCharWidth + 1;

            // 2 hex alfa-digits 1 space 1 ascii per byte = 4; if ascii is disabled then 3
            // to prevent devision by zero use the min value 1
            setBytesPerLine(std::max(charWidth / (_asciiArea ? 4 : 3),1));
        }


    }
    adjust();
}

bool QHexEdit::focusNextPrevChild(bool next)
{
    if (_addressArea)
    {
        if ( (next && _editAreaIsAscii) || (!next && !_editAreaIsAscii ))
            return QWidget::focusNextPrevChild(next);
        else
            return false;
    }
    else
    {
        return QWidget::focusNextPrevChild(next);
    }
}

// ********************************************************************** Handle selections
void QHexEdit::resetSelection()
{
    _bSelectionBegin = _bSelectionInit;
    _bSelectionEnd = _bSelectionInit;
}

void QHexEdit::resetSelection(qint64 pos)
{
    pos = pos / 2 ;
    if (pos < 0)
        pos = 0;
    if (pos > _chunks->size())
        pos = _chunks->size();

    _bSelectionInit = pos;
    _bSelectionBegin = pos;
    _bSelectionEnd = pos;
}

void QHexEdit::setSelection(qint64 pos)
{
    pos = pos / 2;
    if (pos < 0)
        pos = 0;
    if (pos > _chunks->size())
        pos = _chunks->size();

    if (pos >= _bSelectionInit)
    {
        _bSelectionEnd = pos;
        _bSelectionBegin = _bSelectionInit;
    }
    else
    {
        _bSelectionBegin = pos;
        _bSelectionEnd = _bSelectionInit;
    }
}

int QHexEdit::getSelectionBegin()
{
    return _bSelectionBegin;
}

int QHexEdit::getSelectionEnd()
{
    return _bSelectionEnd;
}

//归正位置
//qint64 QHexEdit::calculatePos(qint64 pos)
//{
////    int charCount = pos +1;
//    int spaceCount = pos/2;  // + charCount%2-1;
//    qint64 y = pos+spaceCount;
//    qint64 newPos = y*8/9; // y-(y+1)/9;
//    return newPos;
//}

// ********************************************************************** Private utility functions
void QHexEdit::init()
{
    _undoStack->clear();
    setAddressOffset(0);
    resetSelection(0);
    setCursorPosition(0);
    verticalScrollBar()->setValue(0);
    _modified = false;
}

void QHexEdit::adjust()
{    
    // recalc Graphics
    if (_addressArea)
    {
        _addrDigits = addressWidth();
        _pxPosHexX = _pxGapAdr + _addrDigits*_pxCharWidth + _pxGapAdrHex;
    }
    else
        _pxPosHexX = _pxGapAdrHex;
    _pxPosAdrX = _pxGapAdr;
    if(m_isGroupMode)
    {
        _pxPosAsciiX = _pxPosHexX + _hexCharsInLine * _pxCharWidth + _pxGapHexAscii;
    }
    else
        _pxPosAsciiX = _pxPosHexX + _hexCharsInLine * _pxCharWidth + _pxGapHexAscii;

    // set horizontalScrollBar()
    int pxWidth = _pxPosAsciiX;
    if (_asciiArea)
        pxWidth += _bytesPerLine*_pxCharWidth;
    horizontalScrollBar()->setRange(0, pxWidth - viewport()->width());
    horizontalScrollBar()->setPageStep(viewport()->width());

    // set verticalScrollbar()
    _rowsShown = ((viewport()->height()-4)/_pxCharHeight);
    int lineCount = (int)(_chunks->size() / (qint64)_bytesPerLine) + 1;
    verticalScrollBar()->setRange(0, lineCount - _rowsShown);
    verticalScrollBar()->setPageStep(_rowsShown);
    int value = verticalScrollBar()->value();
    _bPosFirst = (qint64)value * _bytesPerLine;
    _bPosLast = _bPosFirst + (qint64)(_rowsShown * _bytesPerLine) - 1;
    if (_bPosLast >= _chunks->size())
        _bPosLast = _chunks->size() - 1;
    readBuffers();
    setCursorPosition(_cursorPosition);
}

void QHexEdit::dataChangedPrivate(int)
{
    _modified = _undoStack->index() != 0;
    adjust();
    emit dataChanged();
}

void QHexEdit::refresh()
{
    ensureVisible();
    readBuffers();
}

void QHexEdit::readBuffers()
{
//    qDebug()<<"_chunks->data starts";
    _dataShown = _chunks->data(_bPosFirst, _bPosLast - _bPosFirst + _bytesPerLine + 1, &_markedShown);
//    qDebug()<<"_chunks->data ends";
    _hexDataShown = QByteArray(_dataShown.toHex());
}

QString QHexEdit::toReadable(const QByteArray &ba)
{
    QString result;

    for (int i=0; i < ba.size(); i += 16)
    {
        QString addrStr = QString("%1").arg(_addressOffset + i, addressWidth(), 16, QChar('0'));
        QString hexStr;
        QString ascStr;
        for (int j=0; j<16; j++)
        {
            if ((i + j) < ba.size())
            {
                hexStr.append(" ").append(ba.mid(i+j, 1).toHex());
                char ch = ba[i + j];
                if ((ch < 0x20) || (ch > 0x7e))
                        ch = '.';
                ascStr.append(QChar(ch));
            }
        }
        result += addrStr + " " + QString("%1").arg(hexStr, -48) + "  " + QString("%1").arg(ascStr, -17) + "\n";
    }
    return result;
}

QString QHexEdit::toJAMCode(const QByteArray &ba,unsigned short groupSize,QString splitStr)
{
    //XXXXXXXX-XXXXXXXXX
    QString result;

    for (int i=0; i < ba.size(); i += groupSize)
    {
        QString hexStr;
        for (int j=0; j<groupSize; j++)
        {
            if ((i + j) < ba.size())
            {
                hexStr.append(ba.mid(i+j, 1).toHex());
            }
        }
        result += QString("%1").arg(hexStr)+splitStr;
    }
    if(result.endsWith(splitStr))
    {
        result.remove(result.length()-1,1);
    }
    return result;
}

void QHexEdit::updateCursor()
{
    if (_blink)
        _blink = false;
    else
        _blink = true;
    viewport()->update(_cursorRect);
}
