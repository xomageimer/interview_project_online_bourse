Для успешной сборки и компиляции проекта необходимо выполнить следующие шаги:

1) git clone https://github.com/xomageimer/interview_project_online_bourse.git
2) cd interview_project_online_bourse
3) git submodule update --init --recursive

Необходимо иметь следующие фреймворки: **boost** версии не ниже 1.7, **OpenSSL** любой версии, которая поддерживается boost::asio::ssl и **Qt5** со следующими модулями: Qt5::Core, Qt5::Widgets, Qt5::Charts. Также необходимо иметь **PostgreSQL** на стороне сервера на котором будет находиться необходимая база данных!

4) Далее необходимо создать сертификат и приватный ключ для сервера, можно воспользоваться прилагающимися файлами или сгенерировать свои, для этого с помощью консоли и установленного OpenSSL фреймворка по пути "../interview_project_online_bourse/" выполним следующие команды:
    1. openssl genrsa -out server.key 2048
    2. openssl req -x509 -new -nodes -key server.key -days 20000 -out server_certificate.crt
       - в интерактивном меню необходимо ответить на вопросы
    3. openssl genrsa -out user.key 2048
    4. openssl req -new -key user.key -out user.csr
       - в интерактивном меню необходимо ответить на такие же вопросы, стоит учесть, что Common Name должен быть всегда разным
    5. openssl x509 -req -in user.csr -CA server_certificate.crt -CAkey server.key -CAcreateserial -out user.crt -days 20000
    6. openssl dhparam -out dh2048.pem 2048
   
5) Сборка CMake (стоит отметить, что тесты должны выполняться последовательно и с использованием аргументов командной строки для конфигурации подключения к базе данных)