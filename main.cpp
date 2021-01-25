#include "widget.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    qSetMessagePattern("[%{time h:mm:ss zzz} %{if-debug}Debug%{endif}%{if-warning}Waring%{endif}%{if-critical}Critical%{endif}%{if-fatal}Fatal%{endif}] %{file}:%{line} : %{message}");

    QApplication a(argc, argv);
    Widget w;
    w.show();

    return a.exec();
}
