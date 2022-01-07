#include <iostream>
#include <tgbot/tgbot.h>
#include <sstream>
#include <vector>
#include <exception>
#include <Python.h>
#include <curl/curl.h>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <string>
#include <memory>
#include <unordered_map>
#include <boost/regex.hpp>

const std::string token = "1986031662:AAE6uNucgYjFzz1QnRuETvPcrcEtc1LJ2MM";

const std::string file_template = "https://api.telegram.org/file/bot";

size_t write_data(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    FILE *stream = (FILE *)userdata;
    if (!stream)
    {
        printf("!!! No stream\n");
        return 0;
    }

    size_t written = fwrite((FILE *)ptr, size, nmemb, stream);
    return written;
}

bool download_jpeg(const char *url)
{
    FILE *file;
    CURL *curl;

    char outfilename[FILENAME_MAX] = "../in.jpg";

    curl = curl_easy_init();
    if (curl)
    {
        file = fopen(outfilename, "wb");

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);

        CURLcode response = curl_easy_perform(curl);
        if (response != CURLE_OK)
        {
            std::cout << "failed to download\n";
            return false;
        }
        curl_easy_cleanup(curl);
        fclose(file);
        return true;
    }

    return false;
}

const wchar_t *GetWC(const char *c)
{
    const size_t cSize = strlen(c) + 1;
    wchar_t *wc = new wchar_t[cSize];
    mbstowcs(wc, c, cSize);

    return wc;
}

class MyPythonApi
{
public:
    MyPythonApi()
    {
        Py_Initialize();

        sys = PyImport_ImportModule("sys");
        sys_path = PyObject_GetAttrString(sys, "path");
        folder_path = PyUnicode_FromString((const char *)"../scripts/");
        model_folder_path = PyUnicode_FromString((const char *)"../scripts/models");
        PyList_Append(sys_path, folder_path);
        PyList_Append(sys_path, model_folder_path);

        printf(Py_GetVersion());

        PyObject *apiName = PyUnicode_FromString("cmdline");

        api_module = PyImport_Import(apiName);
        if (!api_module)
        {
            PyErr_Print();
            Py_DECREF(apiName);
            throw std::runtime_error("Such module was not found");
        }
        // Словарь объектов содержащихся в модуле
        pDict = PyModule_GetDict(api_module);
        if (!pDict)
        {
            PyErr_Print();
            Py_DECREF(apiName);
            throw std::runtime_error("Unable to get dict from module");
        }
        Py_DECREF(apiName);
    }

    void call(PyObject *pylist)
    {
        call_inner_func("predict", pylist);
    }

    ~MyPythonApi()
    {
        Py_DECREF(api_module);
        Py_DECREF(tricks_module);
        Py_DECREF(ai_module);
        Py_DECREF(config_module);
        Py_DECREF(model_folder_path);
        Py_DECREF(pDict);
        Py_DECREF(sys);
        Py_DECREF(sys_path);
        Py_DECREF(folder_path);
        Py_DECREF(predict_func);

        for (auto item : funcs)
        {
            Py_DECREF(item.second);
        }

        Py_Finalize();

        fflush(stderr);
        fflush(stdout);
    }

private:
    PyObject *model_folder_path;
    PyObject *api_module;
    PyObject *ai_module;
    PyObject *tricks_module;
    PyObject *config_module;
    PyObject *pDict;
    PyObject *sys;
    PyObject *sys_path;
    PyObject *folder_path;
    PyObject *predict_func;
    std::unordered_map<std::string, PyObject *> funcs;

    void call_inner_func(const char *func_name, PyObject *pylist)
    {
        if (funcs.find(func_name) == funcs.end())
        {
            funcs[func_name] = PyDict_GetItemString(pDict, func_name);
        }
        if (!funcs[func_name])
        {
            throw std::runtime_error("Unable to get func from dict");
        }

        // Проверка pObjct на годность.
        if (!PyCallable_Check(funcs[func_name]))
        {
            throw std::runtime_error("Unable to call func");
        }
        PyObject *pVal = nullptr;
        pVal = PyObject_CallFunctionObjArgs(funcs[func_name], pylist, NULL);
        if (pVal != nullptr)
        {
            PyObject *pResultRepr = PyObject_Repr(pVal);

            // Если полученную строку не скопировать, то после очистки ресурсов Python её не будет.
            // Для начала pResultRepr нужно привести к массиву байтов.
            char *ret = strdup(PyBytes_AS_STRING(PyUnicode_AsEncodedString(pResultRepr, "utf-8", "ERROR")));

            printf(ret);

            Py_XDECREF(pResultRepr);
            PyErr_Print();
        }
        else
        {
            Py_XDECREF(pVal);
            PyErr_Print();
            return;
        }
        Py_XDECREF(pVal);
    }
};

struct Point
{
    double x, y;
    std::string hex_code;
};

enum States
{
    Processing = 0,
    Points = 1,
    Idle = 2
};

int main()
{
    const std::string photoFilePath = "../out.jpg";
    const std::string invertedPhotoFilePath = "../inverted_out.jpg";
    const std::string photoMimeType = "image/png";
    const auto points_regex = boost::regex("[0-9]+ [0-9]+ #[0-9a-fA-F]+");

    double compress = 0.89;
    int width = 0;
    int height = 0;
    TgBot::Bot bot(token);

    MyPythonApi py_api = MyPythonApi();

    PyObject *pylist = PyList_New(0);

    States state = States::Idle;

    bot.getEvents().onCommand("start", [&bot](TgBot::Message::Ptr message)
                              { bot.getApi().sendMessage(message->chat->id, "Привет! Это бот для раскрашивания черно-белых эскизов, для начала работы отправьте изображение."); });

    bot.getEvents().onCommand("stop", [&bot, &invertedPhotoFilePath, &photoFilePath, &photoMimeType, &py_api, &state, &pylist](TgBot::Message::Ptr message)
                              {
                                  state = States::Processing;
                                  bot.getApi().sendMessage(message->chat->id, "Обработка");
                                  py_api.call(pylist);
                                  int points = PyList_GET_SIZE(pylist);
                                  std::cout << "INFO : " << points;
                                  bot.getApi().sendMessage(message->chat->id, "Результат:");
                                  bot.getApi().sendPhoto(message->chat->id, TgBot::InputFile::fromFile(photoFilePath, photoMimeType));
                                  if (points > 0)
                                  {
                                      bot.getApi().sendMessage(message->chat->id, "Результат с инвертированными цветами:");
                                      bot.getApi().sendPhoto(message->chat->id, TgBot::InputFile::fromFile(invertedPhotoFilePath, photoMimeType));
                                  }
                                  bot.getApi().sendMessage(message->chat->id, "Обработка закончена, что делать дальше?");
                                  bot.getApi().sendMessage(message->chat->id, "Отправьте команду /refresh для работы с прошлым скетчем. В противном случае отправьте новый скетч");
                                  state = States::Idle;
                                  Py_XDECREF(pylist);
                                  pylist = PyList_New(0);
                              });

    bot.getEvents().onCommand("refresh", [&bot, &invertedPhotoFilePath, &photoFilePath, &photoMimeType, &py_api, &state, &pylist](TgBot::Message::Ptr message)
                              {
                                  state = States::Points;
                                  bot.getApi().sendMessage(message->chat->id,  "Работа со старым фото. Напоминаю. Эскиз может быть раскрашен по умолчанию, для этого нажмите /stop.");
                                  bot.getApi().sendMessage(message->chat->id,  "Также вы можете ввести желаемые цвета в формате: x y цвет_в_коде_HEX, где x, y - координаты пикселя ");
                                  bot.getApi().sendMessage(message->chat->id,  "Желаемая точка и область вокруг нее будут раскрашены в указанный цвет.");
                                  bot.getApi().sendMessage(message->chat->id,  "Когда закончите вводить точки нажмите /stop "); 

                              });

    bot.getEvents().onNonCommandMessage([&bot, &state, &points_regex, &pylist, &width, &height, &compress](TgBot::Message::Ptr message)
                                        {
                                            std::istringstream message_stream(message->text);
                                            int size = message->photo.size();

                                            if (state == States::Idle && size > 0)
                                            {
                                                TgBot::File::Ptr file = bot.getApi().getFile(message->photo[2].get()->fileId);
                                                std::ostringstream photo_url;
                                                photo_url << file_template << token << '/' << file->filePath;
                                                width = message->photo[2]->width;
                                                height = message->photo[2]->height;
                                                download_jpeg(photo_url.str().data());
                                                bot.getApi().sendMessage(message->chat->id, "Фото получено. Эскиз может быть раскрашен по умолчанию, для этого нажмите /stop.");
                                                bot.getApi().sendMessage(message->chat->id, "Также вы можете ввести желаемые цвета в формате: \n x y цвет_в_коде_HEX, где x, y - координаты пикселя. \n Желаемая точка и область вокруг нее будут раскрашены в указанный цвет");
                                                bot.getApi().sendMessage(message->chat->id, "Когда закончите вводить точки нажмите /stop");
                                                state = States::Points;
                                            }
                                            else if (state == States::Idle)
                                            {
                                                bot.getApi().sendMessage(message->chat->id, "Пожайлуста, пришлите скетч для обработки");
                                            }
                                            else if (state == States::Points && message->text.size() > 0)
                                            {
                                                if (boost::regex_match(message_stream.str(), points_regex))
                                                {
                                                    PyObject *point = PyList_New(0);
                                                    Point p;
                                                    message_stream >> p.x >> p.y >> p.hex_code;
                                                    if (p.x > width || p.y > height)
                                                    {
                                                        bot.getApi().sendMessage(message->chat->id, "Точка вне границ изображения");
                                                        return;
                                                    }
                                                    p.x *= compress;
                                                    p.y *= compress;
                                                    p.x /= width;
                                                    p.y /= height;

                                                    PyObject *color_code = PyUnicode_FromString(p.hex_code.data());
                                                    PyObject *px = PyFloat_FromDouble(p.x);
                                                    PyObject *py = PyFloat_FromDouble(p.y);

                                                    int operations = 0;
                                                    operations += PyList_Append(point, px);
                                                    operations += PyList_Append(point, py);
                                                    operations += PyList_Append(point, color_code);
                                                    operations += PyList_Append(pylist, point);

                                                    if (operations != 0)
                                                    {
                                                        bot.getApi().sendMessage(message->chat->id, "Debug: Error while append items to lists");
                                                        PyErr_Print();
                                                        Py_DECREF(point);
                                                        Py_DECREF(py);
                                                        Py_DECREF(px);
                                                        Py_DECREF(color_code);
                                                    }

                                                    std::cout << p.x << p.y << p.hex_code;
                                                    bot.getApi().sendMessage(message->chat->id, "Точка принята");

                                                    Py_DECREF(point);
                                                    Py_DECREF(py);
                                                    Py_DECREF(px);
                                                    Py_DECREF(color_code);
                                                }
                                                else
                                                {
                                                    bot.getApi().sendMessage(message->chat->id, "Точка не принята");
                                                }
                                            }
                                            else
                                            {
                                                bot.getApi().sendMessage(message->chat->id, "Ваше сообщение не точка");
                                            }
                                        });

    try
    {
        printf("Bot username: %s\n", bot.getApi().getMe()->username.c_str());
        TgBot::TgLongPoll longPoll(bot);
        while (true)
        {
            printf("Long poll started\n");
            longPoll.start();
        }
    }
    catch (TgBot::TgException &e)
    {
        printf("error: %s\n", e.what());
    }
    return 0;
}
