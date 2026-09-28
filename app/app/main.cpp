#include <Python.h>

#include <QDebug>
#include <QDateTime>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QStandardPaths>

#include <QCommandLineParser>
#include <QStandardPaths>
#include <QMainWindow>
#include <QCoreApplication>
#include <QSurfaceFormat>
#include <QTextCodec>
#include <QStringList>
#include <QMessageBox>

#include <stdexcept>
#include <exception>

#include "app/app.h"
#include "graph/hooks/hooks.h"

#include "fab/fab.h"
#include "graph/graph.h"

////////////////////////////////////////////////////////////////////////////////
// Logging
////////////////////////////////////////////////////////////////////////////////

static QFile* g_log_file = nullptr;
static QTextStream* g_log_stream = nullptr;

static void messageHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    Q_UNUSED(ctx);
    const char* level = "DEBUG";
    switch (type)
    {
        case QtDebugMsg:    level = "DEBUG"; break;
        case QtInfoMsg:     level = "INFO";  break;
        case QtWarningMsg:  level = "WARN";  break;
        case QtCriticalMsg: level = "ERROR"; break;
        case QtFatalMsg:    level = "FATAL"; break;
    }

    const QString timestamp = QDateTime::currentDateTime().toString(
            Qt::ISODateWithMs);
    const QString line = QString("[%1] [%2] %3")
            .arg(timestamp)
            .arg(QString(level))
            .arg(msg);

    // Write to log file if open
    if (g_log_stream)
    {
        *g_log_stream << line << '\n';
        g_log_stream->flush();
    }

    // Always mirror to stderr so it shows up in Xcode / terminal
    fprintf(stderr, "%s\n", line.toUtf8().constData());

    if (type == QtFatalMsg)
        abort();
}

static void initLogging()
{
    // Standard macOS log location: ~/Library/Logs/Antimony/
    // On other platforms, log next to the app data directory.
#if defined(Q_OS_MAC)
    const QString log_dir = QDir::homePath() + "/Library/Logs/Antimony";
#else
    const QString log_dir = QStandardPaths::writableLocation(
            QStandardPaths::AppLocalDataLocation) + "/logs";
#endif
    QDir().mkpath(log_dir);
    const QString log_path = log_dir + "/antimony.log";

    g_log_file = new QFile(log_path);
    if (g_log_file->open(QIODevice::Append | QIODevice::Text))
    {
        g_log_stream = new QTextStream(g_log_file);
        // Write a separator so each run is easy to find
        *g_log_stream << "\n=== Antimony started "
                      << QDateTime::currentDateTime().toString(Qt::ISODate)
                      << " ===\n";
        g_log_stream->flush();
        qInstallMessageHandler(messageHandler);
        qInfo() << "Log file:" << log_path;
    }
    else
    {
        fprintf(stderr, "[WARN] Could not open log file: %s\n",
                log_path.toUtf8().constData());
        delete g_log_file;
        g_log_file = nullptr;
    }
}

int main(int argc, char *argv[])
{
    // Use UTF-8, ignoring any LANG settings in the environment
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));

    {   // Set the default OpenGL version to be 2.1 with sample buffers
        QSurfaceFormat format;
        format.setVersion(2, 1);
        QSurfaceFormat::setDefaultFormat(format);
    }

    // Create the Application object
    App app(argc, argv);

    // Set up file logging as early as possible so we capture everything
    initLogging();

    // Initialize various Python modules and the interpreter itself
    fab::preInit();
    Graph::preInit();
    AppHooks::preInit();
    Py_Initialize();

    // Set locale to C to make atof correctly parse floats
    setlocale(LC_NUMERIC, "C");

    {   // Modify Python's default search path to include the application's
        // directory (as this doesn't happen on Linux by default)
#if defined Q_OS_MAC
        QStringList path = QCoreApplication::applicationDirPath().split("/");
        path.removeLast();
        path << "Resources";
        fab::postInit({path.join("/").toStdString()});
#elif defined Q_OS_LINUX
        auto dir = QCoreApplication::applicationDirPath();
        std::vector<std::string> fab_paths =
            {(dir + "/sb").toStdString(),
             (dir + "/../share/antimony/").toStdString()};
        for (auto p : QStandardPaths::standardLocations(
                QStandardPaths::AppDataLocation))
        {
            fab_paths.push_back(p.toStdString());
        }
        fab::postInit(fab_paths);
#elif defined Q_OS_OPENBSD
        auto dir = QCoreApplication::applicationDirPath();
        std::vector<std::string> fab_paths =
            {(dir + "/sb").toStdString(),
             (dir + "/../share/antimony/").toStdString()};
        for (auto p : QStandardPaths::standardLocations(
                QStandardPaths::AppDataLocation))
        {
            fab_paths.push_back(p.toStdString());
        }
        fab::postInit(fab_paths);
#elif defined Q_OS_WIN32
        auto dir = QCoreApplication::applicationDirPath();
        fab::postInit({(dir + "/sb").toStdString()});
#else
#error "Unknown OS!"
#endif
    }

    {   // Install operator.or_ as a reducer for shapes
        auto op = PyImport_ImportModule("operator");
        Datum::installReducer(fab::ShapeType, PyObject_GetAttrString(op, "or_"));
        Py_DECREF(op);
    }

    {   // Check to make sure that the fab module exists
        PyObject* fab = PyImport_ImportModule("fab");
        if (!fab)
        {
            PyErr_Print();
            QMessageBox::critical(NULL, "Import error",
                    "Import Error:<br><br>"
                    "Could not find <tt>fab</tt> Python module.<br>"
                    "Antimony will now exit.");
            exit(1);
        }
        Py_DECREF(fab);
    }

    {   // Parse command-line arguments
        QCommandLineParser parser;
        parser.setApplicationDescription("CAD from a parallel universe");
        parser.addHelpOption();
        QCommandLineOption forceHeightmap("heightmap",
                "Open 3D windows in heightmap mode");
        parser.addOption(forceHeightmap);
        parser.addPositionalArgument("file", "File to open", "[file]");

        parser.process(app);

        auto args = parser.positionalArguments();
        if (args.length() > 1)
        {
            qCritical("Too many command-line arguments");
            exit(1);
        }
        else if (args.length() == 1)
        {
            app.loadFile(args[0]);
        }
    }

    app.makeDefaultWindows();

    // Run event loop, catching any unhandled C++ exceptions so they appear
    // in the log before the process terminates.
    int exit_code = 0;
    try
    {
        exit_code = app.exec();
    }
    catch (const std::exception& e)
    {
        qCritical() << "Unhandled exception:" << e.what();
        throw;
    }
    catch (...)
    {
        qCritical() << "Unhandled unknown exception";
        throw;
    }
    return exit_code;
}
