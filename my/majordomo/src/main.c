#include <pureble.h>

#include "broker.h"


md_app_params application = {
    .daemon = 0,
    .logfile = NULL,
    .pidfile = NULL,
    .transport = NULL
};


static void args_default_callback(const char* string)
{
    plog_warn("Нет обработчика: %s! Запустите программу с аргументом -h для справки", string);
}

static void args_help_callback()
{
    papp_help_show();
}

static void args_version_callback()
{
    printf( "Broker - route request to workers\n"
            "version:      broker/v0.1\n"
            "build-date:   " __DATE__ " " __TIME__ "\n"
            "project page: https://github.com/tnako/DP\n"
            );
}

static void args_daemon_callback()
{
    application.daemon = 1;
}

static void args_logfile_callback(const char* string)
{
    // ToDo: добавить проверку на валидность
    if (likely(string)) {
        application.logfile = strdup(string);
    }
}

static void args_pidfile_callback(const char* string)
{
    // ToDo: добавить проверку на валидность
    if (likely(string)) {
        application.pidfile = strdup(string);
    }
}

static void args_transport_callback(const char* string)
{
    // ToDo: добавить проверку на валидность
    if (likely(string)) {
        application.transport = strdup(string);
    }
}

int main(int argc, char** argv)
{
    if (argc > 1) {
        papp_help_set_header("Broker - route request to workers\n"
                             "version:      broker/v0.1\n"
                             "build-date:   " __DATE__ " " __TIME__ "\n"
                             "project page: https://github.com/tnako/DP\n"
                             );
        papp_help_set_footer("Пример:  broker -d -l broker_error_log -H tcp://*:8050\n");

        papp_args_add("help", 'h', false, args_help_callback, "Выводит текущую справка");
        papp_args_add("version", 'v', false, args_version_callback, "Отобразить версию программы");
        papp_args_add("daemon", 'd', false, args_daemon_callback, "После запуска, программа становится демоном");
        papp_args_add("log_file", 'l', true, args_logfile_callback, "Путь до файла логирования");
        papp_args_add("pid_file", 'p', true, args_pidfile_callback, "Путь до pid-файла");
        papp_args_add("transport", 't', true, args_transport_callback, "Тип транспорта: интерфейс, адрес и порт. По умолчанию \"tcp://127.0.0.1:8050\"");
        papp_args_check(argc, argv, args_default_callback);
        papp_args_destroy();
    }


    //    if (unlikely(!transport)) {
    //        transport="tcp://127.0.0.1:8050";
    //    }

    //todure_run(daemon, error_log_file, pid_file, transport);
    papp_run(0, NULL, broker_main_loop);

    plog_dbg("Bye bye");

    return EXIT_SUCCESS;
}

