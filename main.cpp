#include <QApplication>

#include "UI/MainWindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("HM_VISION");
    app.setOrganizationName("HMVision");

    HMVision::MainWindow window;
    window.show();

    return app.exec();
}
