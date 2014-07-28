#ifndef CONFIGS_H
#define CONFIGS_H

#include <QObject>

#ifndef Q_OS_MAC
    #define EXEC_FOLDER QString("./../la-editor/")
#else
    #define EXEC_FOLDER QString("/Users/marcellosalomao/Projects/AkademioEditor/")
#endif



#endif
