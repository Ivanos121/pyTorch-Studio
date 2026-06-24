#include <QTextBlockUserData>

class FolderBlockData : public QTextBlockUserData
{
public:
    int indentLevel = 0;        // Уровень отступа строки (количество пробелов/табов)
    bool isFoldStart = false;   // Является ли строка началом функции/класса (def/class)
    bool isFolded = false;      // Свернут ли этот блок в данный момент
};