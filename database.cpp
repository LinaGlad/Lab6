#include "database.h"
#include <iomanip>
#include <iostream>
#include <locale>
#include <codecvt>
#include <vector>
#include <string>
#include <cctype>
#include <limits>

void checkConn(PGconn* conn) {
    if (PQstatus(conn) != CONNECTION_OK) {
        std::cerr << "Ошибка подключения: " << PQerrorMessage(conn) << std::endl;
        PQfinish(conn);
        exit(1);
    }
}

int getVisualLength(const std::string& s) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    std::wstring ws = conv.from_bytes(s);
    return static_cast<int>(ws.length());
}

std::string repeat(const std::string& str, int n) {
    if (n <= 0) return "";
    std::string res;
    res.reserve(str.size() * n);
    while (n--) res += str;
    return res;
}

void printCell(const std::string& content, int targetWidth) {
    int visualLen = getVisualLength(content);
    int padding = targetWidth - visualLen;
    if (padding < 0) padding = 0;
    std::cout << "| " << content << repeat(" ", padding);
}

void printResult(PGresult* res) {
    ExecStatusType status = PQresultStatus(res);

    if (status == PGRES_COMMAND_OK) {
        std::cout << "Операция выполнена успешно.\n";
        return;
    }

    if (status != PGRES_TUPLES_OK) {
        std::cerr << "Ошибка: " << PQresultErrorMessage(res) << std::endl;
        return;
    }

    int rows = PQntuples(res);
    int cols = PQnfields(res);
    std::vector<int> widths(cols, 0);

    for (int j = 0; j < cols; ++j) {
        widths[j] = getVisualLength(PQfname(res, j));
        for (int i = 0; i < rows; ++i) {
            int len = getVisualLength(PQgetvalue(res, i, j));
            if (len > widths[j]) widths[j] = len;
        }
        widths[j] += 2;
    }

    auto printDiv = [&](char l, char m, char r, char h) {
        std::cout << l;
        for (int j = 0; j < cols; ++j) {
            std::cout << repeat(std::string(1, h), widths[j] + 1);
            if (j < cols - 1) std::cout << m;
        }
        std::cout << r << "\n";
        };

    std::cout << "\n";
    printDiv('+', '+', '+', '-');

    for (int j = 0; j < cols; ++j) {
        printCell(PQfname(res, j), widths[j]);
    }
    std::cout << "|\n";

    printDiv('+', '+', '+', '-');

    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            printCell(PQgetvalue(res, i, j), widths[j]);
        }
        std::cout << "|\n";
    }
    printDiv('+', '+', '+', '-');
    std::cout << std::endl;
}

void execAndPrint(PGconn* conn, const char* query, int nParams,
    const char* const* params) {
    PGresult* res = PQexecParams(conn, query, nParams, nullptr,
        params, nullptr, nullptr, 0);
    printResult(res);
    PQclear(res);
}

bool isNumber(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

bool readerExists(PGconn* conn, const std::string& readerId) {
    const char* q =
        "SELECT 1 FROM readers WHERE reader_id = $1::int;";
    const char* params[1] = { readerId.c_str() };
    PGresult* res = PQexecParams(conn, q, 1, nullptr, params, nullptr, nullptr, 0);
    bool ok = (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0);
    PQclear(res);
    if (!ok)
        std::cout << "Читатель с таким ID не найден.\n";
    return ok;
}

bool bookExists(PGconn* conn, const std::string& bookId) {
    const char* q =
        "SELECT 1 FROM books WHERE book_id = $1::int;";
    const char* params[1] = { bookId.c_str() };
    PGresult* res = PQexecParams(conn, q, 1, nullptr, params, nullptr, nullptr, 0);
    bool ok = (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0);
    PQclear(res);
    if (!ok)
        std::cout << "Книга с таким ID не найдена.\n";
    return ok;
}

bool resolveIdByIdOrName(
    PGconn* conn,
    const std::string& tableName,
    const std::string& idColumn,
    const std::string& nameColumn,
    const std::string& userInput,
    const std::string& description,
    std::string& outId
) {
    if (userInput.empty()) {
        std::cout << "Ввод не должен быть пустым.\n";
        return false;
    }

    // Если пользователь ввёл число — считаем это ID
    if (isNumber(userInput)) {
        std::string q =
            "SELECT " + idColumn +
            " FROM " + tableName +
            " WHERE " + idColumn + " = $1::int;";

        const char* params[1] = { userInput.c_str() };
        PGresult* res = PQexecParams(conn, q.c_str(), 1, nullptr, params, nullptr, nullptr, 0);

        if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
            std::cout << "Нет " << description << "а с таким ID.\n";
            PQclear(res);
            return false;
        }

        outId = PQgetvalue(res, 0, 0);
        PQclear(res);
        return true;
    }

    // Поиск по имени (частичное совпадение)
    std::string ilikeQuery =
        "SELECT " + idColumn + ", " + nameColumn +
        " FROM " + tableName +
        " WHERE " + nameColumn + " ILIKE '%' || $1 || '%' "
        " ORDER BY " + idColumn + ";";

    const char* params[1] = { userInput.c_str() };
    PGresult* res = PQexecParams(conn, ilikeQuery.c_str(), 1, nullptr,
                                 params, nullptr, nullptr, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        std::cerr << "Ошибка поиска по " << description << ": "
                  << PQresultErrorMessage(res) << std::endl;
        PQclear(res);
        return false;
    }

    int rows = PQntuples(res);
    if (rows == 0) {
        std::cout << "Не найден ни один " << description << ".\n";
        PQclear(res);
        return false;
    }

    if (rows == 1) {
        outId = PQgetvalue(res, 0, 0);
        PQclear(res);
        return true;
    }

    std::cout << "Найдено несколько вариантов. Выберите ID:\n";
    printResult(res);
    PQclear(res);

    std::string chosenId;
    std::cout << "Введите ID: ";
    std::cin >> chosenId;  

    if (!isNumber(chosenId)) {
        std::cout << "Некорректный ID.\n";
        return false;
    }

    std::string qCheck =
        "SELECT " + idColumn +
        " FROM " + tableName +
        " WHERE " + idColumn + " = $1::int;";

    const char* paramsCheck[1] = { chosenId.c_str() };
    PGresult* resCheck = PQexecParams(conn, qCheck.c_str(), 1, nullptr,
                                      paramsCheck, nullptr, nullptr, 0);

    if (PQresultStatus(resCheck) != PGRES_TUPLES_OK || PQntuples(resCheck) == 0) {
        std::cout << "Нет " << description << "а с таким ID.\n";
        PQclear(resCheck);
        return false;
    }

    outId = PQgetvalue(resCheck, 0, 0);
    PQclear(resCheck);
    return true;
}

void listBooks(PGconn* conn) {
    const char* query =
        "SELECT b.book_id AS id, b.title, a.name AS author, g.name AS genre, "
        "       p.name AS publisher, l.name AS language, b.year, b.pages, "
        "       b.copies_total, b.copies_available, "
        "       CASE WHEN b.copies_available > 0 "
        "            THEN 'Есть в наличии' ELSE 'Нет в наличии' END AS status "
        "FROM books b "
        "JOIN authors a    ON b.author_id = a.author_id "
        "JOIN genres g     ON b.genre_id = g.genre_id "
        "JOIN publishers p ON b.publisher_id = p.publisher_id "
        "JOIN languages l  ON b.language_id = l.language_id "
        "ORDER BY b.book_id;";
    execAndPrint(conn, query, 0, nullptr);
}

void listActiveLoans(PGconn* conn) {
    const char* query =
        "SELECT l.loan_id, r.full_name, b.title, l.loan_date "
        "FROM loans l "
        "JOIN readers r ON l.reader_id = r.reader_id "
        "JOIN books b   ON l.book_id = b.book_id "
        "WHERE l.return_date IS NULL "
        "ORDER BY l.loan_id;";
    execAndPrint(conn, query, 0, nullptr);
}

void addReader(PGconn* conn) {
    std::string name, phone, email;
    std::cout << "ФИО: ";
    std::getline(std::cin, name);
    std::cout << "Телефон: ";
    std::getline(std::cin, phone);
    std::cout << "Email: ";
    std::getline(std::cin, email);

    const char* query =
        "INSERT INTO readers (full_name, phone, email) VALUES ($1, $2, $3);";

    const char* params[3] = { name.c_str(), phone.c_str(), email.c_str() };
    execAndPrint(conn, query, 3, params);
}

void addBook(PGconn* conn) {
    std::string title, authorId, genreId, publisherId, languageId, year, pages;
    std::string copies;

    std::cout << "Название: ";
    std::getline(std::cin, title);
    std::cout << "ID автора: ";
    std::getline(std::cin, authorId);
    std::cout << "ID жанра: ";
    std::getline(std::cin, genreId);
    std::cout << "ID издателя: ";
    std::getline(std::cin, publisherId);
    std::cout << "ID языка: ";
    std::getline(std::cin, languageId);
    std::cout << "Год: ";
    std::getline(std::cin, year);
    std::cout << "Страниц: ";
    std::getline(std::cin, pages);
    std::cout << "Количество экземпляров: ";
    std::getline(std::cin, copies);

    const char* selectQuery =
        "SELECT book_id FROM books "
        "WHERE title = $1 "
        "  AND author_id = $2::int "
        "  AND genre_id = $3::int "
        "  AND publisher_id = $4::int "
        "  AND language_id = $5::int "
        "  AND year = $6::int "
        "  AND pages = $7::int;";

    const char* selectParams[7] = {
        title.c_str(), authorId.c_str(), genreId.c_str(),
        publisherId.c_str(), languageId.c_str(), year.c_str(), pages.c_str()
    };

    PGresult* res = PQexecParams(conn, selectQuery, 7, nullptr,
        selectParams, nullptr, nullptr, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        std::cerr << "Ошибка при проверке существующей книги: "
            << PQresultErrorMessage(res) << std::endl;
        PQclear(res);
        return;
    }

    if (PQntuples(res) > 0) {
        std::string bookId = PQgetvalue(res, 0, 0);
        PQclear(res);

        const char* updateQuery =
            "UPDATE books "
            "SET copies_total = copies_total + $1::int, "
            "    copies_available = copies_available + $1::int "
            "WHERE book_id = $2::int;";

        const char* updateParams[2] = { copies.c_str(), bookId.c_str() };
        execAndPrint(conn, updateQuery, 2, updateParams);
    }
    else {
        PQclear(res);

        const char* insertQuery =
            "INSERT INTO books (title, author_id, genre_id, publisher_id, language_id, "
            "                    year, pages, copies_total, copies_available) "
            "VALUES ($1, $2::int, $3::int, $4::int, $5::int, $6::int, $7::int, "
            "        $8::int, $8::int);";

        const char* insertParams[8] = {
            title.c_str(), authorId.c_str(), genreId.c_str(),
            publisherId.c_str(), languageId.c_str(), year.c_str(),
            pages.c_str(), copies.c_str()
        };

        execAndPrint(conn, insertQuery, 8, insertParams);
    }
}

void loanBook(PGconn* conn) {
    std::string bookId, readerId, date;
    std::cout << "ID книги: ";
    std::getline(std::cin, bookId);
    if (!isNumber(bookId) || !bookExists(conn, bookId)) return;

    std::cout << "ID читателя: ";
    std::getline(std::cin, readerId);
    if (!isNumber(readerId) || !readerExists(conn, readerId)) return;

    std::cout << "Дата выдачи (YYYY-MM-DD): ";
    std::getline(std::cin, date);

    const char* checkQuery =
        "SELECT copies_available FROM books WHERE book_id = $1::int;";
    const char* checkParams[1] = { bookId.c_str() };
    PGresult* res = PQexecParams(conn, checkQuery, 1, nullptr,
        checkParams, nullptr, nullptr, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        std::cerr << "Ошибка при проверке книги.\n";
        PQclear(res);
        return;
    }

    int available = std::stoi(PQgetvalue(res, 0, 0));
    PQclear(res);

    if (available <= 0) {
        std::cout << "Нельзя выдать книгу: нет доступных экземпляров.\n";
        return;
    }

    const char* insertQuery =
        "INSERT INTO loans (book_id, reader_id, loan_date) "
        "VALUES ($1::int, $2::int, $3::date);";
    const char* insertParams[3] = { bookId.c_str(), readerId.c_str(), date.c_str() };
    execAndPrint(conn, insertQuery, 3, insertParams);

    const char* updateQuery =
        "UPDATE books "
        "SET copies_available = copies_available - 1 "
        "WHERE book_id = $1::int;";
    const char* updateParams[1] = { bookId.c_str() };
    execAndPrint(conn, updateQuery, 1, updateParams);
}

void returnBook(PGconn* conn) {
    std::string loanId, date;
    std::cout << "ID выдачи: ";
    std::getline(std::cin, loanId);
    if (!isNumber(loanId)) {
        std::cout << "Некорректный ID выдачи.\n";
        return;
    }

    std::cout << "Дата возврата (YYYY-MM-DD): ";
    std::getline(std::cin, date);

    const char* querySelect =
        "SELECT book_id FROM loans WHERE loan_id = $1::int AND return_date IS NULL;";
    const char* paramsSel[1] = { loanId.c_str() };
    PGresult* res = PQexecParams(conn, querySelect, 1, nullptr,
        paramsSel, nullptr, nullptr, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        std::cout << "Нет активной выдачи с таким ID (возможно, уже возвращена).\n";
        PQclear(res);
        return;
    }

    std::string bookId = PQgetvalue(res, 0, 0);
    PQclear(res);

    const char* query1 =
        "UPDATE loans SET return_date = $1::date WHERE loan_id = $2::int;";
    const char* params1[2] = { date.c_str(), loanId.c_str() };
    execAndPrint(conn, query1, 2, params1);

    const char* query2 =
        "UPDATE books "
        "SET copies_available = LEAST(copies_available + 1, copies_total) "
        "WHERE book_id = $1::int;";
    const char* params2[1] = { bookId.c_str() };
    execAndPrint(conn, query2, 1, params2);
}

void deleteBook(PGconn* conn) {
    std::string bookId, countStr;
    std::cout << "ID книги: ";
    std::getline(std::cin, bookId);
    if (!isNumber(bookId) || !bookExists(conn, bookId)) return;

    std::cout << "Сколько экземпляров удалить: ";
    std::getline(std::cin, countStr);

    int toDelete = 0;
    try {
        toDelete = std::stoi(countStr);
    }
    catch (...) {
        std::cout << "Некорректное число.\n";
        return;
    }

    if (toDelete <= 0) {
        std::cout << "Количество для удаления должно быть положительным.\n";
        return;
    }

    const char* activeQuery =
        "SELECT COUNT(*) FROM loans "
        "WHERE book_id = $1::int AND return_date IS NULL;";
    const char* activeParams[1] = { bookId.c_str() };
    PGresult* resActive = PQexecParams(conn, activeQuery, 1, nullptr,
        activeParams, nullptr, nullptr, 0);

    if (PQresultStatus(resActive) != PGRES_TUPLES_OK) {
        std::cerr << "Ошибка при проверке активных выдач: "
            << PQresultErrorMessage(resActive) << std::endl;
        PQclear(resActive);
        return;
    }

    int activeCount = std::stoi(PQgetvalue(resActive, 0, 0));
    PQclear(resActive);

    const char* infoQuery =
        "SELECT copies_total, copies_available "
        "FROM books WHERE book_id = $1::int;";
    PGresult* resInfo = PQexecParams(conn, infoQuery, 1, nullptr,
        activeParams, nullptr, nullptr, 0);

    if (PQresultStatus(resInfo) != PGRES_TUPLES_OK || PQntuples(resInfo) == 0) {
        std::cerr << "Книга с таким ID не найдена.\n";
        PQclear(resInfo);
        return;
    }

    int total = std::stoi(PQgetvalue(resInfo, 0, 0));
    int available = std::stoi(PQgetvalue(resInfo, 0, 1));
    PQclear(resInfo);

    if (toDelete > available) {
        std::cout << "Нельзя удалить больше экземпляров, чем сейчас свободно.\n";
        return;
    }

    int newTotal = total - toDelete;
    int newAvailable = available - toDelete;

    if (newTotal < activeCount) {
        std::cout << "Нельзя удалить столько экземпляров: останется меньше, чем активных выдач.\n";
        return;
    }

    if (newTotal <= 0) {
        const char* delQuery =
            "DELETE FROM books WHERE book_id = $1::int;";
        const char* delParams[1] = { bookId.c_str() };
        execAndPrint(conn, delQuery, 1, delParams);
    }
    else {
        const char* updQuery =
            "UPDATE books "
            "SET copies_total = $1::int, copies_available = $2::int "
            "WHERE book_id = $3::int;";
        std::string newTotalStr = std::to_string(newTotal);
        std::string newAvailStr = std::to_string(newAvailable);
        const char* updParams[3] = {
            newTotalStr.c_str(), newAvailStr.c_str(), bookId.c_str()
        };
        execAndPrint(conn, updQuery, 3, updParams);
    }
}

void booksByYear(PGconn* conn) {
    std::string op, year;
    std::cout << "Введите оператор (<, >, =): ";
    std::getline(std::cin, op);
    if (op != "<" && op != ">" && op != "=") {
        std::cout << "Некорректный оператор.\n";
        return;
    }

    std::cout << "Введите год: ";
    std::getline(std::cin, year);
    if (!isNumber(year)) {
        std::cout << "Год должен быть числом.\n";
        return;
    }

    std::string query =
        "SELECT b.book_id AS id, b.title, a.name AS author, g.name AS genre, "
        "       p.name AS publisher, l.name AS language, b.year, b.pages, "
        "       b.copies_total, b.copies_available, "
        "       CASE WHEN b.copies_available > 0 "
        "            THEN 'Есть в наличии' ELSE 'Нет в наличии' END AS status "
        "FROM books b "
        "JOIN authors a    ON b.author_id = a.author_id "
        "JOIN genres g     ON b.genre_id = g.genre_id "
        "JOIN publishers p ON b.publisher_id = p.publisher_id "
        "JOIN languages l  ON b.language_id = l.language_id "
        "WHERE b.year " + op + " $1::int "
        "ORDER BY b.year, b.book_id;";

    const char* params[1] = { year.c_str() };
    execAndPrint(conn, query.c_str(), 1, params);
}

void booksByPublisher(PGconn* conn) {
    std::string input;
    std::cout << "Введите издательство (ID или часть названия): ";

    std::getline(std::cin, input);

    std::string publisherId;
    if (!resolveIdByIdOrName(conn,
        "publishers",
        "publisher_id",
        "name",
        input,
        "издательство",
        publisherId)) {
        return;
    }

    const char* query =
        "SELECT b.book_id AS id, b.title, a.name AS author, g.name AS genre, "
        "       p.name AS publisher, l.name AS language, b.year, b.pages, "
        "       b.copies_total, b.copies_available, "
        "       CASE WHEN b.copies_available > 0 "
        "            THEN 'Есть в наличии' ELSE 'Нет в наличии' END AS status "
        "FROM books b "
        "JOIN authors a    ON b.author_id = a.author_id "
        "JOIN genres g     ON b.genre_id = g.genre_id "
        "JOIN publishers p ON b.publisher_id = p.publisher_id "
        "JOIN languages l  ON b.language_id = l.language_id "
        "WHERE p.publisher_id = $1::int "
        "ORDER BY b.book_id;";

    const char* params[1] = { publisherId.c_str() };
    execAndPrint(conn, query, 1, params);
}

void booksByGenre(PGconn* conn) {
    std::string input;
    std::cout << "Введите жанр (ID или часть названия): ";

    std::getline(std::cin, input);

    std::string genreId;
    if (!resolveIdByIdOrName(conn,
        "genres",
        "genre_id",
        "name",
        input,
        "жанр",
        genreId)) {
        return;
    }

    const char* query =
        "SELECT b.book_id AS id, b.title, a.name AS author, g.name AS genre, "
        "       p.name AS publisher, l.name AS language, b.year, b.pages, "
        "       b.copies_total, b.copies_available, "
        "       CASE WHEN b.copies_available > 0 "
        "            THEN 'Есть в наличии' ELSE 'Нет в наличии' END AS status "
        "FROM books b "
        "JOIN authors a    ON b.author_id = a.author_id "
        "JOIN genres g     ON b.genre_id = g.genre_id "
        "JOIN publishers p ON b.publisher_id = p.publisher_id "
        "JOIN languages l  ON b.language_id = l.language_id "
        "WHERE g.genre_id = $1::int "
        "ORDER BY b.book_id;";

    const char* params[1] = { genreId.c_str() };
    execAndPrint(conn, query, 1, params);
}

void booksByPages(PGconn* conn) {
    std::string op, pages;
    std::cout << "Введите оператор (<, >, =): ";
    std::getline(std::cin, op);
    if (op != "<" && op != ">" && op != "=") {
        std::cout << "Некорректный оператор.\n";
        return;
    }

    std::cout << "Введите количество страниц: ";
    std::getline(std::cin, pages);
    if (!isNumber(pages)) {
        std::cout << "Количество страниц должно быть числом.\n";
        return;
    }

    std::string query =
        "SELECT b.book_id AS id, b.title, a.name AS author, g.name AS genre, "
        "       p.name AS publisher, l.name AS language, b.year, b.pages, "
        "       b.copies_total, b.copies_available, "
        "       CASE WHEN b.copies_available > 0 "
        "            THEN 'Есть в наличии' ELSE 'Нет в наличии' END AS status "
        "FROM books b "
        "JOIN authors a    ON b.author_id = a.author_id "
        "JOIN genres g     ON b.genre_id = g.genre_id "
        "JOIN publishers p ON b.publisher_id = p.publisher_id "
        "JOIN languages l  ON b.language_id = l.language_id "
        "WHERE b.pages " + op + " $1::int "
        "ORDER BY b.pages, b.book_id;";

    const char* params[1] = { pages.c_str() };
    execAndPrint(conn, query.c_str(), 1, params);
}

void booksByAuthor(PGconn* conn) {
    std::string input;
    std::cout << "Введите автора (ID или часть имени): ";

    std::getline(std::cin, input);

    std::string authorId;
    if (!resolveIdByIdOrName(conn,
        "authors",
        "author_id",
        "name",
        input,
        "автор",
        authorId)) {
        return;
    }

    const char* query =
        "SELECT b.book_id AS id, b.title, a.name AS author, g.name AS genre, "
        "       p.name AS publisher, l.name AS language, b.year, b.pages, "
        "       b.copies_total, b.copies_available, "
        "       CASE WHEN b.copies_available > 0 "
        "            THEN 'Есть в наличии' ELSE 'Нет в наличии' END AS status "
        "FROM books b "
        "JOIN authors a    ON b.author_id = a.author_id "
        "JOIN genres g     ON b.genre_id = g.genre_id "
        "JOIN publishers p ON b.publisher_id = p.publisher_id "
        "JOIN languages l  ON b.language_id = l.language_id "
        "WHERE a.author_id = $1::int "
        "ORDER BY b.book_id;";

    const char* params[1] = { authorId.c_str() };
    execAndPrint(conn, query, 1, params);
}

void freeBooks(PGconn* conn) {
    const char* query =
        "SELECT b.book_id AS id, b.title, a.name AS author, g.name AS genre, "
        "       p.name AS publisher, l.name AS language, b.year, b.pages, "
        "       b.copies_total, b.copies_available, "
        "       CASE WHEN b.copies_available > 0 "
        "            THEN 'Есть в наличии' ELSE 'Нет в наличии' END AS status "
        "FROM books b "
        "JOIN authors a    ON b.author_id = a.author_id "
        "JOIN genres g     ON b.genre_id = g.genre_id "
        "JOIN publishers p ON b.publisher_id = p.publisher_id "
        "JOIN languages l  ON b.language_id = l.language_id "
        "WHERE b.copies_available > 0 "
        "ORDER BY b.book_id;";
    execAndPrint(conn, query, 0, nullptr);
}

void listReaders(PGconn* conn) {
    const char* query =
        "SELECT r.reader_id AS id, r.full_name, r.phone, r.email, "
        "       CASE WHEN EXISTS ("
        "                SELECT 1 FROM loans l "
        "                WHERE l.reader_id = r.reader_id "
        "                  AND l.return_date IS NULL"
        "            ) "
        "            THEN 'Сейчас есть книги' "
        "            ELSE 'Сейчас нет книг' "
        "       END AS status "
        "FROM readers r "
        "ORDER BY r.reader_id;";
    execAndPrint(conn, query, 0, nullptr);
}

void readerLoans(PGconn* conn) {
    std::string readerId;
    std::cout << "ID читателя: ";
    std::getline(std::cin, readerId);
    if (!isNumber(readerId) || !readerExists(conn, readerId)) return;

    const char* query =
        "SELECT l.loan_id AS loan, b.book_id AS book_id, b.title, "
        "       a.name AS author, g.name AS genre, "
        "       p.name AS publisher, l2.name AS language, "
        "       b.year, b.pages, l.loan_date "
        "FROM loans l "
        "JOIN books b      ON l.book_id = b.book_id "
        "JOIN authors a    ON b.author_id = a.author_id "
        "JOIN genres g     ON b.genre_id = g.genre_id "
        "JOIN publishers p ON b.publisher_id = p.publisher_id "
        "JOIN languages l2 ON b.language_id = l2.language_id "
        "WHERE l.reader_id = $1::int "
        "  AND l.return_date IS NULL "
        "ORDER BY l.loan_id;";

    const char* params[1] = { readerId.c_str() };
    execAndPrint(conn, query, 1, params);
}

void addAuthor(PGconn* conn) {
    std::string name, country;
    std::cout << "Имя автора: ";
    std::getline(std::cin, name);
    std::cout << "Страна (опционально): ";
    std::getline(std::cin, country);

    const char* query =
        "INSERT INTO authors (name, country) VALUES ($1, $2);";
    const char* params[2] = { name.c_str(), country.c_str() };
    execAndPrint(conn, query, 2, params);
}

void addGenre(PGconn* conn) {
    std::string name;
    std::cout << "Название жанра: ";
    std::getline(std::cin, name);

    const char* query =
        "INSERT INTO genres (name) VALUES ($1);";
    const char* params[1] = { name.c_str() };
    execAndPrint(conn, query, 1, params);
}

void addPublisher(PGconn* conn) {
    std::string name, city;
    std::cout << "Название издательства: ";
    std::getline(std::cin, name);
    std::cout << "Город: ";
    std::getline(std::cin, city);

    const char* query =
        "INSERT INTO publishers (name, city) VALUES ($1, $2);";
    const char* params[2] = { name.c_str(), city.c_str() };
    execAndPrint(conn, query, 2, params);
}

void addLanguage(PGconn* conn) {
    std::string name;
    std::cout << "Название языка: ";
    std::getline(std::cin, name);

    const char* query =
        "INSERT INTO languages (name) VALUES ($1);";
    const char* params[1] = { name.c_str() };
    execAndPrint(conn, query, 1, params);
}

void listReferenceData(PGconn* conn) {
    std::cout << "\n===== АВТОРЫ =====\n";
    const char* q1 =
        "SELECT author_id AS id, name, country "
        "FROM authors ORDER BY author_id;";
    execAndPrint(conn, q1, 0, nullptr);

    std::cout << "\n===== ЖАНРЫ =====\n";
    const char* q2 =
        "SELECT genre_id AS id, name "
        "FROM genres ORDER BY genre_id;";
    execAndPrint(conn, q2, 0, nullptr);

    std::cout << "\n===== ИЗДАТЕЛЬСТВА =====\n";
    const char* q3 =
        "SELECT publisher_id AS id, name, city "
        "FROM publishers ORDER BY publisher_id;";
    execAndPrint(conn, q3, 0, nullptr);

    std::cout << "\n===== ЯЗЫКИ =====\n";
    const char* q4 =
        "SELECT language_id AS id, name "
        "FROM languages ORDER BY language_id;";
    execAndPrint(conn, q4, 0, nullptr);
}
