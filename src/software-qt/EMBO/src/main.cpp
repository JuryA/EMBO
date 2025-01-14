/*
 * CTU/EMBO - EMBedded Oscilloscope <github.com/parezj/EMBO>
 * Author: Jakub Parez <parez.jakub@gmail.com>
 */

#include "window__main.h"
#include "settings.h"

#include <QApplication>
#include <QStyleFactory>


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    qInfo() << QStyleFactory::keys();
    //QApplication::setStyle(QStyleFactory::create("windowsvista"));

    //qApp->setStyle(QStyleFactory::create("Fusion"));

    /*
    QPalette dark;
    dark.setColor(QPalette::Window,          QColor( 37,  37,  37));
    dark.setColor(QPalette::WindowText,      QColor(212, 212, 212));
    dark.setColor(QPalette::Base,            QColor( 60,  60,  60));
    dark.setColor(QPalette::AlternateBase,   QColor( 45,  45,  45));
    dark.setColor(QPalette::PlaceholderText, QColor(127, 127, 127));
    dark.setColor(QPalette::Text,            QColor(212, 212, 212));
    dark.setColor(QPalette::Button,          QColor( 45,  45,  45));
    dark.setColor(QPalette::ButtonText,      QColor(212, 212, 212));
    dark.setColor(QPalette::BrightText,      QColor(240, 240, 240));
    dark.setColor(QPalette::Highlight,       QColor( 38,  79, 120));
    dark.setColor(QPalette::HighlightedText, QColor(240, 240, 240));
    dark.setColor(QPalette::Light,           QColor( 60,  60,  60));
    dark.setColor(QPalette::Midlight,        QColor( 52,  52,  52));
    dark.setColor(QPalette::Dark,            QColor( 30,  30,  30) );
    dark.setColor(QPalette::Mid,             QColor( 37,  37,  37));
    dark.setColor(QPalette::Shadow,          QColor( 0,    0,   0));
    dark.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
    */

    QPalette light;
    light.setColor(QPalette::Window,          QColor("#f0f0f0"));
    light.setColor(QPalette::WindowText,      QColor("#000000"));
    light.setColor(QPalette::Base,            QColor("#ffffff"));
    light.setColor(QPalette::AlternateBase,   QColor("#f6f6f6"));
    light.setColor(QPalette::PlaceholderText, QColor("#000000"));
    light.setColor(QPalette::Text,            QColor("#000000"));
    light.setColor(QPalette::Button,          QColor("#f0f0f0"));
    light.setColor(QPalette::ButtonText,      QColor("#000000"));
    light.setColor(QPalette::BrightText,      QColor("f0f0f0"));
    light.setColor(QPalette::Highlight,       QColor("#3399ff"));
    light.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    light.setColor(QPalette::Light,           QColor("#ffffff"));
    light.setColor(QPalette::Midlight,        QColor("#e3e3e3"));
    light.setColor(QPalette::Dark,            QColor("#a0a0a0"));
    light.setColor(QPalette::Mid,             QColor("#a0a0a0"));
    light.setColor(QPalette::Shadow,          QColor("#696969"));
    light.setColor(QPalette::Disabled, QPalette::Text, QColor("#787878"));

    qApp->setPalette(light);

    Settings settings;
    WindowMain w;
    w.show();
    return a.exec();
}
