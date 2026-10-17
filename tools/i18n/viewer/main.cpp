#include <QApplication>
#include <QTextCodec>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // UTF-8 支持
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
    
    MainWindow w;
    w.show();
    
    return app.exec();
}
